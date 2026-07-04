# Disting NT Thik Oscillator Specification

## Status

Implemented. Builds for hardware and native test targets; pending hands-on nt_emu and hardware listening validation.

## Goal

Build a minimal Expert Sleepers disting NT custom C++ plugin that behaves as one very thick, pitched oscillator. It should sound like a small organic bank of oscillators while remaining simple: one perceived pitch, stereo output, and two main sound macros.

## Product Boundary

This is a **minimal organic unison oscillator**.

It is not:

- a chord oscillator
- a scale/interval oscillator
- an additive organ bank
- a general multi-oscillator editor
- a port or variant of `nt_enosc`

Reject feature creep toward scales, chords, interval modes, freeze, warp, twist, voice-count editing, or ensemble-engine behavior unless the project is deliberately re-scoped.

## User-Facing Parameters

Recommended order:

1. `Pitch CV` — CV input, 1V/oct around `Note` + `Fine Tune`.
2. `Tone CV` — CV input for `Tone` modulation.
3. `Thickness CV` — CV input for `Thickness` modulation.
4. `Output L` — audio output with output mode.
5. `Output R` — audio output with output mode.
6. `Note` — base pitch as MIDI note.
7. `Fine Tune` — small pitch offset.
8. `Thickness` — macro controlling swarm thickness, scaled at 0.01% resolution.
9. `Tone` — macro controlling darker/rounder to brighter/sawier timbre, scaled at 0.01% resolution.

No mono/stereo boolean. No separate Drift, Voice Count, Detune, Spread, Shape, Scale, or Chord controls.

## Sound Design

- Sonic model: one pitched organic swarm.
- Core timbre: triangle/saw blend.
- Swarm size: medium, pitch-focused.
- Output: always stereo.
- Tone: macro from mostly triangle/darker to mostly saw/brighter.
- Thickness: macro increasing:
  - side-voice prominence
  - detune width
  - drift depth
  - stereo width
- Drift: tied to Thickness; no separate Drift parameter.

## Initial Implementation Constants

Tune by ear after the first build, but start with:

- Voice count: `9`
- Maximum detune at full Thickness: approximately `±14 cents`
- Drift rates: slow, independent per voice, roughly `0.05–0.25 Hz`
- Stereo spread: deterministic alternating voice pan positions across L/R, not random hard-panning
- Tone mapping: crossfade from mostly triangle/darker to mostly saw/brighter
- Thickness mapping: coordinated increase of detune, drift depth, side-voice level, and stereo width

These are implementation defaults, not exposed controls.

## DSP Architecture

Use a local lightweight oscillator engine:

- PolyBLEP saw generation per voice.
- Derived triangle component per voice.
- Per-voice phase, base detune offset, slow drift phase/rate, and pan position.
- Sum voices into stereo accumulators.
- Normalize output to avoid clipping as Thickness increases.
- Apply gentle output gain compensation if needed so low/high Thickness feel comparable.

### Pitch

Pitch should be computed from:

- `Note` as the anchor MIDI note.
- `Fine Tune` as a small semitone or cent offset.
- `Pitch CV` as 1V/oct when patched.

The oscillator must remain clearly pitched at high Thickness.

### CV Macro Handling

`Tone CV` and `Thickness CV` are direct CV inputs for immediate modular patchability even though NT supports generic CV mapping. Clamp resulting macro values to valid ranges.

### Output Handling

Use two audio output parameters with output modes, e.g. `Output L` and `Output R`. Honor replace/add mode for each output.

## disting NT Implementation Notes

Follow disting NT plugin conventions:

- Implement `pluginEntry`, `calculateRequirements`, `construct`, and `step`.
- Use `numFrames = numFramesBy4 * 4` before indexing buffers.
- Audio/CV bus parameters are 1-based; subtract 1 for `busFrames` indexing.
- Set `alg->parameters` in `construct`.
- Use `parameterChanged` for cached values where useful.
- Keep hot per-voice state in DTC.
- Use a unique GUID containing at least one capital letter.
- Tag as `kNT_tagInstrument`.

## Acceptance Criteria

- Builds as a disting NT custom plugin object.
- Exposes only the approved parameters.
- Produces a stereo pitched oscillator sound without requiring input audio.
- `Note` produces sound standalone without Pitch CV.
- `Pitch CV` tracks musically as 1V/oct around the selected note.
- `Thickness` audibly increases swarm thickness while preserving pitch.
- `Tone` audibly moves between darker/rounder and brighter/sawier variants of the same identity.
- `Tone CV` and `Thickness CV` modulate their corresponding macros.
- Stereo output feels wide but remains usable when summed.
- No scale/chord/interval/ensemble modes are present.

## ADRs

- `docs/adr/0001-organic-swarm-not-general-bank.md`
- `docs/adr/0002-triangle-saw-core-timbre.md`
- `docs/adr/0003-medium-swarm-pitch-focused.md`
- `docs/adr/0004-thickness-as-single-swarm-macro.md`
- `docs/adr/0005-always-stereo-output.md`
- `docs/adr/0006-minimal-pitched-control-set.md`
- `docs/adr/0007-tone-as-single-timbre-macro.md`
- `docs/adr/0008-direct-cv-inputs-for-tone-and-thickness.md`
- `docs/adr/0009-drift-tied-to-thickness.md`
- `docs/adr/0010-polyblep-saw-derived-triangle-core.md`
- `docs/adr/0011-minimal-organic-unison-not-ensemble-oscillator.md`
- `docs/adr/0012-starting-implementation-constants.md`
