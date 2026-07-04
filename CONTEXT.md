# Disting NT Thik Oscillator

A minimal disting NT oscillator plugin whose purpose is to produce one thick, pitched oscillator sound.

## Language

**Thik oscillator**:
A single-pitch oscillator whose sound is made thick by a swarm of internally detuned and drifting voices. It should feel like one huge oscillator, not a managed set of separate oscillators.

**Oscillator bank**:
For this project, an internal organic swarm of voices around one pitch. _Avoid_: chord oscillator, additive organ bank, general multi-oscillator instrument.

**Triangle/saw blend**:
The core waveform identity: warmer than pure saw, richer than pure triangle. _Avoid_: full waveform menu.

**Medium swarm**:
A voice cluster that is clearly thick and alive while preserving a strong perceived pitch. _Avoid_: pitch fog, chord cloud, huge ensemble wash.

**Thickness**:
A single macro control that increases the apparent density of the swarm by coordinating voice prominence, detune width, and organic movement. _Avoid_: separate voice-count editing.

**Always stereo**:
The oscillator presents left and right outputs as part of its identity; stereo spread is not an optional mode. _Avoid_: mono/stereo boolean.

**Pitch CV**:
The external pitch control input, interpreted as 1V/oct around the selected note and fine tune. _Avoid_: exposing a V/Oct amount control unless later required.

**Note**:
The base musical pitch used when no pitch CV is patched, and the anchor pitch when CV is patched. _Avoid_: raw frequency as the main pitch control.

**Fine Tune**:
A small pitch offset around the selected note for calibration or musical beating.

**Tone**:
A single timbre macro that moves the triangle/saw swarm from darker and rounder to brighter and sawier. _Avoid_: waveform menu, unrelated shape modes.

**Tone CV**:
A direct CV input for modulating Tone, included for immediate patchability despite NT's generic CV mapping support.

**Thickness CV**:
A direct CV input for modulating Thickness, included for immediate patchability despite NT's generic CV mapping support.

**Drift**:
Subtle per-voice movement that is part of Thickness, not a separate control. Higher Thickness means more breathing movement while preserving pitch focus.

**PolyBLEP core**:
The local oscillator implementation approach: anti-aliased saw generation with a derived triangle component. _Avoid_: wavetable infrastructure or imported ensemble engines.

**Minimal organic unison**:
The product boundary: one pitched organic swarm controlled by Tone and Thickness. _Avoid_: scales, chords, interval modes, freeze, warp, twist, or ensemble-engine behavior.
