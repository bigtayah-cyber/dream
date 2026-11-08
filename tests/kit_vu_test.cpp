// Test de la balistique du vumètre du Kit (sans JUCE).
// g++ -std=c++17 -O2 tests/kit_vu_test.cpp -o kit_vu_test && ./kit_vu_test
// ou : cmake --build . --target kit_vu_test
#include "../Source/Kit/VuBallistics.h"
#include <cstdio>
#include <vector>

int main() {
    const double sr = 48000.0, pi = 3.14159265358979323846;
    const int block = 256;
    const double amp = std::pow(10.0, -18.0 / 20.0) * std::sqrt(2.0);  // sinus à -18 dBFS RMS
    std::vector<float> buf((size_t)block);
    int fails = 0;

    // 1) Niveau : sinus 1 kHz à -18 dBFS RMS -> 0 VU ±0,2 après stabilisation.
    kit::VuBallistics vu;
    long n = 0;
    double t99 = -1, peak = 0;
    for (int b = 0; b < (int)(3.0 * sr / block); ++b) {
        for (int i = 0; i < block; ++i, ++n) buf[(size_t)i] = (float)(amp * std::sin(2 * pi * 1000.0 * n / sr));
        vu.process(buf.data(), block, sr);
        const double t = (double)(b + 1) * block / sr;
        if (t99 < 0 && vu.position() >= 0.99) t99 = t;
        peak = std::max(peak, vu.position());
    }
    std::printf("sinus -18 dBFS RMS : %.3f VU (attendu 0 +-0,2)\n", vu.vu());
    if (std::abs(vu.vu()) > 0.2) ++fails;

    // 2) Temps de montée à 99 % (échelon de 0 à 0 VU) : 300 ms ±10 %, avec la granularité du bloc.
    std::printf("montee a 99 %% : %.1f ms (attendu 270..330)\n", t99 * 1000.0);
    if (t99 < 0.270 || t99 > 0.330) ++fails;

    // 3) Dépassement : légèrement amorti (0,5..2,5 %).
    std::printf("depassement : %.2f %%\n", (peak - 1.0) * 100.0);
    if (peak < 1.005 || peak > 1.025) ++fails;

    // 4) Position sur l'arc : 0 VU à 71 % de l'échelle, +3 VU en butée.
    const double f0 = kit::VuBallistics::needleFraction(0.0), f3 = kit::VuBallistics::needleFraction(3.0);
    std::printf("0 VU -> %.1f %% de l'arc, +3 VU -> %.1f %%\n", f0 * 100.0, f3 * 100.0);
    if (std::abs(f0 - 0.708) > 0.01 || std::abs(f3 - 1.0) > 1e-6) ++fails;

    std::printf(fails ? "ECHEC (%d)\n" : "OK\n", fails);
    return fails ? 1 : 0;
}
