#include <cmath>
#include <cstdio>
#include <vector>

#include <distingnt/api.h>

const _NT_globals NT_globals = {48000, 128, NULL, 0, 0, 0};

#include "../thik_osc.cpp"

struct SignalStats {
    double mean;
    double rms;
};

static SignalStats measureTriangle(float note, float sampleRate) {
    VoiceState voice = {};
    voice.phase = 0.137f;
    voice.tri = 1.0f - 4.0f * fastAbs(voice.phase - 0.5f);

    const float hz = 440.0f * fastExp2((note - 69.0f) * (1.0f / 12.0f));
    const float dt = hz / sampleRate;
    const int warmupFrames = (int)(sampleRate * 4.0f);
    const int measureFrames = (int)(sampleRate * 4.0f);

    for (int i = 0; i < warmupFrames; ++i) {
        processVoice(voice, dt, 1.0f, 0.0f, 1.0f);
    }

    double sum = 0.0;
    double sumSquares = 0.0;
    for (int i = 0; i < measureFrames; ++i) {
        const double sample = (double)processVoice(voice, dt, 1.0f, 0.0f, 1.0f);
        sum += sample;
        sumSquares += sample * sample;
    }

    const double mean = sum / (double)measureFrames;
    const double variance = sumSquares / (double)measureFrames - mean * mean;
    return {mean, std::sqrt(variance)};
}

static bool expectRelative(double actual,
                           double expected,
                           double tolerance,
                           const char* message) {
    const double relativeError = expected != 0.0 ? actual / expected - 1.0 : actual;
    if (std::fabs(relativeError) <= tolerance) return true;
    std::fprintf(
        stderr,
        "%s: got %.6f, expected %.6f (%+.2f%%)\n",
        message,
        actual,
        expected,
        relativeError * 100.0);
    return false;
}

static bool expectAbsolute(double actual,
                           double tolerance,
                           const char* message) {
    if (std::fabs(actual) <= tolerance) return true;
    std::fprintf(stderr, "%s: got %+.6f\n", message, actual);
    return false;
}

struct RenderFixture {
    std::vector<unsigned char> sram;
    std::vector<unsigned char> dtc;
    int16_t values[kNumParams];
    _NT_algorithm* alg;

    RenderFixture() : alg(NULL) {
        _NT_algorithmRequirements req = {};
        calculateRequirements(req, NULL);
        sram.resize(req.sram);
        dtc.resize(req.dtc);
        _NT_algorithmMemoryPtrs ptrs = {sram.data(), NULL, dtc.data(), NULL};
        alg = construct(ptrs, req, NULL);
        for (int i = 0; i < kNumParams; ++i) values[i] = parameters[i].def;
        values[kParamOutputLMode] = 1;
        values[kParamOutputRMode] = 1;
        alg->v = values;
        alg->vIncludingCommon = values;
        parameterChanged(alg, 0);
    }
};

static std::vector<float> renderMacroBlock(bool routeZeroMacroCv) {
    RenderFixture fixture;
    if (routeZeroMacroCv) {
        fixture.values[kParamToneCV] = 1;
        fixture.values[kParamThicknessCV] = 2;
    }

    const int frames = 128;
    std::vector<float> bus(28 * frames, 0.0f);
    step(fixture.alg, bus.data(), frames / 4);

    const int leftOffset = (fixture.values[kParamOutputL] - 1) * frames;
    const int rightOffset = (fixture.values[kParamOutputR] - 1) * frames;
    std::vector<float> output(frames * 2);
    for (size_t i = 0; i < (size_t)frames; ++i) {
        output[i] = bus[(size_t)leftOffset + i];
        output[(size_t)frames + i] = bus[(size_t)rightOffset + i];
    }
    return output;
}

static float renderRootPhaseAdvance(float pitchCv) {
    RenderFixture fixture;
    fixture.values[kParamPitch] = 36;
    fixture.values[kParamPitchCV] = 1;

    const int frames = 4;
    std::vector<float> bus(28 * frames, 0.0f);
    for (size_t i = 0; i < (size_t)frames; ++i) bus[i] = pitchCv;

    _thikOscAlgorithm* alg = (_thikOscAlgorithm*)fixture.alg;
    const float start = alg->dtc->voices[0].phase;
    step(fixture.alg, bus.data(), frames / 4);
    return alg->dtc->voices[0].phase - start;
}

int main() {
    bool ok = true;

    const SignalStats low = measureTriangle(12.0f, 48000.0f);
    const SignalStats middle = measureTriangle(60.0f, 48000.0f);
    const SignalStats lowAt96k = measureTriangle(12.0f, 96000.0f);

    ok = expectRelative(
        low.rms,
        middle.rms,
        0.01,
        "triangle level should remain stable across pitch") && ok;
    ok = expectRelative(
        lowAt96k.rms,
        low.rms,
        0.005,
        "triangle level should remain stable across sample rate") && ok;
    ok = expectAbsolute(low.mean, 0.005, "low triangle should remain DC centered") && ok;
    ok = expectAbsolute(middle.mean, 0.005, "middle triangle should remain DC centered") && ok;

    const std::vector<float> staticMacros = renderMacroBlock(false);
    const std::vector<float> zeroMacroCv = renderMacroBlock(true);
    double maximumRenderDifference = 0.0;
    for (size_t i = 0; i < staticMacros.size(); ++i) {
        const double difference = std::fabs((double)staticMacros[i] - (double)zeroMacroCv[i]);
        if (difference > maximumRenderDifference) maximumRenderDifference = difference;
    }
    ok = expectAbsolute(
        maximumRenderDifference,
        0.00001,
        "zero macro CV should match the optimized static path") && ok;

    const float baseAdvance = renderRootPhaseAdvance(0.0f);
    const float octaveAdvance = renderRootPhaseAdvance(1.0f);
    ok = expectRelative(
        (double)octaveAdvance,
        (double)baseAdvance * 2.0,
        0.001,
        "one volt of Pitch CV should double frequency") && ok;

    if (ok) {
        std::printf(
            "DSP fidelity OK (triangle RMS low %.6f, middle %.6f, low@96k %.6f; "
            "path delta %.8f)\n",
            low.rms,
            middle.rms,
            lowAt96k.rms,
            maximumRenderDifference);
    }
    return ok ? 0 : 1;
}
