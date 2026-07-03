# Minimal pitched control set

The oscillator will expose a minimal control set that works both standalone and under modular pitch control: `Pitch CV`, `Note`, `Fine Tune`, `Thickness`, `Output L`, and `Output R`. This keeps the plugin focused while still making it practical as a pitched disting NT oscillator.

**Considered options**: Pitch CV plus Tune only, Pitch CV plus V/Oct amount plus Tune, MIDI Note plus Pitch CV plus Tune, Frequency Hz plus Pitch CV.

**Decision**: `Pitch CV`, `Note`, `Fine Tune`, `Thickness`, `Output L`, and `Output R`.

**Consequence**: the plugin should avoid extra oscillator-bank controls unless a later decision explicitly adds them.
