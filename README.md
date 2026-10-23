# FM1JUNO

Circuit-informed JUNO-106 virtual-analog instrument for the M-VAVE FM-1, with selective JUNO-60 circuit ancestry used only where it is technically justified.

## Status

**Phase 0 research/specification is advanced. Synth implementation has intentionally not started yet.**

The goal is not a generic "Juno-ish" subtractive synth. `MODEL_106` is the primary fidelity target and is being reconstructed from:

- Roland service/owner documentation;
- JUNO-106 assigner/module firmware behavior;
- 8253 timer semantics;
- MC5534 DCO/ramp circuitry;
- A1QH80017A / IR3109 / BA662 topology;
- 106-specific HPF/output and dual-MN3009 chorus circuitry;
- explicit hardware-measurement plans for constants that public evidence cannot close safely.

JUNO-60 remains a secondary reference for shared topology, measurement methods and future explicit DEEP/60-derived alternatives. Strict 106 behavior never silently inherits 60-specific clock, IR3R01 envelope, HPF loading or chorus constants.

## Current strict signal path

```text
keyboard / MIDI
 -> authentic 106 allocator / control firmware model
 -> six 8253 Mode-3 timer voices
 -> six MC5534-derived saw / PWM / sub paths
 + one common physical noise source
 -> six source mixers
 -> six A1QH80017A VCF/VCA voices
 -> voice sum
 -> global HPF 0/1/2/3
 -> global output VCA
 -> dual-MN3009 / dual-MN3101 stereo chorus
 -> stereo out
```

Authentic polyphony is six physical voices. Normal seventh-note voice stealing is deliberately **not** part of strict 106 behavior.

## Engineering approach

FM1JUNO separates three things that are often collapsed in VA emulations:

1. **original digital/control behavior** — implemented from the recovered integer/timer semantics;
2. **high-precision physical reference DSP** — observable and calibration-driven on desktop;
3. **FM-1 target DSP** — fixed-point/realtime reductions continuously compared against the reference.

Unknown analogue constants are marked `MEASURE_PENDING` with an explicit physical measurement test. They are not hidden as magic numbers in the DSP.

## FM-1 target

The core is outer-block-size independent.

Current platforms expose different wrappers:

- M-VAVE official V14 SDK: **44.1 kHz / 128 stereo frames**;
- current Dinghy/Felucca custom firmware: **44.1 kHz / 256 stereo frames**.

The first standalone FM1JUNO deployment is planned around Dinghy/Felucca because it exposes the complete custom audio callback and recovery/host tooling. The official M-VAVE SDK remains an authoritative current hardware/API reference.

FM-1 resource and realtime targets are documented in `docs/research/FM1_TARGET_BUDGET.md`.

## Documentation

Start here:

- [`docs/README.md`](docs/README.md) — authoritative documentation map and precedence rules.
- [`docs/PHASE0_REFERENCE_ARCHITECTURE.md`](docs/PHASE0_REFERENCE_ARCHITECTURE.md) — current integrated 106 architecture.
- [`docs/research/PHASE0_CHECKLIST.md`](docs/research/PHASE0_CHECKLIST.md) — Phase-0 gate/open items.
- [`docs/research/PHASE0_MEASUREMENT_PLAN.md`](docs/research/PHASE0_MEASUREMENT_PLAN.md) — exact plan for unresolved physical measurements.
- [`docs/research/FM1_TARGET_BUDGET.md`](docs/research/FM1_TARGET_BUDGET.md) — FM-1 CPU/memory/platform strategy.
- [`docs/PHASE1_REFERENCE_ENGINE_PLAN.md`](docs/PHASE1_REFERENCE_ENGINE_PLAN.md) — implementation/test sequence before first DSP commit.

Older `ARCHITECTURE.md`, `PLAN.md`, `REFERENCES.md` and `CIRCUIT_MODEL_SPEC_V0.md` are retained as historical baselines; newer specialized documents take precedence.

## Ground rule

Fidelity before feature count.

DEEP features may later expose circuit-derived variation, aging, hybrid 60/106 blocks and additional modulation, but strict `MODEL_106` must remain reproducible, testable and separable from those extensions.
