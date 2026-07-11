#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include <distingnt/api.h>

const _NT_globals NT_globals = {48000, 128, NULL, 0, 0, 0};

#include "../thik_osc.cpp"

struct RmsPair {
    double l;
    double r;
};

static RmsPair measureRms(int thicknessParam, int toneParam) {
    _NT_algorithmRequirements req = {};
    calculateRequirements(req, NULL);

    std::vector<unsigned char> sram(req.sram);
    std::vector<unsigned char> dtc(req.dtc);
    _NT_algorithmMemoryPtrs ptrs = {sram.data(), NULL, dtc.data(), NULL};
    _NT_algorithm* alg = construct(ptrs, req, NULL);

    int16_t values[kNumParams];
    for (int i = 0; i < kNumParams; ++i) values[i] = parameters[i].def;
    values[kParamOutputLMode] = 1;
    values[kParamOutputRMode] = 1;
    values[kParamThickness] = (int16_t)thicknessParam;
    values[kParamTone] = (int16_t)toneParam;
    alg->v = values;
    alg->vIncludingCommon = values;
    parameterChanged(alg, kParamThickness);
    parameterChanged(alg, kParamTone);

    const int framesPerStep = 128;
    const int numFramesBy4 = framesPerStep / 4;
    const int buses = 28;
    std::vector<float> bus(buses * framesPerStep, 0.0f);

    const int warmupSteps = (48000 * 4) / framesPerStep;
    const int measureSteps = (48000 * 16) / framesPerStep;
    for (int stepIndex = 0; stepIndex < warmupSteps; ++stepIndex) {
        std::fill(bus.begin(), bus.end(), 0.0f);
        step(alg, bus.data(), numFramesBy4);
    }

    double sumSqL = 0.0;
    double sumSqR = 0.0;
    long count = 0;
    const int outLBus = values[kParamOutputL] - 1;
    const int outRBus = values[kParamOutputR] - 1;
    for (int stepIndex = 0; stepIndex < measureSteps; ++stepIndex) {
        std::fill(bus.begin(), bus.end(), 0.0f);
        step(alg, bus.data(), numFramesBy4);

        const float* l = bus.data() + outLBus * framesPerStep;
        const float* r = bus.data() + outRBus * framesPerStep;
        for (int i = 0; i < framesPerStep; ++i) {
            sumSqL += (double)l[i] * (double)l[i];
            sumSqR += (double)r[i] * (double)r[i];
            ++count;
        }
    }

    return {std::sqrt(sumSqL / (double)count), std::sqrt(sumSqR / (double)count)};
}

static bool verifyTone(int toneParam) {
    static const int thicknessValues[] = {
        0, 500, 1000, 1500, 2500, 3500, 4500, 5500, 6500, 7500, 8500, 10000,
    };
    static const double allowedRelativeDrift = 0.03;

    double baseline = 0.0;
    double worstRelativeDrift = 0.0;
    int worstThickness = 0;

    for (int i = 0; i < (int)NS_ARRAY_SIZE(thicknessValues); ++i) {
        const int thickness = thicknessValues[i];
        const RmsPair rms = measureRms(thickness, toneParam);
        const double average = 0.5 * (rms.l + rms.r);
        if (i == 0) baseline = average;

        const double relativeDrift = (baseline > 0.0) ? (average / baseline - 1.0) : 0.0;
        if (std::fabs(relativeDrift) > std::fabs(worstRelativeDrift)) {
            worstRelativeDrift = relativeDrift;
            worstThickness = thickness;
        }
    }

    std::printf(
        "Tone %5.1f%%: baseline RMS %.6f, worst Thickness drift %+5.2f%% at %.1f%%\n",
        toneParam * 0.01,
        baseline,
        worstRelativeDrift * 100.0,
        worstThickness * 0.01);

    if (std::fabs(worstRelativeDrift) > allowedRelativeDrift) {
        std::fprintf(
            stderr,
            "RMS drift exceeds %.1f%% at Tone %.1f%%\n",
            allowedRelativeDrift * 100.0,
            toneParam * 0.01);
        return false;
    }

    return true;
}

static bool verifyToneSweep() {
    static const int toneValues[] = {
        0, 1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000,
    };
    static const double allowedRelativeDrift = 0.025;

    double baseline = 0.0;
    double worstRelativeDrift = 0.0;
    int worstTone = 0;
    for (int i = 0; i < (int)NS_ARRAY_SIZE(toneValues); ++i) {
        const RmsPair rms = measureRms(0, toneValues[i]);
        const double average = 0.5 * (rms.l + rms.r);
        if (i == 0) baseline = average;
        const double relativeDrift = average / baseline - 1.0;
        if (std::fabs(relativeDrift) > std::fabs(worstRelativeDrift)) {
            worstRelativeDrift = relativeDrift;
            worstTone = toneValues[i];
        }
    }

    std::printf(
        "Tone sweep: baseline RMS %.6f, worst drift %+5.2f%% at %.1f%%\n",
        baseline,
        worstRelativeDrift * 100.0,
        worstTone * 0.01);
    if (std::fabs(worstRelativeDrift) <= allowedRelativeDrift) return true;

    std::fprintf(
        stderr,
        "Tone RMS drift exceeds %.1f%%\n",
        allowedRelativeDrift * 100.0);
    return false;
}

int main() {
    bool ok = true;
    ok = verifyTone(0) && ok;
    ok = verifyTone(kDefaultTone) && ok;
    ok = verifyTone(10000) && ok;
    ok = verifyToneSweep() && ok;
    return ok ? 0 : 1;
}
