#include <cstdio>
#include <vector>

#include <distingnt/api.h>

const _NT_globals NT_globals = {48000, 128, NULL, 0, 0, 0};

#include "../thik_osc.cpp"

static bool expectTrue(bool value, const char* message) {
    if (value) return true;
    std::fprintf(stderr, "%s\n", message);
    return false;
}

static bool expectFloat(float actual, float expected, const char* message) {
    if (actual == expected) return true;
    std::fprintf(stderr, "%s: got %.3f, expected %.3f\n", message, actual, expected);
    return false;
}

static float renderRootPhaseAdvance(_NT_algorithm* base) {
    _thikOscAlgorithm* alg = (_thikOscAlgorithm*)base;
    const int framesPerStep = 16;
    std::vector<float> bus(28 * framesPerStep, 0.0f);

    const float start = alg->dtc->voices[0].phase;
    step(base, bus.data(), framesPerStep / 4);
    float end = alg->dtc->voices[0].phase;
    if (end < start) end += 1.0f;
    return end - start;
}

int main() {
    _NT_algorithmRequirements req = {};
    calculateRequirements(req, NULL);

    std::vector<unsigned char> sram(req.sram);
    std::vector<unsigned char> dtc(req.dtc);
    _NT_algorithmMemoryPtrs ptrs = {sram.data(), NULL, dtc.data(), NULL};
    _NT_algorithm* base = construct(ptrs, req, NULL);
    _thikOscAlgorithm* alg = (_thikOscAlgorithm*)base;

    int16_t values[kNumParams];
    for (int i = 0; i < kNumParams; ++i) values[i] = parameters[i].def;
    base->v = values;
    base->vIncludingCommon = values;
    parameterChanged(base, kParamPitch);

    bool ok = true;
    ok = expectTrue(factory.midiMessage == midiMessage, "factory does not expose midiMessage") && ok;
    ok = expectFloat(activePitchNote(alg), 60.0f, "fallback pitch should be the Pitch parameter") && ok;

    midiMessage(base, 0x92, 72, 100);
    ok = expectTrue(alg->midiNoteActive, "omni note-on should activate MIDI pitch") && ok;
    ok = expectFloat(activePitchNote(alg), 72.0f, "MIDI note-on should override fallback pitch") && ok;

    midiMessage(base, 0x82, 71, 0);
    ok = expectTrue(alg->midiNoteActive, "different note-off should not release current MIDI pitch") && ok;

    midiMessage(base, 0x92, 72, 0);
    ok = expectTrue(!alg->midiNoteActive, "velocity-zero note-on should release current MIDI pitch") && ok;
    ok = expectFloat(activePitchNote(alg), 60.0f, "released MIDI pitch should return to fallback pitch") && ok;

    values[kParamPitch] = 64;
    values[kParamMidiChannel] = 2;
    parameterChanged(base, kParamPitch);
    parameterChanged(base, kParamMidiChannel);

    midiMessage(base, 0x90, 67, 100);
    ok = expectTrue(!alg->midiNoteActive, "wrong MIDI channel should be ignored") && ok;
    ok = expectFloat(activePitchNote(alg), 64.0f, "ignored channel should leave fallback pitch active") && ok;

    midiMessage(base, 0x91, 67, 100);
    ok = expectTrue(alg->midiNoteActive, "selected MIDI channel should activate MIDI pitch") && ok;
    ok = expectFloat(activePitchNote(alg), 67.0f, "selected MIDI channel should set pitch") && ok;

    midiMessage(base, 0x81, 67, 0);
    ok = expectTrue(!alg->midiNoteActive, "selected MIDI channel note-off should release pitch") && ok;
    ok = expectFloat(activePitchNote(alg), 64.0f, "note-off should return to updated fallback pitch") && ok;

    values[kParamPitch] = 60;
    values[kParamMidiChannel] = 0;
    parameterChanged(base, kParamPitch);
    parameterChanged(base, kParamMidiChannel);
    alg->midiNoteActive = false;
    initialiseVoices(alg->dtc, (float)NT_globals.sampleRate);
    const float fallbackAdvance = renderRootPhaseAdvance(base);

    midiMessage(base, 0x90, 72, 100);
    initialiseVoices(alg->dtc, (float)NT_globals.sampleRate);
    const float midiAdvance = renderRootPhaseAdvance(base);
    ok = expectTrue(
        midiAdvance > fallbackAdvance * 1.8f && midiAdvance < fallbackAdvance * 2.2f,
        "rendered pitch should follow the active MIDI note") && ok;

    if (ok) std::printf("MIDI pitch handling OK\n");
    return ok ? 0 : 1;
}
