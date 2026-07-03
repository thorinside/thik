# Starting implementation constants

The first implementation will use curated starting constants for the organic swarm, to be tuned by ear after the plugin runs. The initial voice count is 9; max Thickness detune is about ±14 cents; drift is slow and independent per voice at roughly 0.05–0.25 Hz; stereo spread alternates voices across the field; Tone crossfades from mostly triangle/darker to mostly saw/brighter; and Thickness increases detune, drift depth, side-voice level, and stereo width together.

**Decision**: use these constants as implementation defaults, not exposed controls.

**Consequence**: the first build should prioritize a coherent curated sound over exhaustive parameterization. Changes after listening should tune constants, not add new controls by default.
