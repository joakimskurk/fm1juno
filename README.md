# FM1JUNO

Circuit-informed JUNO-106 reference engine and M-VAVE FM-1 deployment project.

## Current state

**Phase 0 architecture is complete.**

Primary handoff:

- [`docs/PHASE0_COMPLETE.md`](docs/PHASE0_COMPLETE.md)
- [`docs/PHASE0_REFERENCE_ARCHITECTURE.md`](docs/PHASE0_REFERENCE_ARCHITECTURE.md)
- [`docs/research/PHASE0_CHECKLIST.md`](docs/research/PHASE0_CHECKLIST.md)
- [`docs/PHASE1_REFERENCE_ENGINE_PLAN.md`](docs/PHASE1_REFERENCE_ENGINE_PLAN.md)

Documentation map:

- [`docs/README.md`](docs/README.md)

## Frozen strict-106 blocks

```text
CONTROL_TIMING_PHASE0_ARCHITECTURE_FROZEN
MC5534_PHASE0_ARCHITECTURE_FROZEN
SOURCE_MIXER_PHASE0_ARCHITECTURE_FROZEN
80017A_PHASE0_ARCHITECTURE_FROZEN
GLOBAL_OUTPUT_PHASE0_ARCHITECTURE_FROZEN
CHORUS_PHASE0_FROZEN
FM1_TARGET_PHASE0_ARCHITECTURE_FROZEN
```

The remaining unknown original-hardware values are explicit calibration/profile data with measurement routes. They are not hidden generic-VA assumptions.

## Next branch

Implementation belongs on:

```text
phase1/reference-engine
```

The first Phase-1 milestone is a deterministic host/reference scaffold plus the exact digital/control core and `Timer106`, before analogue fidelity DSP is added.

## Core engineering rule

```text
evidence
 -> explicit specification
 -> high-precision reference
 -> deterministic tests
 -> measured target approximation
 -> FM-1 profiling
```

The FM-1 build is a reduction of the reference model, never the source of truth.
