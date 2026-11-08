// Balistique d'un vumètre VU (IEC 60268-17) : aiguille du 2e ordre légèrement amortie,
// 99 % de la déviation finale en 300 ms, dépassement d'environ 1,5 %.
// Alimentée en RMS ; 0 VU = -18 dBFS RMS par défaut. Sans dépendance JUCE (testable avec g++ seul).
#pragma once
#include <algorithm>
#include <cmath>

namespace kit {

struct VuBallistics {
    static constexpr double zeta = 0.80;    // amortissement : dépassement ≈ 1,5 %
    static constexpr double omega = 13.10;  // pulsation propre (rad/s) : 99 % en 300 ms
    double refDbfs = -18.0;                 // niveau RMS (dBFS) qui donne 0 VU

    void reset() { x = v = 0.0; }

    // Fait avancer l'aiguille de dt secondes avec une entrée RMS linéaire (pleine échelle = 1).
    void step(double rmsLinear, double dt) {
        const double target = rmsLinear / std::pow(10.0, refDbfs / 20.0);  // tension relative à 0 VU
        const int n = std::max(1, (int)std::ceil(dt / 0.0005));             // pas internes de 0,5 ms
        const double h = dt / n;
        for (int i = 0; i < n; ++i) {
            v += h * (omega * omega * (target - x) - 2.0 * zeta * omega * v);
            x += h * v;
        }
        x = std::max(0.0, x);
    }
    // Bloc d'échantillons (mono ou déjà sommé) : RMS du bloc, puis avance de n / sampleRate.
    void process(const float* data, int n, double sampleRate) {
        if (n <= 0) return;
        double s = 0;
        for (int i = 0; i < n; ++i) s += (double)data[i] * data[i];
        step(std::sqrt(s / n), n / sampleRate);
    }
    double position() const { return x; }  // tension relative (1 = 0 VU)
    double vu() const { return x > 1e-6 ? 20.0 * std::log10(x) : -60.0; }

    // Position de l'aiguille sur l'arc (0..1) : proportionnelle à la tension, +3 VU en butée, 0 VU à 71 %.
    static double needleFraction(double vuDb) { return std::clamp(std::pow(10.0, vuDb / 20.0) / std::pow(10.0, 3.0 / 20.0), 0.0, 1.06); }

private:
    double x = 0.0, v = 0.0;
};

}  // namespace kit
