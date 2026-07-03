# PolyBLEP saw with derived triangle core

The oscillator voices will use a PolyBLEP saw core with a derived triangle component. This supports the triangle/saw timbre identity while reducing aliasing compared with naive phase oscillators, without requiring wavetable infrastructure or a larger imported oscillator engine.

**Considered options**: naive phase oscillators with soft tone shaping, PolyBLEP saw with derived triangle, wavetable oscillator, porting an existing oscillator engine.

**Decision**: PolyBLEP saw with derived triangle.

**Consequence**: the implementation should keep the voice engine lightweight and local to this plugin, with anti-aliased saw generation and triangle derivation suitable for a pitched oscillator.
