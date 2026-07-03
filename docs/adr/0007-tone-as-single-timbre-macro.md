# Tone as a single timbre macro

The oscillator will expose a single Tone control for moving the triangle/saw swarm from darker and rounder to brighter and sawier. This gives the curated oscillator useful range without turning it into a generic waveform or synthesis editor.

**Considered options**: fixed triangle/saw blend, Tone macro, explicit Triangle/Saw blend, Shape plus Thickness.

**Decision**: single Tone macro.

**Consequence**: Tone should preserve the triangle/saw identity and avoid adding waveform selection or unrelated timbre modes.
