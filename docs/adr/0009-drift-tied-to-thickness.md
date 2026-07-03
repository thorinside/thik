# Drift tied to Thickness

The oscillator's organic drift will be tied to the `Thickness` macro. Low Thickness should sound more stable and pitch-centered; higher Thickness should increase detune width and subtle voice movement so the swarm breathes more.

**Considered options**: always-on subtle drift, drift tied to Thickness, separate Drift control, static detuned voices.

**Decision**: tie drift to Thickness.

**Consequence**: there is no separate Drift parameter. The implementation should keep drift bounded so the oscillator remains clearly pitched even at high Thickness.
