#include "OrbeAnalysis.h"
#include <algorithm>
#include <numeric>

namespace oa {

const char* const kNoteNames[12] = {"C", "C♯", "D", "E♭", "E", "F", "F♯", "G", "A♭", "A", "B♭", "B"};
const int kMajor[7] = {0, 2, 4, 5, 7, 9, 11}, kMinor[7] = {0, 2, 3, 5, 7, 8, 10};

void fft(double* re, double* im, int n) {
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
    }
    for (int len = 2; len <= n; len <<= 1) {
        const double ang = -2 * kPi / len, wr = std::cos(ang), wi = std::sin(ang);
        for (int i = 0; i < n; i += len) {
            double cr = 1, ci = 0;
            for (int j = 0; j < len / 2; ++j) {
                const int a = i + j, b = a + len / 2;
                const double tr = re[b] * cr - im[b] * ci, ti = re[b] * ci + im[b] * cr;
                re[b] = re[a] - tr; im[b] = im[a] - ti; re[a] += tr; im[a] += ti;
                const double nr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = nr;
            }
        }
    }
}

static std::vector<double> hann(int n) { std::vector<double> w((size_t)n); for (int i = 0; i < n; ++i) w[(size_t)i] = .5 - .5 * std::cos(2 * kPi * i / (n - 1)); return w; }

// ---------------- tempo ----------------
Tempo detectTempo(const float* sig, int len, double sr) {
    Tempo out;
    const double dur = len / sr, maxS = std::min(dur, 120.0), st = std::max(0.0, (dur - maxS) / 2);
    const long s0 = (long)std::floor(st * sr), s1 = std::min((long)len, (long)std::floor((st + maxS) * sr));
    const float* x = sig + s0; const long xl = std::max(0L, s1 - s0);
    const int N = 1024, hop = 512;
    const long frames = std::max(0L, (xl - N) / hop);
    if (frames < 400) return out;
    const auto win = hann(N);
    std::vector<double> env((size_t)frames), re(N), im(N);
    const int maxBin = std::min(N / 2, (int)jsRound(8000 / (sr / N)));
    std::vector<double> prev((size_t)maxBin, 0.0), cur((size_t)maxBin, 0.0);
    for (long f = 0; f < frames; ++f) {
        const long o = f * hop;
        for (int i = 0; i < N; ++i) { re[(size_t)i] = x[o + i] * win[(size_t)i]; im[(size_t)i] = 0; }
        fft(re.data(), im.data(), N);
        double fl = 0;
        for (int k = 1; k < maxBin; ++k) { const double m = std::log1p(100 * std::hypot(re[(size_t)k], im[(size_t)k])), d = m - prev[(size_t)k]; if (d > 0) fl += d; cur[(size_t)k] = m; }
        env[(size_t)f] = fl; std::swap(prev, cur);
    }
    const double fps = sr / hop; const long w = (long)jsRound(fps * .4);
    std::vector<double> od((size_t)frames); double acc = 0;
    for (long i = 0; i < frames; ++i) { acc += env[(size_t)i]; if (i >= w) acc -= env[(size_t)(i - w)]; const double mean = acc / (double)std::min(i + 1, w); od[(size_t)i] = std::max(0.0, env[(size_t)i] - mean); }
    auto lagOf = [&](double b) { return 60 * fps / b; };
    const long lmin = (long)std::floor(lagOf(210)), lmax = (long)std::ceil(lagOf(55));
    std::vector<double> ac((size_t)(lmax * 2 + 2), 0.0);
    for (long L = lmin; L <= std::min(lmax * 2, frames - 1); ++L) { double s = 0; for (long i = 0; i + L < frames; ++i) s += od[(size_t)i] * od[(size_t)(i + L)]; ac[(size_t)L] = s / (double)(frames - L); }
    auto A = [&](long i) { return (i >= 0 && i < (long)ac.size()) ? ac[(size_t)i] : 0.0; };
    auto prior = [](double b) { const double z = std::log2(b / 118) / .85; return std::exp(-.5 * z * z); };
    double best = 0, sum = 0, sum2 = 0; long bestL = lmin; int cnt = 0;
    for (long L = lmin; L <= lmax; ++L) {
        const double bpm = 60 * fps / (double)L, s = (A(L) + .5 * A(2 * L) + .25 * A((long)jsRound((double)L / 2))) * prior(bpm);
        sum += s; sum2 += s * s; ++cnt; if (s > best) { best = s; bestL = L; }
    }
    const double y0 = A(bestL - 1), y1 = A(bestL), y2 = A(bestL + 1), den = y0 - 2 * y1 + y2, dl = den != 0 ? .5 * (y0 - y2) / den : 0;
    double bpm = 60 * fps / ((double)bestL + std::max(-.5, std::min(.5, dl)));
    const double mean = sum / cnt, sd = std::sqrt(std::max(1e-12, sum2 / cnt - mean * mean));
    out.conf = std::max(0.0, std::min(1.0, ((best - mean) / sd - 1) / 4));
    auto odAt = [&](double i) { return (i >= 0 && i < (double)frames) ? od[(size_t)i] : 0.0; };
    auto comb = [&](double bb, double& phase) {
        const double P = 60 * fps / bb; double bs = -1, bp = 0;
        for (double p = 0; p < P; p += .5) {
            double s = 0, n = 0;
            for (double k = p; k < (double)frames; k += P) { const double i = jsRound(k); s += odAt(i) + .5 * (odAt(i - 1) + odAt(i + 1)); n++; }
            s /= std::max(1.0, n); if (s > bs) { bs = s; bp = p; }
        }
        phase = bp; return bs;
    };
    double ph = 0;
    struct C { double b, s; }; std::vector<C> cands;
    for (double v : {bpm, bpm * 2, bpm / 2}) if (v >= 55 && v <= 210) cands.push_back({v, comb(v, ph) * prior(v)});
    std::stable_sort(cands.begin(), cands.end(), [](const C& a, const C& b) { return a.s > b.s; });
    if (!cands.empty()) bpm = cands[0].b;
    double fb = bpm, fs = -1;
    for (double d = -1.5; d <= 1.5; d += .05) { const double c = comb(bpm + d, ph); if (c > fs) { fs = c; fb = bpm + d; } }
    bpm = std::abs(fb - jsRound(fb)) < .2 ? jsRound(fb) : jsRound(fb * 10) / 10;
    comb(bpm, ph);
    out.bpm = bpm; out.phase = st + ph / fps;
    for (double v : {bpm / 2, bpm * 2}) if (v >= 50 && v <= 220) out.alt[out.nAlt++] = jsRound(v * 10) / 10;
    return out;
}

// ---------------- tonalité ----------------
static const double KMAJ[12] = {6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88}, KMIN[12] = {6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17};
static double corr12(const double* a, const double* b) {
    double ma = 0, mb = 0; for (int i = 0; i < 12; ++i) { ma += a[i]; mb += b[i]; } ma /= 12; mb /= 12;
    double n = 0, da = 0, db = 0; for (int i = 0; i < 12; ++i) { n += (a[i] - ma) * (b[i] - mb); da += (a[i] - ma) * (a[i] - ma); db += (b[i] - mb) * (b[i] - mb); }
    const double d = da * db; return n / std::sqrt(d != 0 ? d : 1);
}
Key detectKey(const float* sig, int len, double sr) {
    Key out; const double LO = 50, HI = 2000, PW = .5;
    const double dur = len / sr, maxS = std::min(dur, 90.0), st = std::max(0.0, (dur - maxS) / 2);
    const long s0 = (long)std::floor(st * sr), s1 = std::min((long)len, (long)std::floor((st + maxS) * sr));
    const float* x = sig + s0; const long xl = std::max(0L, s1 - s0);
    const int N = 8192, hop = 4096; const long frames = std::max(1L, (xl - N) / hop);
    const auto win = hann(N);
    std::vector<double> re(N), im(N), mag(N / 2);
    double chroma[12] = {};
    const int kLo = (int)std::ceil(LO * N / sr), kHi = std::min(N / 2 - 2, (int)std::floor(HI * N / sr));
    struct Pk { double f, m; }; std::vector<Pk> pk;
    for (long f = 0; f < frames; ++f) {
        const long o = f * hop; double en = 0;
        for (int i = 0; i < N; ++i) { const double v = (o + i < xl ? x[o + i] : 0.0) * win[(size_t)i]; re[(size_t)i] = v; im[(size_t)i] = 0; en += v * v; }
        if (en < 1e-6) continue;
        fft(re.data(), im.data(), N);
        double mx = 0;
        for (int k = 1; k < N / 2; ++k) { mag[(size_t)k] = std::hypot(re[(size_t)k], im[(size_t)k]); if (k >= kLo && k <= kHi && mag[(size_t)k] > mx) mx = mag[(size_t)k]; }
        pk.clear();
        for (int k = kLo; k <= kHi; ++k) if (mag[(size_t)k] > mx * .04 && mag[(size_t)k] >= mag[(size_t)k - 1] && mag[(size_t)k] > mag[(size_t)k + 1]) {
            const double a = mag[(size_t)k - 1], b = mag[(size_t)k], c = mag[(size_t)k + 1], dd = a - 2 * b + c, d = .5 * (a - c) / (dd != 0 ? dd : 1);
            pk.push_back({(k + d) * sr / N, b});
        }
        double c[12] = {};
        for (size_t pi = 0; pi < pk.size(); ++pi) {
            const Pk& p = pk[pi]; double w = 1;
            for (int h = 2; h <= 5; ++h) {
                const double f0 = p.f / h; if (f0 < LO * .7) break;
                auto it = std::lower_bound(pk.begin(), pk.end(), f0 * (1 - .03), [](const Pk& q, double v) { return q.f < v; });
                bool hit = false;
                for (; it != pk.end() && it->f <= f0 * (1 + .03) + 1e-9; ++it) if (&*it != &p && std::abs(it->f - f0) / f0 < .03 && it->m > p.m * .25) { hit = true; break; }
                if (hit) { w = .15; break; }
            }
            const double m = 12 * std::log2(p.f / 440) + 69, r = jsRound(m); if (std::abs(m - r) > .4) continue;
            c[(((int)r % 12) + 12) % 12] += std::pow(p.m, PW) * w * (p.f < 200 ? 1.3 : 1);
        }
        double cm = 0; for (double v : c) cm = std::max(cm, v); if (cm == 0) cm = 1;
        for (int i = 0; i < 12; ++i) chroma[i] += c[i] / cm;
    }
    std::vector<KeyRank> rank;
    for (int t = 0; t < 12; ++t) { double rot[12]; for (int i = 0; i < 12; ++i) rot[i] = chroma[(i + t) % 12]; rank.push_back({t, false, corr12(rot, KMAJ)}); rank.push_back({t, true, corr12(rot, KMIN)}); }
    std::stable_sort(rank.begin(), rank.end(), [](const KeyRank& a, const KeyRank& b) { return a.r > b.r; });
    double mx = 0; for (double v : chroma) mx = std::max(mx, v); if (mx == 0) mx = 1;
    out.pc = rank[0].pc; out.minor = rank[0].minor; out.r = rank[0].r;
    out.conf = std::max(0.0, std::min(1.0, (rank[0].r - rank[1].r) * 6 + (rank[0].r - .5)));
    for (int i = 0; i < 12; ++i) out.chroma[i] = chroma[i] / mx;
    for (int i = 0; i < 4; ++i) out.rank[i] = rank[(size_t)i];
    return out;
}

std::vector<double> onsets(const float* x, int len, double sr, double sens, int maxN) {
    const int hop = 256, n = len / hop; if (n < 4) return {0.0};
    std::vector<double> e((size_t)n), od((size_t)n, 0.0);
    for (int i = 0; i < n; ++i) { double s = 0; for (int j = i * hop; j < (i + 1) * hop; ++j) s += (double)x[j] * x[j]; e[(size_t)i] = std::sqrt(s / hop); }
    for (int i = 1; i < n; ++i) od[(size_t)i] = std::max(0.0, std::log(e[(size_t)i] + 1e-4) - std::log(e[(size_t)i - 1] + 1e-4)) * std::sqrt(e[(size_t)i]);
    double m = 0, s2 = 0; for (double v : od) { m += v; s2 += v * v; } m /= n;
    const double sd = std::sqrt(std::max(0.0, s2 / n - m * m)), thr = m + (2.6 - 2.3 * sens) * sd, emax = *std::max_element(e.begin(), e.end());
    const int gap = (int)jsRound(.07 * sr / hop);
    struct P { int i; double v; }; std::vector<P> pk;
    for (int i = 2; i < n - 1; ++i) if (od[(size_t)i] > thr && od[(size_t)i] >= od[(size_t)i - 1] && od[(size_t)i] > od[(size_t)i + 1] && e[(size_t)i] > emax * .03) {
        if (!pk.empty() && i - pk.back().i < gap) { if (od[(size_t)i] > pk.back().v) pk.back() = {i, od[(size_t)i]}; } else pk.push_back({i, od[(size_t)i]});
    }
    std::stable_sort(pk.begin(), pk.end(), [](const P& a, const P& b) { return a.v > b.v; });
    if ((int)pk.size() > maxN - 1) pk.resize((size_t)maxN - 1);
    std::sort(pk.begin(), pk.end(), [](const P& a, const P& b) { return a.i < b.i; });
    std::vector<double> sel; for (auto& p : pk) sel.push_back(std::max(0.0, (p.i - 1) * (double)hop / sr - .003));
    if (sel.empty() || sel[0] > .03) sel.insert(sel.begin(), 0.0); else sel[0] = 0;
    if ((int)sel.size() > maxN) sel.resize((size_t)maxN);
    return sel;
}

std::string camelot(int pc, bool minor) {
    pc = ((pc % 12) + 12) % 12;
    const int n = minor ? ((7 * ((pc + 3) % 12) + 7) % 12) + 1 : ((7 * pc + 7) % 12) + 1;
    return std::to_string(n) + (minor ? "A" : "B");
}
bool fromCamelot(const std::string& code, int& pc, bool& minor) {
    if (code.size() < 2) return false;
    minor = code.back() == 'A';
    for (int p = 0; p < 12; ++p) if (camelot(p, minor) == code) { pc = p; return true; }
    return false;
}
std::array<std::string, 4> compatCodes(const std::string& code) {
    const int n = std::atoi(code.c_str()); const char l = code.back(), o = l == 'A' ? 'B' : 'A';
    auto w = [](int k) { return ((k - 1 + 12) % 12) + 1; };
    return {code, std::to_string(w(n - 1)) + l, std::to_string(w(n + 1)) + l, std::to_string(n) + o};
}
std::string keyShort(int pc, bool minor) { return std::string(kNoteNames[((pc % 12) + 12) % 12]) + (minor ? "m" : ""); }
std::string keyLong(int pc, bool minor) { return std::string(kNoteNames[((pc % 12) + 12) % 12]) + (minor ? " mineur" : " majeur"); }

// ---------------- mesure du mix ----------------
const Band kBands[7] = {{"sub", "Sub", 20, 60, 4}, {"bass", "Basses", 60, 250, 3}, {"lowmid", "Bas-médiums", 250, 500, 3}, {"mid", "Médiums", 500, 2000, 3},
                        {"highmid", "Haut-médiums", 2000, 4000, 3}, {"presence", "Présence", 4000, 6000, 3.5}, {"air", "Air", 6000, 20000, 4}};
//                                                                  sub bass lowmid mid highmid presence air
const Profile kProfiles[11] = {
    {"trap", "Rap", "Trap / Drill", -9, -6, 6, {4, 1, -1, 0, 0, 0, 1}},
    {"boombap", "Rap", "Boom bap / Lo-fi", -12, -8, 7, {-1, 2, 1, 0, -1, -2, -4}},
    {"rnb", "Pop & alt", "R&B", -11, -8, 7.5, {1, 1, 0, 0, 0, 0, 1}},
    {"pop", "Pop & alt", "Pop", -10, -7, 7, {0, 0, 0, 0, 0, 0, 0}},
    {"hyperpop", "Pop & alt", "Hyperpop / Digicore", -8, -5, 5.5, {2, 0, 0, 0, 1, 2, 2}},
    {"ambient", "Pop & alt", "Ambient", -18, -12, 10, {-3, -1, 0, 1, 0, 0, 1}},
    {"house", "Club", "House / Techno", -9, -6, 6.5, {2, 1, -1, 0, 0, 0, 1}},
    {"dnb", "Club", "DnB / Dubstep", -8, -5, 5.5, {3, 1, -1, 0, 1, 0, 1}},
    {"afro", "Monde", "Afro / Amapiano", -10, -7, 7, {2, 1, 0, 0, 0, 0, 0}},
    {"reggaeton", "Monde", "Reggaeton", -9, -6, 6.5, {2, 1, 0, 0, 0, 0, 0}},
    {"stream", "Neutre", "Streaming neutre", -14, -10, 9, {0, 0, 0, 0, 0, 0, 0}}};

static const char* const ADV[7][2][2] = {
    {{"Trop de sub-basses", "Baisse 30–60 Hz sur l'808 ou la basse, coupe le sub des autres pistes (filtre passe-haut vers 30 Hz) et ajoute un sidechain kick / 808."}, {"Pas assez de sub", "Monte un shelf doux vers 50 Hz ou double ton 808 avec une sinus. Vérifie au casque : les petites enceintes ne montrent pas cette zone."}},
    {{"Basses trop chargées", "Creuse 100–200 Hz sur tout ce qui n'est ni kick ni basse pour leur laisser la place."}, {"Basses légères", "Redonne du corps au kick vers 80–100 Hz et à la fondamentale de la basse."}},
    {{"Bas-médiums boueux", "Coupe 250–400 Hz de 2 à 3 dB sur les pads, claviers et voix doublées : le mix s'éclaircit sans perdre de volume."}, {"Mix un peu creux", "Rends de la chaleur aux voix et claviers vers 300 Hz."}},
    {{"Médiums en avant", "Le son devient nasal ou « boîte » : creuse doucement entre 800 Hz et 1,5 kHz."}, {"Médiums en retrait", "La voix et le lead risquent de manquer de présence : bosse large et douce vers 1 kHz."}},
    {{"Haut-médiums agressifs", "C'est la zone qui fatigue l'oreille : de-esser ou EQ dynamique vers 3 kHz sur la voix et les leads."}, {"Manque de mordant", "Monte un peu 2–4 kHz sur la voix et la snare pour qu'elles passent devant."}},
    {{"Présence trop forte", "Adoucis 4–6 kHz, souvent la faute des hats et des sifflantes."}, {"Manque de présence", "Ajoute un peu de 5 kHz sur la voix et la snare."}},
    {{"Trop d'aigus", "Baisse le shelf aigu ou le volume des hats, surveille les sifflantes."}, {"Mix terne", "Shelf doux à 10–12 kHz sur le bus ou un exciter léger."}}};

double curveFreq(int i) { return 20.0 * std::pow(2.0, i / 6.0); }

static double bandMean(const double* arr, double lo, double hi, int n, double fs) {
    const double bin = fs / n; double a = 0; int c = 0;
    for (int k = std::max(1, (int)std::floor(lo / bin)); k <= std::min(n / 2 - 1, (int)std::ceil(hi / bin)); ++k) { const double fk = k * bin; if (fk < lo || fk > hi) continue; a += arr[k]; ++c; }
    if (c) return a / c;
    const double g = std::sqrt(lo * hi) / bin, fr = g - std::floor(g);
    const int kc = std::max(1, std::min(n / 2 - 1, (int)jsRound(g))), k0 = std::max(1, std::min(n / 2 - 2, (int)std::floor(g)));
    const double v = arr[k0] * (1 - fr) + arr[k0 + 1] * fr; return v != 0 ? v : arr[kc];
}

void MixMeter::prepare(double sr) {
    fs = sr; seg = (int)jsRound(fs * .1);
    hi.init(); hs.init();
    rm.assign(4096, 0.0); rs.assign(4096, 0.0); pm.assign(2048, 0.0); ps.assign(2048, 0.0); win = hann(4096); re.assign(4096, 0.0); im.assign(4096, 0.0); tmp.assign(2048, 0.0); sa.assign(2048, 0.0); sb.assign(2048, 0.0); live.assign(kCurveN, 0.f);
    // pondération K (BS.1770) : étage haut-shelf puis passe-haut
    double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196, K = std::tan(kPi * f0 / fs);
    const double Vh = std::pow(10, G / 20), Vb = std::pow(Vh, 0.4996667741545416), a0 = 1 + K / Q + K * K;
    Bq s1; s1.b0 = (Vh + Vb * K / Q + K * K) / a0; s1.b1 = 2 * (K * K - Vh) / a0; s1.b2 = (Vh - Vb * K / Q + K * K) / a0; s1.a1 = 2 * (K * K - 1) / a0; s1.a2 = (1 - K / Q + K * K) / a0;
    f0 = 38.13547087602444; Q = 0.5003270373238773; K = std::tan(kPi * f0 / fs);
    const double d = 1 + K / Q + K * K; Bq s2; s2.b0 = 1; s2.b1 = -2; s2.b2 = 1; s2.a1 = 2 * (K * K - 1) / d; s2.a2 = (1 - K / Q + K * K) / d;
    kA[0] = kB[0] = s1; kA[1] = kB[1] = s2;
    reset();
}
void MixMeter::reset() {
    hi.clear(); hs.clear();
    for (auto* b : {&kA[0], &kA[1], &kB[0], &kB[1]}) b->x1 = b->x2 = b->y1 = b->y2 = 0;
    segFill = segCount = 0; aSum = bSum = 0; std::fill(segP, segP + 30, 0.0);
    std::memset(hx, 0, sizeof hx);
    peakLin = tpLin = sL = sR = LL = RR = LR = cl = cr = clr = 0; run = clips = 0; total = 0; ring = 0; nFrames = 0;
    std::fill(rm.begin(), rm.end(), 0.0); std::fill(rs.begin(), rs.end(), 0.0); std::fill(pm.begin(), pm.end(), 0.0); std::fill(ps.begin(), ps.end(), 0.0); std::fill(live.begin(), live.end(), 0.f);
}

static void histAdd(std::vector<double>& sum, std::vector<uint32_t>& cnt, double p) {
    const double lk = -0.691 + db10(p); if (lk <= -70) return;
    const int b = std::max(0, std::min(8000, (int)std::floor((lk + 70) * 100))); sum[(size_t)b] += p; cnt[(size_t)b]++;
}

void MixMeter::process(const float* L, const float* R, int n) {
    mono = L == R;
    const double al = std::exp(-1.0 / (0.5 * fs));
    for (int i = 0; i < n; ++i) {
        const double l = L[i], r = R[i], m = std::max(std::abs(l), std::abs(r));
        peakLin = std::max(peakLin, m); sL += l; sR += r; LL += l * l; RR += r * r; LR += l * r;
        cl = cl * al + l * l * (1 - al); cr = cr * al + r * r * (1 - al); clr = clr * al + l * r * (1 - al);
        if (m >= .9995) { if (++run == 3) ++clips; } else run = 0;
        const double thr = peakLin * .6;
        for (int c = 0; c < (mono ? 1 : 2); ++c) {
            double* h = hx[c]; h[0] = h[1]; h[1] = h[2]; h[2] = h[3]; h[3] = c ? r : l;
            if (std::abs(h[1]) < thr && std::abs(h[2]) < thr) continue;
            for (double t : {.25, .5, .75}) {
                const double c0 = h[1], c1 = .5 * (h[2] - h[0]), c2 = h[0] - 2.5 * h[1] + 2 * h[2] - .5 * h[3], c3 = .5 * (h[3] - h[0]) + 1.5 * (h[1] - h[2]);
                tpLin = std::max(tpLin, std::abs(((c3 * t + c2) * t + c1) * t + c0));
            }
        }
        const double kl = kA[1].run(kA[0].run(l)), kr = mono ? kl : kB[1].run(kB[0].run(r));
        aSum += kl * kl; bSum += kr * kr;
        rm[(size_t)ring] = (l + r) * .5; rs[(size_t)ring] = (l - r) * .5; ring = (ring + 1) & 4095; ++total;
        if ((total & 2047) == 0 && total >= 4096) addSpectrum();
        if (++segFill == seg) {
            segP[segCount % 30] = (aSum + bSum) / seg; ++segCount; segFill = 0; aSum = bSum = 0;
            auto meanLast = [&](int k) { double a = 0; for (int j = 1; j <= k; ++j) a += segP[((segCount - j) % 30 + 30) % 30]; return a / k; };
            if (segCount >= 4) histAdd(hi.sum, hi.cnt, meanLast(4));
            if (segCount >= 30 && (segCount - 30) % 5 == 0) histAdd(hs.sum, hs.cnt, meanLast(30));
        }
    }
}

void MixMeter::addSpectrum() {
    double en = 0;
    for (int i = 0; i < 4096; ++i) { const size_t j = (size_t)((ring + i) & 4095); re[(size_t)i] = rm[j] * win[(size_t)i]; im[(size_t)i] = rs[j] * win[(size_t)i]; en += re[(size_t)i] * re[(size_t)i]; }
    if (en < 1e-8) return;
    fft(re.data(), im.data(), 4096);
    for (int k = 1; k < 2048; ++k) {
        const double mr = (re[(size_t)k] + re[(size_t)(4096 - k)]) / 2, mi = (im[(size_t)k] - im[(size_t)(4096 - k)]) / 2;
        const double sr_ = (im[(size_t)k] + im[(size_t)(4096 - k)]) / 2, si = -(re[(size_t)k] - re[(size_t)(4096 - k)]) / 2;
        tmp[(size_t)k] = mr * mr + mi * mi; pm[(size_t)k] += tmp[(size_t)k]; ps[(size_t)k] += sr_ * sr_ + si * si;
    }
    ++nFrames;
    const double q = std::pow(2.0, 1.0 / 12.0);
    for (int i = 0; i < kCurveN; ++i) {
        const float v = (float)db10(bandMean(tmp.data(), curveFreq(i) / q, curveFreq(i) * q, 4096, fs));
        live[(size_t)i] = nFrames == 1 ? v : live[(size_t)i] * .7f + v * .3f;
    }
}

void MixMeter::fill(MixSnap& o) const {
    o = MixSnap(); o.ch = mono ? 1 : 2; o.clips = clips; o.frames = nFrames; o.seconds = seconds();
    auto lk = [](double p) { return std::max(-70.0, -0.691 + db10(p)); };
    auto meanLast = [&](int k) { double a = 0; for (int j = 1; j <= k; ++j) a += segP[((segCount - j) % 30 + 30) % 30]; return a / k; };
    if (segCount >= 4) o.lufsM = (float)lk(meanLast(4));
    if (segCount >= 1) o.lufsS = (float)lk(meanLast(std::min(30, segCount)));
    // intégré : portes −70 LUFS puis −10 LU
    double S = 0; uint64_t C = 0;
    for (size_t b = 0; b < 8001; ++b) { S += hi.sum[b]; C += hi.cnt[b]; }
    if (C) {
        const double rel = lk(S / (double)C) - 10; double S2 = 0; uint64_t C2 = 0;
        for (size_t b = 0; b < 8001; ++b) if (-70 + ((double)b + .5) * .01 > rel) { S2 += hi.sum[b]; C2 += hi.cnt[b]; }
        o.lufsI = C2 ? (float)lk(S2 / (double)C2) : -70.f;
    }
    // LRA : courts termes, porte −20 LU, percentiles 10 et 95
    double Ss = 0; uint64_t Cs = 0;
    for (size_t b = 0; b < 8001; ++b) { Ss += hs.sum[b]; Cs += hs.cnt[b]; }
    if (Cs) {
        const double rel = lk(Ss / (double)Cs) - 20; uint64_t n = 0;
        for (size_t b = 0; b < 8001; ++b) if (-70 + ((double)b + .5) * .01 > rel) n += hs.cnt[b];
        if (n > 2) {
            auto pc = [&](double q) { const uint64_t idx = (uint64_t)std::floor(q * (double)(n - 1)); uint64_t a = 0; for (size_t b = 0; b < 8001; ++b) { const double c = -70 + ((double)b + .5) * .01; if (c <= rel) continue; a += hs.cnt[b]; if (a > idx) return c; } return rel; };
            o.lra = (float)(pc(.95) - pc(.1));
        }
    }
    const double tp = std::max(tpLin, peakLin);
    o.tp = (float)(20 * std::log10(std::max(tp, 1e-6))); o.peak = (float)(20 * std::log10(std::max(peakLin, 1e-6)));
    o.plr = o.lufsI > -70 ? o.tp - o.lufsI : 0.f;
    o.corr = (LL > 0 && RR > 0) ? (float)(LR / std::sqrt(LL * RR)) : 1.f;
    o.corrLive = (cl > 1e-12 && cr > 1e-12) ? (float)std::max(-1.0, std::min(1.0, clr / std::sqrt(cl * cr))) : 1.f;
    o.dc = total ? (float)(std::max(std::abs(sL), std::abs(sR)) / (double)total) : 0.f;
    if (nFrames) {
        const double q = std::pow(2.0, 1.0 / 12.0), nf = nFrames;
        auto& a = sa; auto& b = sb; for (size_t k = 0; k < 2048; ++k) { a[k] = pm[k] / nf; b[k] = ps[k] / nf; }
        for (int i = 0; i < 7; ++i) o.bands[i] = (float)db10(bandMean(a.data(), kBands[i].lo, kBands[i].hi, 4096, fs));
        for (int i = 0; i < kCurveN; ++i) { o.curve[i] = (float)db10(bandMean(a.data(), curveFreq(i) / q, curveFreq(i) * q, 4096, fs)); o.live[i] = live[(size_t)i]; }
        o.lowSide = (float)(db10(bandMean(b.data(), 20, 150, 4096, fs)) - db10(bandMean(a.data(), 20, 150, 4096, fs)));
        double sa = 0, sb = 0; for (size_t k = 0; k < a.size(); ++k) { sa += a[k]; sb += b[k]; }
        o.totSide = (float)(db10(sb) - db10(sa));
        o.valid = true;
    }
}

static void normArr(double* a, int n) { double m = 0; for (int i = 0; i < n; ++i) m += a[i]; m /= n; for (int i = 0; i < n; ++i) a[i] -= m; }

Report evaluate(const MixSnap& A, int pi) {
    Report R; const Profile& P = kProfiles[std::max(0, std::min(10, pi))];
    double base[7];
    for (int i = 0; i < 7; ++i) {
        auto model = [](double f) { return f >= 45 ? -4.5 * std::log2(f / 1000) : -4.5 * std::log2(45.0 / 1000) + 3 * std::log2(f / 45); };
        double a = 0; for (int j = 0; j <= 200; ++j) a += std::pow(10, model(kBands[i].lo + (kBands[i].hi - kBands[i].lo) * j / 200.0) / 10);
        base[i] = db10(a / 201);
    }
    for (int i = 0; i < 7; ++i) R.target[i] = base[i] + P.tilt[i];
    normArr(R.target, 7);
    for (int i = 0; i < 7; ++i) { R.shape[i] = A.bands[i]; R.tol[i] = kBands[i].tol; }
    normArr(R.shape, 7);
    for (int i = 0; i < 7; ++i) R.dev[i] = R.shape[i] - R.target[i];
    R.lufsLo = P.lufsLo; R.lufsHi = P.lufsHi; R.plr = P.plr;
    auto& out = R.diags;
    auto add = [&](int sev, const std::string& title, const std::string& meas, const std::string& fix, int band = -1) { Diag d; d.sev = sev; d.band = band; d.title = title; d.meas = meas; d.fix = fix; out.push_back(d); };
    const double lo = P.lufsLo, hi = P.lufsHi, lufs = A.lufsI;
    const std::string tgt = n1(lo) + " à " + n1(hi);
    if (lufs > hi + .5) add(lufs > hi + 2.5 ? 0 : 1, "Master plus fort que la cible", n1(lufs) + " LUFS · visé " + tgt, "Relâche le limiteur d'environ " + n1(lufs - hi) + " dB. Spotify et YouTube ramènent tout vers −14 LUFS : au-delà de la cible tu perds du punch sans gagner de volume.");
    else if (lufs < lo - .5) add(lufs < lo - 3 ? 0 : 1, "Mix plus faible que la cible", n1(lufs) + " LUFS · visé " + tgt, "Il manque environ " + n1(lo - lufs) + " dB. Monte le gain avant le limiteur du master, ou compresse un peu le bus pour rapprocher les niveaux.");
    else add(2, "Volume dans la cible", n1(lufs) + " LUFS", "Le niveau perçu colle au style visé.");
    if (A.plr < P.plr - 1.5) add(0, "Mix écrasé par la compression", "PLR " + n1(A.plr) + " dB · mini " + n1(P.plr), "Les transitoires (kick, snare) sont aplaties. Baisse le gain du limiteur, ralentis l'attaque des compresseurs du bus, ou utilise un clipper doux avant le limiteur.");
    else if (A.plr < P.plr) add(1, "Dynamique un peu serrée", "PLR " + n1(A.plr) + " dB · mini " + n1(P.plr), "Gagne 1 dB de dynamique en relâchant le limiteur : le mix respirera mieux une fois normalisé.");
    else if (A.plr > P.plr + 7) add(1, "Très dynamique pour le style", "PLR " + n1(A.plr) + " dB", "Les pics dépassent beaucoup le corps du mix : compresse un peu le bus ou les éléments qui sautent.");
    if (A.tp > 0) add(0, "Pics au-dessus de 0 dB", f1(A.tp) + " dBTP", "Ça saturera après conversion en MP3 ou AAC. Mets un limiteur « true peak » en fin de chaîne avec un plafond à −1 dBTP.");
    else if (A.tp > -1) add(1, "Marge de pic trop faible", f1(A.tp) + " dBTP · conseillé ≤ −1", "Règle le plafond du limiteur à −1 dBTP pour éviter la distorsion sur les plateformes.");
    if (A.clips > 20) add(0, "Écrêtage audible", std::to_string(A.clips) + " zones écrêtées", "Le signal contient des suites d'échantillons collées au maximum. Baisse le master ou exporte en 24 bits sans normaliser.");
    else if (A.clips > 0) add(1, "Quelques échantillons écrêtés", std::to_string(A.clips) + " zones", "Baisse de 0,5 dB avant le limiteur ou vérifie le plafond de sortie.");
    bool anyBand = false;
    for (int i = 0; i < 7; ++i) {
        const double d = R.dev[i], t = R.tol[i]; if (std::abs(d) <= t) continue; anyBand = true;
        auto fmt = [](double v) { return v < 1000 ? std::to_string((int)v) : std::to_string((int)(v / 1000)) + "k"; };
        const char* const* a = ADV[i][d > 0 ? 0 : 1];
        add(std::abs(d) > t * 2 ? 0 : 1, std::string(a[0]) + " (" + fmt(kBands[i].lo) + "–" + fmt(kBands[i].hi) + " Hz)", f1(d) + " dB vs cible", a[1], i);
    }
    if (!anyBand) add(2, "Équilibre tonal cohérent", "toutes les zones dans la tolérance", "Aucune zone de fréquences ne sort du lot par rapport à la cible du style.");
    if (A.ch > 1) {
        if (A.corr < 0) add(0, "Problème de phase", "corrélation " + n1(A.corr), "En mono, des éléments vont disparaître. Cherche un élément inversé de phase ou un élargisseur trop poussé, et vérifie le mix en mono.");
        else if (A.corr < .15) add(1, "Stéréo très large", "corrélation " + n1(A.corr), "Risque de perte en mono (téléphone, enceinte Bluetooth). Réduis les effets d'élargissement sur le bus.");
        else if (A.corr > .97) add(1, "Mix presque mono", "corrélation " + n1(A.corr), "Ouvre l'image : panoramique sur les hats et percussions, chorus ou doublage léger sur les pads.");
        else add(2, "Image stéréo saine", "corrélation " + n1(A.corr), "Bon équilibre entre largeur et compatibilité mono.");
        if (A.lowSide > -10) add(0, "Basses trop larges", "côtés " + f1(A.lowSide) + " dB sous 150 Hz", "Mets tout ce qui est sous 120 Hz en mono (utilitaire « bass mono » sur le bus) : le kick et l'808 taperont plus fort et plus droit.");
        else if (A.lowSide > -16) add(1, "Basses un peu larges", "côtés " + f1(A.lowSide) + " dB sous 150 Hz", "Resserre en mono sous 100–120 Hz.");
    }
    if (A.dc > .003) { char b[24]; std::snprintf(b, sizeof b, "%.2f", A.dc * 100); std::string s = b; std::replace(s.begin(), s.end(), '.', ','); add(1, "Décalage DC", s + " %", "Ajoute un filtre passe-haut à 5–10 Hz sur le master : ça libère de la marge."); }
    std::stable_sort(out.begin(), out.end(), [](const Diag& a, const Diag& b) { return a.sev < b.sev; });
    int score = 100; for (auto& d : out) { if (d.sev == 0) score -= 12; if (d.sev == 1) score -= 5; }
    R.score = std::max(0, std::min(100, score));
    for (auto& d : out) d.group = d.band >= 0 ? (d.band < 2 ? "low" : "eq") : "master";
    return R;
}

// ---------------- hauteur (YIN sur signal décimé) ----------------
PitchTrack analyzePitch(const float* in, int n0, double sr0) {
    PitchTrack T; T.dur = n0 / sr0;
    const int dec = sr0 >= 88000 ? 4 : 2; const long nx = n0 / dec;
    std::vector<float> x((size_t)nx); double prev = 0;
    for (long i = 0; i < nx; ++i) { double s = 0; for (int k = 0; k < dec; ++k) s += in[i * dec + k]; s /= dec; prev = prev * .35 + s * .65; x[(size_t)i] = (float)prev; }
    const double sr = sr0 / dec; const int W = 512, hop = 256, tMin = (int)std::floor(sr / 1100), tMax = std::min(W - 1, (int)std::floor(sr / 65));
    std::vector<float> d((size_t)tMax + 2, 0.f);
    float maxR = 0;
    for (long p = 0; p + W * 2 < nx; p += hop) {
        double r = 0; for (int j = 0; j < W; ++j) r += (double)x[(size_t)(p + j)] * x[(size_t)(p + j)];
        r = std::sqrt(r / W); PitchFrame f; f.t = (float)((p + W / 2) / sr); f.rms = (float)r; T.frames.push_back(f); maxR = std::max(maxR, (float)r);
    }
    T.maxR = maxR;
    for (size_t k = 0; k < T.frames.size(); ++k) {
        PitchFrame& fr = T.frames[k]; if (fr.rms < maxR * .03f) continue;
        const float* a = &x[k * hop];
        for (int tau = tMin; tau <= tMax; ++tau) { float s = 0; const float* b = a + tau; for (int j = 0; j < W; ++j) { const float v = a[j] - b[j]; s += v * v; } d[(size_t)tau] = s; }
        float run = 0; int best = -1;
        for (int tau = tMin; tau <= tMax; ++tau) { run += d[(size_t)tau]; d[(size_t)tau] = d[(size_t)tau] * (float)(tau - tMin + 1) / (run != 0 ? run : 1.f); }
        for (int tau = tMin + 1; tau < tMax; ++tau) if (d[(size_t)tau] < .18f && d[(size_t)tau] <= d[(size_t)tau + 1]) { best = tau; break; }
        if (best < 0) { float m = 1e9f; for (int tau = tMin; tau <= tMax; ++tau) if (d[(size_t)tau] < m) { m = d[(size_t)tau]; best = tau; } if (m > .35f) continue; }
        const float a0 = d[(size_t)best - 1], b2 = d[(size_t)best], c = best + 1 <= tMax ? d[(size_t)best + 1] : b2, den = a0 - 2 * b2 + c, sh = den != 0 ? (a0 - c) / (2 * den) : 0.f;
        fr.f = (float)(sr / (best + std::max(-1.f, std::min(1.f, sh)))); fr.q = d[(size_t)best];
    }
    return T;
}

// ---------------- découpage en notes ----------------
SegResult segment(const PitchTrack& T, const SegParams& S) {
    SegResult R; const auto& fr = T.frames; const int nf = (int)fr.size();
    const double thr = .045 + (1 - S.sens) * .08;
    std::vector<char> has((size_t)nf, 0); std::vector<double> mids((size_t)nf, 0.0), med((size_t)nf, 0.0); std::vector<char> hm((size_t)nf, 0);
    for (int i = 0; i < nf; ++i) if (fr[(size_t)i].f > 0 && fr[(size_t)i].rms > T.maxR * thr) { has[(size_t)i] = 1; mids[(size_t)i] = ftom(fr[(size_t)i].f); }
    for (int i = 0; i < nf; ++i) {
        if (!has[(size_t)i]) continue;
        std::vector<double> w; int c = 0;
        for (int k = -2; k <= 2; ++k) { const int j = i + k; if (j >= 0 && j < nf && has[(size_t)j]) { w.push_back(mids[(size_t)j]); ++c; } }
        std::sort(w.begin(), w.end()); med[(size_t)i] = w[c / 2]; hm[(size_t)i] = 1;
    }
    double sx = 0, sy = 0; int cnt = 0;
    for (int i = 0; i < nf; ++i) if (hm[(size_t)i]) { const double a = (med[(size_t)i] - jsRound(med[(size_t)i])) * kPi * 2; sx += std::cos(a); sy += std::sin(a); ++cnt; }
    R.cents = cnt ? std::atan2(sy, sx) / (kPi * 2) : 0; const double off = S.tune ? R.cents : 0;
    struct Seg { double s, e; std::vector<double> vals; double ref; int pend = 0; };
    std::vector<Seg> segs; Seg cur; bool hasCur = false;
    for (int i = 0; i < nf; ++i) {
        const double t = fr[(size_t)i].t;
        if (!hm[(size_t)i]) { if (hasCur) { segs.push_back(cur); hasCur = false; } continue; }
        const double m = med[(size_t)i], q = jsRound(m - off);
        if (hasCur && std::abs(m - off - cur.ref) > .65 && ++cur.pend >= 3) {
            const double cut = fr[(size_t)std::max(0, i - 2)].t; Seg c2 = cur; c2.e = cut; segs.push_back(c2);
            cur = Seg{cut, t, {}, q, 0};
        } else if (hasCur && std::abs(m - off - cur.ref) <= .65) cur.pend = 0;
        const bool onset = hasCur && i > 2 && fr[(size_t)i].rms > fr[(size_t)i - 2].rms * 2.2 && fr[(size_t)i - 2].rms < T.maxR * .25;
        if (onset) { segs.push_back(cur); hasCur = false; }
        if (!hasCur) { cur = Seg{t, t, {}, q, 0}; hasCur = true; }
        cur.e = t + .012; cur.vals.push_back(m - off);
        if (cur.vals.size() == 4) { auto v = cur.vals; std::sort(v.begin(), v.end()); cur.ref = jsRound(v[2]); }
    }
    if (hasCur) segs.push_back(cur);
    std::vector<Note> ns;
    for (auto& s : segs) {
        if (!(s.e - s.s >= S.minlen) || s.vals.empty()) continue;
        auto v = s.vals; std::sort(v.begin(), v.end()); const double md = v[v.size() / 2]; ns.push_back({s.s, s.e, md, (int)jsRound(md)});
    }
    double hist[12] = {}; for (auto& x : ns) hist[((x.n % 12) + 12) % 12] += x.e - x.s;
    static const double MAJ[12] = {6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88}, MIN[12] = {6.33, 2.68, 3.52, 5.38, 2.6, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17};
    double bestS = -2; int bk = 9, bm = 0;
    for (int r = 0; r < 12; ++r) for (int mm = 1; mm >= 0; --mm) {   // majeur d'abord, comme le prototype
        const double* p = mm ? MAJ : MIN; double mh = 0, mp = 0; for (int i = 0; i < 12; ++i) { mh += hist[i]; mp += p[i]; } mh /= 12; mp /= 12;
        double a = 0, b = 0, c = 0; for (int i = 0; i < 12; ++i) { const double xx = hist[(i + r) % 12] - mh, y = p[i] - mp; a += xx * y; b += xx * xx; c += y * y; }
        const double s = a / std::sqrt(b * c != 0 ? b * c : 1); if (s > bestS) { bestS = s; bk = r; bm = mm ? 1 : 0; }
    }
    if (ns.size() >= 3) { R.autoKey = bk; R.autoMode = bm == 1 ? 1 : 0; }
    R.key = S.key >= 0 ? S.key : R.autoKey; R.mode = S.mode >= 0 ? S.mode : R.autoMode;
    if (S.snap && R.mode != 2) {
        const int* iv = R.mode == 0 ? kMinor : kMajor;
        auto in = [&](int pc) { for (int i = 0; i < 7; ++i) if (iv[i] == pc) return true; return false; };
        for (auto& x : ns) {
            const int pc = (((x.n - R.key) % 12) + 12) % 12; if (in(pc)) continue;
            const bool up = in((pc + 1) % 12), dn = in((pc + 11) % 12);
            x.n += up && dn ? (x.raw > x.n ? 1 : -1) : up ? 1 : -1;
        }
    }
    std::vector<Note> mg;
    for (auto& x : ns) { if (!mg.empty() && mg.back().n == x.n && x.s - mg.back().e < .05) mg.back().e = x.e; else mg.push_back(x); }
    if (S.grid) {
        const double st = 60.0 / S.bpm / 4 * S.grid;
        for (auto& x : mg) { x.s = jsRound(x.s / st) * st; x.e = std::max(x.s + st, jsRound(x.e / st) * st); }
        for (size_t i = 0; i + 1 < mg.size(); ++i) if (mg[i].e > mg[i + 1].s) mg[i].e = mg[i + 1].s;
    }
    for (auto& x : mg) if (x.e > x.s) { Note y = x; y.n = x.n + 12 * S.oct; R.notes.push_back(y); }
    return R;
}

std::vector<uint8_t> writeMidi(const std::vector<MidiNote>& notes, double bpm, const char* trackName) {
    struct Ev { long tick; int ord; std::vector<uint8_t> b; };
    auto vlq = [](unsigned n) { std::vector<uint8_t> b{(uint8_t)(n & 0x7f)}; n >>= 7; while (n) { b.insert(b.begin(), (uint8_t)((n & 0x7f) | 0x80)); n >>= 7; } return b; };
    auto chunk = [&](std::vector<Ev> evs) {
        std::stable_sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) { return a.tick != b.tick ? a.tick < b.tick : a.ord < b.ord; });
        std::vector<uint8_t> o; long last = 0;
        for (auto& e : evs) { auto v = vlq((unsigned)(e.tick - last)); o.insert(o.end(), v.begin(), v.end()); o.insert(o.end(), e.b.begin(), e.b.end()); last = e.tick; }
        o.insert(o.end(), {0, 0xff, 0x2f, 0});
        std::vector<uint8_t> c{'M', 'T', 'r', 'k', (uint8_t)(o.size() >> 24), (uint8_t)(o.size() >> 16), (uint8_t)(o.size() >> 8), (uint8_t)o.size()}; c.insert(c.end(), o.begin(), o.end()); return c;
    };
    auto nameEv = [&](const std::string& s) { std::vector<uint8_t> b{0xff, 0x03}; auto v = vlq((unsigned)s.size()); b.insert(b.end(), v.begin(), v.end()); b.insert(b.end(), s.begin(), s.end()); return b; };
    const int PPQ = 480; const long m = (long)jsRound(60000000.0 / bpm);
    std::vector<std::vector<uint8_t>> chunks;
    chunks.push_back(chunk({{0, 0, nameEv("Orbe")}, {0, 0, {0xff, 0x51, 3, (uint8_t)(m >> 16), (uint8_t)(m >> 8), (uint8_t)m}}, {0, 0, {0xff, 0x58, 4, 4, 2, 24, 8}}}));
    std::vector<Ev> evs{{0, 0, nameEv(trackName)}};
    for (auto& x : notes) {
        const long on = (long)jsRound(x.t * PPQ), off = std::max(on + 1, (long)jsRound((x.t + x.d) * PPQ));
        const int vel = std::max(1, std::min(127, (int)jsRound(x.v * 115) + 8)), nn = std::max(0, std::min(127, x.n));
        evs.push_back({on, 2, {0x90, (uint8_t)nn, (uint8_t)vel}}); evs.push_back({off, 1, {0x80, (uint8_t)nn, 0}});
    }
    chunks.push_back(chunk(evs));
    std::vector<uint8_t> f{'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, (uint8_t)chunks.size(), (uint8_t)(PPQ >> 8), (uint8_t)(PPQ & 255)};
    for (auto& c : chunks) f.insert(f.end(), c.begin(), c.end());
    return f;
}

}  // namespace oa
