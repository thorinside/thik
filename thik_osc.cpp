#include <distingnt/api.h>
#include <math.h>
#include <new>
#include <stdint.h>

#ifndef NULL
#define NULL 0
#endif

#define NS_ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static const float kTwoPi = 6.28318530717958647692f;
static const float kCentsToLinearRatioApprox = 0.0005776226504666211f;
static const float kMacroCvScale = 0.2f;              // 5V spans the full macro range.
static const float kMaxThicknessDetuneCents = 14.0f;
static const float kMaxThicknessDriftCents = 1.6f;
static const float kMaxStereoWidth = 0.82f;
static const float kOutputGain = 4.35f;
static const float kSoftClipDrive = 0.22f;
static const float kOutputVolts = 5.0f;
static const float kMaxPhaseIncrement = 0.45f;
static const float kTriangleLeakPerCycle = 0.04f;
static const float kMinThicknessLevelTrim = 0.43f;
static const float kThicknessLevelTrimRise = 3.5f;
static const float kMaxThicknessLevelLift = 0.14f;
static const int kNumVoices = 9;
static const int kDefaultNote = 60;
static const int kDefaultFineTune = 0;
static const int kDefaultThickness = 5500;
static const int kDefaultTone = 4500;
static const int kDefaultMidiChannel = 0;

static inline float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static inline float clampf(float x, float lo, float hi) {
    if (!(x == x)) return lo;
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static inline float finiteOr(float x, float fallback) {
    return (x == x) ? x : fallback;
}

static inline float polyBlep(float t, float dt) {
    if (dt <= 0.0f) return 0.0f;
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

static inline float fastAbs(float x) {
    return (x < 0.0f) ? -x : x;
}

static inline float fastExp2(float x) {
    int whole = (int)x;
    if (x < (float)whole) --whole;

    float frac = x - (float)whole;
    float frac2 = frac * frac;
    float frac3 = frac2 * frac;
    float frac4 = frac3 * frac;
    float frac5 = frac4 * frac;
    float frac6 = frac5 * frac;

    // 6th-order expansion of 2^frac on [0, 1). This keeps pitch error very
    // small without depending on libm symbols in the hardware object.
    float mantissa = 1.0f
        + 0.69314718056f * frac
        + 0.24022650695f * frac2
        + 0.05550410866f * frac3
        + 0.00961812911f * frac4
        + 0.00133335581f * frac5
        + 0.00015403530f * frac6;

    if (whole < -126) whole = -126;
    if (whole > 127) whole = 127;

    union Bits {
        uint32_t u;
        float f;
    } bits;
    bits.u = (uint32_t)(whole + 127) << 23;
    return mantissa * bits.f;
}

static inline float softClip(float x) {
    x = clampf(x, -4.0f, 4.0f);
    return x / (1.0f + 0.28f * fastAbs(x));
}

static inline float smoothstep01(float x) {
    x = clamp01(x);
    return x * x * (3.0f - 2.0f * x);
}

static inline float toneLevelNorm(float tone) {
    // Triangle and saw are close to orthogonal at the same phase, so dividing
    // by their amplitude sum makes the middle of the Tone sweep substantially
    // quieter. This table approximates constant-power normalization while
    // preserving the existing level at Tone = 0.
    static const float normalization[] = {
        0.98029605f, 1.02970361f, 1.07191938f, 1.10288456f,
        1.11902446f, 1.11825741f, 1.10068589f, 1.06856076f,
        1.02554120f, 0.97568298f, 0.92262353f,
    };

    tone = clamp01(tone);
    const float tablePosition = tone * 10.0f;
    const int index = (int)tablePosition;
    if (index >= (int)NS_ARRAY_SIZE(normalization) - 1) {
        return normalization[NS_ARRAY_SIZE(normalization) - 1];
    }
    const float frac = tablePosition - (float)index;
    return normalization[index]
        + (normalization[index + 1] - normalization[index]) * frac;
}

static inline float thicknessCurveCorrection(float thickness) {
    static const float correction[] = {
        1.000f, 0.933f, 0.897f, 0.910f, 0.899f,
        0.887f, 0.912f, 0.938f, 0.996f, 1.054f,
        1.076f, 1.097f, 1.086f, 1.075f, 1.053f,
        1.031f, 1.034f, 1.036f, 1.030f, 1.024f,
        1.019f,
    };

    thickness = clamp01(thickness);
    const float tablePosition = thickness * 20.0f;
    const int index = (int)tablePosition;
    if (index >= (int)NS_ARRAY_SIZE(correction) - 1) return correction[NS_ARRAY_SIZE(correction) - 1];
    const float frac = tablePosition - (float)index;
    return correction[index] + (correction[index + 1] - correction[index]) * frac;
}

static inline float thicknessLevelTrim(float thickness) {
    // Coherent voices add by amplitude, while the wider swarm increasingly adds
    // by power. Keep the original root trim, then flatten the measured RMS
    // curve so Thickness changes size and motion without acting like a volume
    // control.
    const float lowTrim = kMinThicknessLevelTrim
        + (1.0f - kMinThicknessLevelTrim) * clamp01(thickness * kThicknessLevelTrimRise);
    const float highLift = 1.0f
        + kMaxThicknessLevelLift * smoothstep01((thickness - 0.25f) * 2.0f);
    return lowTrim * highLift * thicknessCurveCorrection(thickness);
}

struct VoiceState {
    float phase;
    float tri;
    float detuneNorm;
    float panNorm;
    float driftSin;
    float driftCos;
    float driftStep;
};

struct _thikOsc_DTC {
    VoiceState voices[kNumVoices];
    int renormCounter;
};

struct _thikOscAlgorithm : public _NT_algorithm {
    explicit _thikOscAlgorithm(_thikOsc_DTC* d)
        : dtc(d),
          pitchNote((float)kDefaultNote),
          fineCents((float)kDefaultFineTune),
          thicknessKnob((float)kDefaultThickness * 0.0001f),
          toneKnob((float)kDefaultTone * 0.0001f),
          midiChannel(kDefaultMidiChannel),
          midiNote(kDefaultNote),
          midiNoteActive(false) {}

    _thikOsc_DTC* dtc;
    float pitchNote;
    float fineCents;
    float thicknessKnob;
    float toneKnob;
    int midiChannel;
    int midiNote;
    bool midiNoteActive;
};

enum {
    kParamPitchCV,
    kParamToneCV,
    kParamThicknessCV,

    kParamOutputL,
    kParamOutputLMode,
    kParamOutputR,
    kParamOutputRMode,

    kParamPitch,
    kParamFineTune,
    kParamThickness,
    kParamTone,
    kParamMidiChannel,

    kNumParams
};

static char const* const midiChannelStrings[] = {
    "Omni",
    "1", "2", "3", "4",
    "5", "6", "7", "8",
    "9", "10", "11", "12",
    "13", "14", "15", "16",
    NULL,
};

static const _NT_parameter parameters[] = {
    NT_PARAMETER_CV_INPUT("Pitch CV", 0, 0)
    NT_PARAMETER_CV_INPUT("Tone CV", 0, 0)
    NT_PARAMETER_CV_INPUT("Thickness CV", 0, 0)

    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Output L", 1, 13)
    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Output R", 1, 14)

    { .name = "Pitch", .min = 0, .max = 127, .def = kDefaultNote, .unit = kNT_unitMIDINote, .scaling = 0, .enumStrings = NULL },
    { .name = "Fine Tune", .min = -100, .max = 100, .def = kDefaultFineTune, .unit = kNT_unitCents, .scaling = 0, .enumStrings = NULL },
    { .name = "Thickness", .min = 0, .max = 10000, .def = kDefaultThickness, .unit = kNT_unitPercent, .scaling = kNT_scaling100, .enumStrings = NULL },
    { .name = "Tone", .min = 0, .max = 10000, .def = kDefaultTone, .unit = kNT_unitPercent, .scaling = kNT_scaling100, .enumStrings = NULL },
    { .name = "MIDI Ch", .min = 0, .max = 16, .def = kDefaultMidiChannel, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = midiChannelStrings },
};

static const uint8_t pageMain[] = {
    kParamPitch,
    kParamFineTune,
    kParamThickness,
    kParamTone,
    kParamMidiChannel,
};

static const uint8_t pageCV[] = {
    kParamPitchCV,
    kParamToneCV,
    kParamThicknessCV,
};

static const uint8_t pageRouting[] = {
    kParamOutputL,
    kParamOutputLMode,
    kParamOutputR,
    kParamOutputRMode,
};

static const _NT_parameterPage pages[] = {
    { .name = "Main", .numParams = NS_ARRAY_SIZE(pageMain), .group = 0, .unused = { 0, 0 }, .params = pageMain },
    { .name = "CV", .numParams = NS_ARRAY_SIZE(pageCV), .group = 0, .unused = { 0, 0 }, .params = pageCV },
    { .name = "Routing", .numParams = NS_ARRAY_SIZE(pageRouting), .group = 0, .unused = { 0, 0 }, .params = pageRouting },
};

static const _NT_parameterPages parameterPages = {
    .numPages = NS_ARRAY_SIZE(pages),
    .pages = pages,
};

static void initialiseVoices(_thikOsc_DTC* d, float sampleRate) {
    static const float detunes[kNumVoices] = {
        0.0f, -0.18f, 0.18f, -0.38f, 0.38f, -0.62f, 0.62f, -1.0f, 1.0f,
    };
    static const float pans[kNumVoices] = {
        0.0f, -0.25f, 0.25f, -0.48f, 0.48f, -0.70f, 0.70f, -1.0f, 1.0f,
    };
    static const float driftRates[kNumVoices] = {
        0.071f, 0.109f, 0.053f, 0.167f, 0.091f, 0.231f, 0.137f, 0.193f, 0.251f,
    };

    if (sampleRate < 1.0f) sampleRate = 48000.0f;

    for (int v = 0; v < kNumVoices; ++v) {
        VoiceState& voice = d->voices[v];
        float phaseSeed = (0.137f + 0.271f * (float)v);
        phaseSeed -= floorf(phaseSeed);
        float driftPhase = 0.619f + 1.173f * (float)v;

        voice.phase = phaseSeed;
        voice.tri = 1.0f - 4.0f * fastAbs(phaseSeed - 0.5f);
        voice.detuneNorm = detunes[v];
        voice.panNorm = pans[v];
        voice.driftSin = (v == 0) ? 0.0f : sinf(driftPhase);
        voice.driftCos = (v == 0) ? 1.0f : cosf(driftPhase);
        voice.driftStep = (v == 0) ? 0.0f : (kTwoPi * driftRates[v] / sampleRate);
    }
    d->renormCounter = 0;
}

static void calculateRequirements(_NT_algorithmRequirements& req, const int32_t*) {
    req.numParameters = NS_ARRAY_SIZE(parameters);
    req.sram = sizeof(_thikOscAlgorithm);
    req.dram = 0;
    req.dtc = sizeof(_thikOsc_DTC);
    req.itc = 0;
}

static _NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs,
                                const _NT_algorithmRequirements&,
                                const int32_t*) {
    _thikOsc_DTC* d = new (ptrs.dtc) _thikOsc_DTC;
    _thikOscAlgorithm* alg = new (ptrs.sram) _thikOscAlgorithm(d);
    alg->parameters = parameters;
    alg->parameterPages = &parameterPages;
    initialiseVoices(d, (float)NT_globals.sampleRate);
    return alg;
}

static void syncCachedParams(_thikOscAlgorithm* alg) {
    // Preset loading restores alg->v, but hosts do not always call
    // parameterChanged() for every parameter afterwards. Keep these cached
    // floats synchronized from the authoritative parameter array.
    alg->pitchNote = (float)alg->v[kParamPitch];
    alg->fineCents = (float)alg->v[kParamFineTune];
    alg->thicknessKnob = clamp01((float)alg->v[kParamThickness] * 0.0001f);
    alg->toneKnob = clamp01((float)alg->v[kParamTone] * 0.0001f);
    alg->midiChannel = alg->v[kParamMidiChannel];
}

static void parameterChanged(_NT_algorithm* self, int) {
    syncCachedParams((_thikOscAlgorithm*)self);
}

static inline void advanceDrift(VoiceState& voice) {
    float oldSin = voice.driftSin;
    float oldCos = voice.driftCos;
    voice.driftSin = oldSin + voice.driftStep * oldCos;
    voice.driftCos = oldCos - voice.driftStep * oldSin;
}

static void renormaliseDrift(_thikOsc_DTC* d) {
    for (int v = 1; v < kNumVoices; ++v) {
        float mag = d->voices[v].driftSin * d->voices[v].driftSin
            + d->voices[v].driftCos * d->voices[v].driftCos;
        if (mag > 0.0001f) {
            float inv = 1.0f / sqrtf(mag);
            d->voices[v].driftSin *= inv;
            d->voices[v].driftCos *= inv;
        }
    }
}

static inline void maybeRenormaliseDrift(_thikOsc_DTC* d) {
    if (++d->renormCounter >= 1024) {
        // The drift LFOs use cheap sin/cos recurrence; renormalise
        // occasionally so roundoff never changes their modulation depth.
        d->renormCounter = 0;
        renormaliseDrift(d);
    }
}

static inline float activePitchNote(const _thikOscAlgorithm* alg) {
    return alg->midiNoteActive ? (float)alg->midiNote : alg->pitchNote;
}

static inline float processVoice(VoiceState& voice,
                                 float dt,
                                 float triGain,
                                 float sawGain,
                                 float toneNorm) {
    float phase = voice.phase;
    if (!(phase >= 0.0f && phase < 1.0f)) phase = 0.0f;
    voice.tri = finiteOr(voice.tri, 0.0f);

    const float edgeBlep = polyBlep(phase, dt);
    float saw = 2.0f * phase - 1.0f;
    saw -= edgeBlep;

    float square = (phase < 0.5f) ? 1.0f : -1.0f;
    square += edgeBlep;
    float halfPhase = phase + 0.5f;
    if (halfPhase >= 1.0f) halfPhase -= 1.0f;
    square -= polyBlep(halfPhase, dt);

    // Leaky integration turns the bandlimited square into a triangle-like
    // component. Scale the leak by phase advance so its settling and level are
    // invariant with pitch and sample rate rather than fixed in samples.
    voice.tri += square * dt * 4.0f;
    voice.tri *= 1.0f - kTriangleLeakPerCycle * dt;
    voice.tri = clampf(voice.tri, -1.25f, 1.25f);
    float tri = clampf(voice.tri, -1.0f, 1.0f);

    phase += dt;
    if (phase >= 1.0f) phase -= 1.0f;
    voice.phase = phase;

    return (tri * triGain + saw * sawGain) * toneNorm;
}

static void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    _thikOscAlgorithm* alg = (_thikOscAlgorithm*)self;
    _thikOsc_DTC* d = alg->dtc;
    const int numFrames = numFramesBy4 * 4;
    const float sampleRate = (NT_globals.sampleRate > 0) ? (float)NT_globals.sampleRate : 48000.0f;

    syncCachedParams(alg);

    const int pitchBus = alg->v[kParamPitchCV];
    const int toneBus = alg->v[kParamToneCV];
    const int thicknessBus = alg->v[kParamThicknessCV];
    const float* pitchCvFrames = (pitchBus > 0) ? (busFrames + (pitchBus - 1) * numFrames) : NULL;
    const float* toneCvFrames = (toneBus > 0) ? (busFrames + (toneBus - 1) * numFrames) : NULL;
    const float* thicknessCvFrames = (thicknessBus > 0) ? (busFrames + (thicknessBus - 1) * numFrames) : NULL;
    const bool hasPitchCv = pitchCvFrames != NULL;
    const bool hasMacroCv = (toneCvFrames != NULL) || (thicknessCvFrames != NULL);

    float* outL = busFrames + (alg->v[kParamOutputL] - 1) * numFrames;
    float* outR = busFrames + (alg->v[kParamOutputR] - 1) * numFrames;
    const bool replaceL = alg->v[kParamOutputLMode] != 0;
    const bool replaceR = alg->v[kParamOutputRMode] != 0;

    const float baseNote = activePitchNote(alg) + alg->fineCents * 0.01f;
    const float baseHzNoCv = 440.0f * fastExp2((baseNote - 69.0f) * (1.0f / 12.0f));
    const float invSampleRate = 1.0f / sampleRate;

    // Most patches leave Tone CV and Thickness CV unassigned; keep their
    // derived macro values out of the per-sample loop in that common case.
    if (!hasMacroCv) {
        const float tone = alg->toneKnob;
        const float thickness = alg->thicknessKnob;
        const float triGain = 0.92f - 0.55f * tone;
        const float sawGain = 0.10f + 0.82f * tone;
        const float toneNorm = toneLevelNorm(tone);

        const float detuneDepthCents = kMaxThicknessDetuneCents * thickness;
        const float driftDepthCents = kMaxThicknessDriftCents * thickness;
        const float sideWeight = thickness * (0.35f + 0.65f * thickness);
        const float stereoWidth = 0.10f + (kMaxStereoWidth - 0.10f) * thickness;
        const float weightSum = 1.0f + (float)(kNumVoices - 1) * sideWeight;
        const float gainDrive = kOutputGain * (2.0f / weightSum)
            * thicknessLevelTrim(thickness) * kSoftClipDrive;

        const float driftRatioDepth = driftDepthCents * kCentsToLinearRatioApprox;
        float staticDetuneMultipliers[kNumVoices];
        float leftWeights[kNumVoices];
        float rightWeights[kNumVoices];
        for (int v = 1; v < kNumVoices; ++v) {
            const VoiceState& voice = d->voices[v];
            staticDetuneMultipliers[v] = 1.0f
                + voice.detuneNorm * detuneDepthCents * kCentsToLinearRatioApprox;
            const float pan = voice.panNorm * stereoWidth;
            leftWeights[v] = sideWeight * 0.5f * (1.0f - pan);
            rightWeights[v] = sideWeight * 0.5f * (1.0f + pan);
        }

        for (int i = 0; i < numFrames; ++i) {
            const float baseHz = hasPitchCv
                ? (baseHzNoCv * fastExp2(clampf(pitchCvFrames[i], -8.0f, 8.0f)))
                : baseHzNoCv;

            float sumL = 0.0f;
            float sumR = 0.0f;

            // The root is always centered and has neither detune nor drift.
            VoiceState& rootVoice = d->voices[0];
            const float rootDt = clampf(baseHz * invSampleRate, 0.0f, kMaxPhaseIncrement);
            const float rootSample = processVoice(rootVoice, rootDt, triGain, sawGain, toneNorm);
            sumL += rootSample * 0.5f;
            sumR += rootSample * 0.5f;

            for (int v = 1; v < kNumVoices; ++v) {
                VoiceState& voice = d->voices[v];
                advanceDrift(voice);

                const float detuneMul = staticDetuneMultipliers[v]
                    + voice.driftSin * driftRatioDepth;
                const float dt = clampf(baseHz * detuneMul * invSampleRate, 0.0f, kMaxPhaseIncrement);
                const float sample = processVoice(voice, dt, triGain, sawGain, toneNorm);

                sumL += sample * leftWeights[v];
                sumR += sample * rightWeights[v];
            }

            const float sampleL = softClip(sumL * gainDrive) * kOutputVolts;
            const float sampleR = softClip(sumR * gainDrive) * kOutputVolts;

            outL[i] = replaceL ? sampleL : (outL[i] + sampleL);
            outR[i] = replaceR ? sampleR : (outR[i] + sampleR);

            maybeRenormaliseDrift(d);
        }
        return;
    }

    for (int i = 0; i < numFrames; ++i) {
        const float pitchCv = hasPitchCv ? clampf(pitchCvFrames[i], -8.0f, 8.0f) : 0.0f;
        const float toneCv = clampf(toneCvFrames ? toneCvFrames[i] : 0.0f, -5.0f, 5.0f);
        const float thicknessCv = clampf(thicknessCvFrames ? thicknessCvFrames[i] : 0.0f, -5.0f, 5.0f);
        const float tone = clamp01(alg->toneKnob + toneCv * kMacroCvScale);
        const float thickness = clamp01(alg->thicknessKnob + thicknessCv * kMacroCvScale);

        const float triGain = 0.92f - 0.55f * tone;
        const float sawGain = 0.10f + 0.82f * tone;
        const float toneNorm = toneLevelNorm(tone);

        const float baseHz = baseHzNoCv * fastExp2(pitchCv);
        const float detuneDepthCents = kMaxThicknessDetuneCents * thickness;
        const float driftDepthCents = kMaxThicknessDriftCents * thickness;
        const float sideWeight = thickness * (0.35f + 0.65f * thickness);
        const float stereoWidth = 0.10f + (kMaxStereoWidth - 0.10f) * thickness;
        const float weightSum = 1.0f + (float)(kNumVoices - 1) * sideWeight;

        float sumL = 0.0f;
        float sumR = 0.0f;

        // Voice 0 is the centered root voice: no detune, pan, or drift step.
        // Keep the side-voice loop branch-free without changing the swarm shape.
        VoiceState& rootVoice = d->voices[0];
        const float rootDt = clampf(baseHz * invSampleRate, 0.0f, kMaxPhaseIncrement);
        const float rootSample = processVoice(rootVoice, rootDt, triGain, sawGain, toneNorm);
        sumL += rootSample * 0.5f;
        sumR += rootSample * 0.5f;

        for (int v = 1; v < kNumVoices; ++v) {
            VoiceState& voice = d->voices[v];
            advanceDrift(voice);

            float cents = voice.detuneNorm * detuneDepthCents + voice.driftSin * driftDepthCents;
            float detuneMul = 1.0f + cents * kCentsToLinearRatioApprox;
            float dt = clampf(baseHz * detuneMul * invSampleRate, 0.0f, kMaxPhaseIncrement);
            float sample = processVoice(voice, dt, triGain, sawGain, toneNorm);

            float pan = voice.panNorm * stereoWidth;
            float leftGain = 0.5f * (1.0f - pan);
            float rightGain = 0.5f * (1.0f + pan);

            sumL += sample * sideWeight * leftGain;
            sumR += sample * sideWeight * rightGain;
        }

        float gain = kOutputGain * (2.0f / weightSum) * thicknessLevelTrim(thickness);
        float sampleL = softClip(sumL * gain * kSoftClipDrive) * kOutputVolts;
        float sampleR = softClip(sumR * gain * kSoftClipDrive) * kOutputVolts;

        outL[i] = replaceL ? sampleL : (outL[i] + sampleL);
        outR[i] = replaceR ? sampleR : (outR[i] + sampleR);

        maybeRenormaliseDrift(d);
    }
}

static void midiMessage(_NT_algorithm* self, uint8_t byte0, uint8_t byte1, uint8_t byte2) {
    _thikOscAlgorithm* alg = (_thikOscAlgorithm*)self;
    syncCachedParams(alg);

    const uint8_t type = byte0 & 0xf0;
    if (type != 0x80 && type != 0x90) return;

    const int channel = (byte0 & 0x0f) + 1;
    if (alg->midiChannel != 0 && alg->midiChannel != channel) return;

    const int note = (byte1 <= 127) ? byte1 : 127;
    const bool noteOn = (type == 0x90 && byte2 != 0);
    if (noteOn) {
        alg->midiNote = note;
        alg->midiNoteActive = true;
    } else if (alg->midiNoteActive && alg->midiNote == note) {
        alg->midiNoteActive = false;
    }
}

static const _NT_factory factory = {
    .guid = NT_MULTICHAR('T', 'h', 'I', 'k'),
    .name = "Thik Oscillator",
    .description = "Minimal organic stereo unison oscillator",
    .numSpecifications = 0,
    .specifications = NULL,
    .calculateStaticRequirements = NULL,
    .initialise = NULL,
    .calculateRequirements = calculateRequirements,
    .construct = construct,
    .parameterChanged = parameterChanged,
    .step = step,
    .draw = NULL,
    .midiRealtime = NULL,
    .midiMessage = midiMessage,
    .tags = kNT_tagInstrument,
    .hasCustomUi = NULL,
    .customUi = NULL,
    .setupUi = NULL,
    .serialise = NULL,
    .deserialise = NULL,
    .midiSysEx = NULL,
    .parameterUiPrefix = NULL,
    .parameterString = NULL,
};

uintptr_t pluginEntry(_NT_selector selector, uint32_t data) {
    switch (selector) {
    case kNT_selector_version:
        return kNT_apiVersionCurrent;
    case kNT_selector_numFactories:
        return 1;
    case kNT_selector_factoryInfo:
        return (uintptr_t)((data == 0) ? &factory : NULL);
    }
    return 0;
}
