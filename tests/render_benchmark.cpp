#include <chrono>
#include <cstdio>
#include <vector>

#include <distingnt/api.h>

const _NT_globals NT_globals = {48000, 128, NULL, 0, 0, 0};

#include "../thik_osc.cpp"

static volatile float benchmarkSink = 0.0f;

struct AlgorithmFixture {
    std::vector<unsigned char> sram;
    std::vector<unsigned char> dtc;
    int16_t values[kNumParams];
    _NT_algorithm* alg;

    explicit AlgorithmFixture(bool macroCv) : alg(NULL) {
        _NT_algorithmRequirements req = {};
        calculateRequirements(req, NULL);
        sram.resize(req.sram);
        dtc.resize(req.dtc);
        _NT_algorithmMemoryPtrs ptrs = {sram.data(), NULL, dtc.data(), NULL};
        alg = construct(ptrs, req, NULL);

        for (int i = 0; i < kNumParams; ++i) values[i] = parameters[i].def;
        values[kParamOutputLMode] = 1;
        values[kParamOutputRMode] = 1;
        if (macroCv) {
            values[kParamToneCV] = 1;
            values[kParamThicknessCV] = 2;
        }
        alg->v = values;
        alg->vIncludingCommon = values;
        parameterChanged(alg, 0);
    }
};

static void benchmark(const char* label, bool macroCv) {
    AlgorithmFixture fixture(macroCv);
    const int framesPerStep = 128;
    const int numFramesBy4 = framesPerStep / 4;
    const int iterations = 30000;
    std::vector<float> bus(28 * framesPerStep, 0.0f);

    for (int i = 0; i < 100; ++i) step(fixture.alg, bus.data(), numFramesBy4);

    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) step(fixture.alg, bus.data(), numFramesBy4);
    const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

    const double seconds = std::chrono::duration<double>(end - start).count();
    const double samples = (double)framesPerStep * (double)iterations;
    const int outputOffset = (fixture.values[kParamOutputL] - 1) * framesPerStep;
    benchmarkSink += bus[(size_t)outputOffset]
        + ((_thikOscAlgorithm*)fixture.alg)->dtc->voices[0].phase;
    std::printf(
        "%s: %.2f Msamples/s (%.1fx real time, checksum %.6f)\n",
        label,
        samples / seconds / 1000000.0,
        samples / seconds / 48000.0,
        (double)benchmarkSink);
}

int main() {
    benchmark("Static macros", false);
    benchmark("Tone + Thickness CV", true);
    return 0;
}
