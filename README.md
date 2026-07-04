# Thik Oscillator

A minimal organic stereo unison oscillator for the Expert Sleepers disting NT.

## Sound

- One pitched organic swarm, not a chord/scale/ensemble oscillator.
- 9 internal voices.
- Triangle/saw timbre via a local PolyBLEP voice engine.
- `Thickness` increases detune, drift, side-voice level, and stereo width.
- `Tone` moves from darker/rounder to brighter/sawier.

## Parameters

- `Pitch CV`
- `Tone CV`
- `Thickness CV`
- `Output L`
- `Output R`
- `Note`
- `Fine Tune`
- `Thickness`
- `Tone`

## Prerequisites

- Official Expert Sleepers `distingNT_API` submodule.
- ARM embedded toolchain for hardware builds, e.g. `arm-none-eabi-g++` and `arm-none-eabi-nm`.
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

Both:

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

`NT_globals` / `_NT_globals` is expected to remain undefined in plugin outputs because it is provided by the disting NT host/API environment. The hardware object may also retain runtime-provided libm symbols such as `sinf`, `cosf`, and `sqrtf`; any other undefined symbol should fail `make check`.

Generated outputs are intentionally ignored by git:

- `build/`
- `plugins/`

## Design Docs

- `SPEC.md`
- `CONTEXT.md`
- `docs/adr/`
