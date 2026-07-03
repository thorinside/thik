# Always stereo output

The oscillator will be a stereo instrument by design, with separate left and right output parameters. Stereo spread is part of the lush swarm identity, so the plugin will not expose a mono/stereo boolean or mode switch.

**Considered options**: mono output, always stereo output, mono/stereo selectable, dual mono plus stereo pair.

**Decision**: always stereo output.

**Consequence**: the implementation should provide two output parameters, such as `Output L` and `Output R`, and keep the swarm controlled enough that summing remains musically pitched and usable.
