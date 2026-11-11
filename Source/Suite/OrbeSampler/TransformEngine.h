// Moteur d'Orbe Sample : transforme une zone de l'échantillon en une boucle rendue hors-ligne (thread de travail).
// Chaîne portée de sample.html : reverse -> hauteur/étirement -> échantillonnage -> bits -> pleurage -> filtres -> réverbe -> craquements -> arrêt de bande.
#pragma once
#include "SamplerDsp.h"

namespace osamp {

struct TParams {
    int mode = 0;              // 0 = liés (vinyle), 1 = indépendants (WSOLA)
    float semi = 0, speed = 100;  // demi-tons ; vitesse en % (mode indépendant)
    bool sync = false;         // mode indépendant : la boucle suit le tempo de l'hôte
    double hostBpm = 0;
    float cut = 1, bits = 16, rate = 1, crackle = 0, wow = 0, verb = 0;
    bool rev = false, tape = false, loop = true;
    int chop = 8;              // 0 aucune, 4, 8, 16, -1 aux attaques
    float za = 0, zb = 1;      // zone (0..1 de la source)
    bool tp = false;           // étirement à transitoires préservés (stretchTP) au lieu du WSOLA d'origine
};

struct Rendered {
    std::shared_ptr<const Sample> pcm;
    int nsl = 0;
    int a[16] = {}, b[16] = {};
    double bpm = 0;            // tempo de la boucle rendue (0 = inconnu)
    int pc = -1; bool minor = false;
    double T = 1;              // facteur de vitesse appliqué
    mutable std::uint64_t gen = 0;   // génération attribuée par Swap::publish (durée de vie, voir Swap.h)
};

inline double rateHz(float v) { return std::round(4000.0 * std::pow(44100.0 / 4000.0, (double)v)); }
inline double cutHz(float v) { return 200.0 * std::pow(100.0, (double)v); }

// Découpe d'un son rendu : tranches égales ou aux attaques.
inline void chopRendered(Rendered& r, int chop) {
    const int n = r.pcm->size();
    std::vector<int> st;
    if (chop == -1) {
        const auto t = onsets(mono(*r.pcm), r.pcm->sr, 0.55f, 16);
        for (double s : t) st.push_back((int)(s * r.pcm->sr));
        if (st.empty() || st[0] > (int)(0.02 * r.pcm->sr)) st.insert(st.begin(), 0);
    } else if (chop > 0) for (int i = 0; i < chop; ++i) st.push_back((int)((double)n * i / chop));
    r.nsl = std::min(16, (int)st.size());
    for (int i = 0; i < r.nsl; ++i) { r.a[i] = st[(size_t)i]; r.b[i] = i + 1 < (int)st.size() ? st[(size_t)i + 1] : n; }
}

class Transformer {
public:
    // Retourne nullptr si annulé.
    std::shared_ptr<Rendered> render(const std::shared_ptr<const Sample>& src, const TParams& P, double srcBpm, int srcPc, bool srcMinor,
                                     const std::atomic<bool>* cancel = nullptr) {
        if (!src || src->size() < 64) return nullptr;
        const double sr = src->sr, dur = src->dur();
        const int total = src->size();
        const int a = std::max(0, std::min(total - 64, (int)(P.za * total))), b = std::max(a + 64, std::min(total, (int)(P.zb * total)));
        const double pf = std::pow(2.0, P.semi / 12.0);
        double T = P.mode == 0 ? pf : (P.sync && srcBpm > 0 && P.hostBpm > 0 ? P.hostBpm / srcBpm : P.speed / 100.0);
        T = std::max(0.25, std::min(4.0, T));
        (void)dur;
        // étape 1 (mise en cache) : zone, reverse, hauteur / étirement
        const Key k{src.get(), a, b, P.rev, P.mode, (int)std::lround(P.semi * 1000), P.mode == 0 ? 0 : (int)std::lround(T * 10000) * 2 + (P.tp ? 1 : 0)};
        if (!(k == key) || stage.empty()) {
            Chs chs(2);
            for (int c = 0; c < 2; ++c) chs[(size_t)c].assign(src->ch[c].begin() + a, src->ch[c].begin() + b);
            if (P.rev) for (auto& c : chs) std::reverse(c.begin(), c.end());
            if (P.mode == 0) chs = resample(chs, pf);
            else { chs = P.tp ? stretchTP(chs, pf / T, sr, cancel) : stretch(chs, pf / T, cancel); if (cancel && cancel->load()) return nullptr; chs = resample(chs, pf); }
            stage = std::move(chs); key = k; keep = src;
        }
        Chs x = stage;
        const int n0 = (int)x[0].size();
        // échantillonnage puis bits
        if (P.rate < 0.995f) {
            const double hold = sr / rateHz(P.rate);
            for (auto& c : x) { double acc = 0; float v = 0; for (int i = 0; i < n0; ++i) { if (--acc <= 0) { v = c[(size_t)i]; acc += hold; } c[(size_t)i] = v; } }
        }
        if (P.bits < 16) { const int bt = std::max(2, (int)std::lround(P.bits)); for (auto& c : x) for (auto& s : c) s = quantize(s, bt); }
        // pleurage : lecture à vitesse modulée (0,55 Hz et 6,3 Hz)
        if (P.wow > 0) {
            Chs y(2);
            std::vector<double> pos;
            double p = 0; int i = 0;
            while (p < n0 - 1 && i < n0 * 2) {
                const double t = i / sr, det = P.wow * 32 * std::sin(2 * kPi * 0.55 * t) + P.wow * 7 * std::sin(2 * kPi * 6.3 * t);
                pos.push_back(p); p += std::pow(2.0, det / 1200.0); ++i;
            }
            for (int c = 0; c < 2; ++c) { y[(size_t)c].resize(pos.size()); for (size_t j = 0; j < pos.size(); ++j) y[(size_t)c][j] = hermite(x[(size_t)c].data(), n0, pos[j]); }
            x = std::move(y);
        }
        const int n = (int)x[0].size();
        // filtres
        {
            onek::Biquad hp[2], lp[2];
            const float hpf = P.cut < 0.5f ? 120.f + (0.5f - P.cut) * 900.f : 20.f, lpf = (float)std::min(20000.0, cutHz(P.cut));
            const bool useLp = P.cut < 0.98f;
            for (int c = 0; c < 2; ++c) { hp[c].highpass((float)sr, hpf, 0.707f); if (useLp) lp[c].lowpass((float)sr, std::min(lpf, 0.45f * (float)sr), 0.5f); }
            for (int c = 0; c < 2; ++c) for (auto& s : x[(size_t)c]) { s = hp[c].process(s); if (useLp) s = lp[c].process(s); }
        }
        // réverbe + queue
        const int tail = P.verb > 0 ? (int)(sr * 2.6) : 0;
        std::vector<float> L((size_t)n + (size_t)tail, 0.f), R = L;
        std::copy(x[0].begin(), x[0].end(), L.begin()); std::copy(x[1].begin(), x[1].end(), R.begin());
        if (tail) {
            voile::Reverb rv; rv.init(sr); rv.setDecay(2.4f);
            const float dry = 1.f - P.verb * 0.35f, wet = P.verb * 1.1f;
            for (int i = 0; i < n + tail; ++i) {
                float ol, orr;
                rv.process(0.5f * (L[(size_t)i] + R[(size_t)i]), 0.35f, ol, orr);
                L[(size_t)i] = L[(size_t)i] * dry + wet * ol; R[(size_t)i] = R[(size_t)i] * dry + wet * orr;
            }
        }
        if (P.crackle > 0) addCrackle(L, R, P.crackle, sr);
        std::vector<std::vector<float>> o{std::move(L), std::move(R)};
        if (P.loop) {  // la queue se replie sur le début : la boucle reste continue
            for (auto& d : o) { std::vector<float> y(d.begin(), d.begin() + n); for (size_t i = (size_t)n; i < d.size(); ++i) y[(i - (size_t)n) % (size_t)n] += d[i]; d = std::move(y); }
        } else if (tail) {
            for (auto& d : o) { size_t e = d.size(); while (e > (size_t)n && std::fabs(d[e - 1]) < 1e-4f) --e; d.resize(e); }
        }
        const double outBpm = srcBpm > 0 ? srcBpm * T : 120.0;
        if (P.tape) {  // arrêt de bande sur le dernier temps
            const double D = std::min(o[0].size() / sr * 0.5, 60.0 / outBpm * 2);
            const int Ln = (int)(D * sr), st = (int)o[0].size() - Ln;
            for (auto& d : o) {
                const std::vector<float> y = d;
                double pos = st;
                for (int i = 0; i < Ln; ++i) { const double rr = 1.0 - (double)i / Ln; const int i1 = (int)pos; const double t = pos - i1; const float v0 = i1 < (int)y.size() ? y[(size_t)i1] : 0.f, v1 = i1 + 1 < (int)y.size() ? y[(size_t)i1 + 1] : 0.f; d[(size_t)(st + i)] = (float)((v0 * (1 - t) + v1 * t) * std::min(1.0, rr * 4)); pos += rr; }
            }
        }
        if (!P.loop) { const int f = std::min((int)(sr * 0.004), (int)o[0].size() >> 2); for (auto& d : o) for (int i = 0; i < f; ++i) { d[(size_t)i] *= (float)i / f; d[d.size() - 1 - (size_t)i] *= (float)i / f; } }
        float pk = 0;
        for (auto& d : o) for (float v : d) pk = std::max(pk, std::fabs(v));
        if (pk > 0.97f) for (auto& d : o) for (auto& v : d) v *= 0.97f / pk;
        auto out = std::make_shared<Rendered>();
        auto pcm = std::make_shared<Sample>();
        pcm->sr = sr; pcm->ch[0] = std::move(o[0]); pcm->ch[1] = std::move(o[1]);
        out->pcm = pcm;
        out->T = T;
        out->bpm = srcBpm > 0 ? srcBpm * T : 0.0;
        out->pc = srcPc >= 0 ? (((srcPc + (int)std::lround(P.semi)) % 12) + 12) % 12 : -1;
        out->minor = srcMinor;
        chopRendered(*out, P.chop);
        return out;
    }

private:
    struct Key {
        const Sample* s = nullptr; int a = 0, b = 0; bool rev = false; int mode = 0, semi = 0, t = 0;
        bool operator==(const Key& o) const { return s == o.s && a == o.a && b == o.b && rev == o.rev && mode == o.mode && semi == o.semi && t == o.t; }
    };
    Key key;
    Chs stage;
    std::shared_ptr<const Sample> keep;  // garde la source vivante : l'adresse du cache reste valide
};

}  // namespace osamp
