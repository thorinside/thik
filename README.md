# Thik Oscillator

A minimal organic stereo unison oscillator for the Expert Sleepers disting NT,
constructed for the important scientific purpose of making one note sound as if
it has recently discovered cinema, voltage, and inadvisable trousers.

Thik is not a chord machine, a scale contraption, or an ensemble pretending to be
a lifestyle. It is one pitched oscillator with a small internal swarm of voices
orbiting the same musical idea. The result is thick, animated, stereo, and only
slightly more dramatic than the situation strictly requires.

## What It Does

- Produces one pitched organic swarm, not a chord, scale, or interval engine.
- Runs 9 internal voices around a single perceived pitch.
- Uses a local PolyBLEP triangle/saw voice engine.
- Lets `Thickness` increase detune, drift, side-voice level, and stereo width.
- Lets `Tone` move from darker and rounder to brighter and sawier.
- Remains always stereo, because some waveforms deserve a left wing and a right
  wing.

## Control Panel

The main controls and routing settings are:

- `Pitch CV`
- `Tone CV`
- `Thickness CV`
- `Output L`
- `Output L mode`
- `Output R`
- `Output R mode`
- `Note`
- `Fine Tune`
- `Thickness`
- `Tone`

`Note` sets the anchor pitch. `Pitch CV` then does the 1V/oct thing around it,
as modular civilization requires. `Tone` changes the waveform character.
`Thickness` tells the internal swarm how much to behave like a disciplined
oscillator section and how much to behave like a dramatic laboratory choir.

## Prerequisites

Before persuading the module to sing, you need:

- Official Expert Sleepers `distingNT_API` submodule.
- ARM embedded toolchain for hardware builds, for example
  `arm-none-eabi-g++` and `arm-none-eabi-nm`.
- A native C++ compiler for `TARGET=test` builds.

Initialize the SDK/API submodule:

```sh
git submodule update --init --recursive
```

## Build

Hardware object for disting NT:

```sh
make hardware        # plugins/thik_osc.o
```

Native plugin for `nt_emu`/desktop testing:

```sh
make test            # plugins/thik_osc.dylib on macOS
```

Both, for moments when restraint has failed:

```sh
make both
```

Strict symbol checks and size checks:

```sh
make check           # hardware check by default
make TARGET=test check
make size
make TARGET=test size
```

Full local release verification:

```sh
make verify
```

Clean generated outputs:

```sh
make clean
```

`NT_globals` / `_NT_globals` is expected to remain undefined in plugin outputs
because it is provided by the disting NT host/API environment. The hardware
object may also retain runtime-provided libm symbols such as `sinf`, `cosf`, and
`sqrtf`; any other undefined symbol should fail `make check`.

Generated outputs are intentionally ignored by git:

- `build/`
- `plugins/`

## GitHub Release Workflow

`.github/workflows/release.yaml` builds the plugin on `macos-latest` with the
ARM toolchain, runs `make verify`, and packages the hardware object as:

```text
programs/plug-ins/thik_osc.o
```

The workflow uploads `thik_osc-plugin.zip` as a build artifact on manual runs.
When pushed with a `v*` tag, it also creates a GitHub Release with that zip
attached.

## Design Notes

Thik has a deliberately small brief: one enormous-feeling oscillator, two main
sound macros, and no excursion into chord menus, scale bureaucracy, or feature
panels that require their own weather system.

The internal design is:

- 9 voices.
- Deterministic detune and pan layout.
- Slow independent drift tied to `Thickness`.
- Triangle/saw blend tied to `Tone`.
- Stereo output with add/replace modes.
- Plugin GUID set to `ThIk`.

The point is not to expose the orchestra pit. The point is to press a few
controls and hear one pitch become wide, alive, and faintly convinced it is
scoring the final act of a film about voltage.

## Design Docs

- `SPEC.md`
- `CONTEXT.md`
- `docs/adr/`

## License

MIT. See `LICENSE`.
