# FM1JUNO documentation map

This file is the entry point for technical work. The project has accumulated early design drafts and newer evidence-backed specifications; use the authority order below rather than choosing a document by filename alone.

## 1. Current authority order

For implementation decisions, use this order:

1. **Newest specialized research/spec document for the block in question.**
2. [`PHASE0_REFERENCE_ARCHITECTURE.md`](PHASE0_REFERENCE_ARCHITECTURE.md) — compact current integration/index.
3. [`research/PHASE0_CHECKLIST.md`](research/PHASE0_CHECKLIST.md) — Phase-0 gate and open items.
4. [`research/PHASE0_MEASUREMENT_PLAN.md`](research/PHASE0_MEASUREMENT_PLAN.md) — how unresolved physical constants are closed.
5. [`research/FM1_TARGET_BUDGET.md`](research/FM1_TARGET_BUDGET.md) — current FM-1 deployment/CPU/memory contract.
6. [`PHASE1_REFERENCE_ENGINE_PLAN.md`](PHASE1_REFERENCE_ENGINE_PLAN.md) — implementation sequence and test architecture.
7. Older baseline files only for historical context.

If two documents conflict, do not average them. The newer specialized document wins unless its status explicitly says hypothesis/open.

## 2. Primary target

Current project priority is **JUNO-106**.

JUNO-60 is used selectively to:

- expose shared IR3109/BA662 circuit ancestry hidden inside the 80017A;
- suggest measurement methods;
- inform later explicit 60/DEEP alternatives.

Strict `MODEL_106` must not inherit 60-specific clock, IR3R01 envelope, HPF loading or chorus constants without independent 106 evidence.

## 3. Current block specifications

### Control / firmware behavior

- [`research/JUNO106_ASSIGNER_RETRIGGER.md`](research/JUNO106_ASSIGNER_RETRIGGER.md)
- [`research/JUNO106_SOLO_UNISON_TIMING.md`](research/JUNO106_SOLO_UNISON_TIMING.md)
- [`research/JUNO106_ROM_CONTROL.md`](research/JUNO106_ROM_CONTROL.md)
- [`research/JUNO106_LFO_PORTAMENTO.md`](research/JUNO106_LFO_PORTAMENTO.md)
- [`research/JUNO106_LFO_RATE_LAW.md`](research/JUNO106_LFO_RATE_LAW.md)
- [`research/JUNO106_LFO_TRIGGER_DELAY.md`](research/JUNO106_LFO_TRIGGER_DELAY.md)
- [`research/JUNO106_PITCH_DCOCV.md`](research/JUNO106_PITCH_DCOCV.md)
- [`research/JUNO106_BENDER.md`](research/JUNO106_BENDER.md)

### DCO / source path

- [`research/JUNO106_MC5534_RAMP.md`](research/JUNO106_MC5534_RAMP.md)
- [`research/JUNO106_SOURCE_MIXER_NOISE.md`](research/JUNO106_SOURCE_MIXER_NOISE.md)

### VCF / VCA

- [`research/JUNO106_VCF_CONTROL.md`](research/JUNO106_VCF_CONTROL.md)
- [`research/JUNO106_80017A_TOPOLOGY.md`](research/JUNO106_80017A_TOPOLOGY.md)

### Global output

- [`research/JUNO106_HPF_GLOBAL_OUTPUT.md`](research/JUNO106_HPF_GLOBAL_OUTPUT.md)

### Chorus — Phase-0 frozen architecture

- [`research/JUNO106_CHORUS.md`](research/JUNO106_CHORUS.md) — authoritative chorus status/index; state `CHORUS_PHASE0_FROZEN`.
- [`research/JUNO106_CHORUS_REFERENCE_MODEL.md`](research/JUNO106_CHORUS_REFERENCE_MODEL.md) — implementation and closure contract.
- [`research/JUNO106_CHORUS_CLOCK.md`](research/JUNO106_CHORUS_CLOCK.md) — MN3101/MN3009 clock and exact delay law.
- [`research/JUNO106_CHORUS_FILTERS_MIX.md`](research/JUNO106_CHORUS_FILTERS_MIX.md) — original BBD audio path, bias, exact IC6 dry/wet summer and output-stage boundary.
- [`research/JUNO106_CHORUS_PRIMARY_SCHEMATIC.md`](research/JUNO106_CHORUS_PRIMARY_SCHEMATIC.md) — direct Roland jack-board schematic transcription/evidence boundary.
- [`research/data/juno106_chorus_component_evidence.csv`](research/data/juno106_chorus_component_evidence.csv) — per-component evidence state.
- [`research/data/juno106_chorus_calibration_v0.yaml`](research/data/juno106_chorus_calibration_v0.yaml) — centralized strict calibration / `MEASURE_PENDING` values.
- [`research/netlists/juno106_chorus_output_mix_reference.cir`](research/netlists/juno106_chorus_output_mix_reference.cir) — verified IC6 summer subcircuit.
- [`research/netlists/README.md`](research/netlists/README.md) — netlist verification rules.

Remaining original-hardware captures are calibration backlog and must replace `MEASURE_PENDING` values before claiming final hardware-calibrated fidelity. They do not reopen the frozen chorus architecture unless measurements contradict primary evidence.

### Evidence / implementation boundary

- [`research/SOURCE_INDEX.md`](research/SOURCE_INDEX.md)
- [`research/JUNO106_EVIDENCE.md`](research/JUNO106_EVIDENCE.md)
- [`research/IMPLEMENTATION_REFERENCES.md`](research/IMPLEMENTATION_REFERENCES.md)

## 4. Historical baseline documents

These files are retained because they show why the architecture evolved, but they are **not the current implementation authority** where newer specs disagree:

- [`ARCHITECTURE.md`](ARCHITECTURE.md)
- [`PLAN.md`](PLAN.md)
- [`REFERENCES.md`](REFERENCES.md)
- [`CIRCUIT_MODEL_SPEC_V0.md`](CIRCUIT_MODEL_SPEC_V0.md)

In particular, old JUNO-60 chorus numbers in baseline documents must never be reused as strict 106 constants. The current chorus specs deliberately removed that shortcut.

## 5. Evidence labels

Research documents use labels such as:

```text
PRIMARY_CONFIRMED
ROM_DERIVED
SCHEMATIC_CONFIRMED
SCHEMATIC_CROSS_CHECK
HARDWARE_MEASURED
DERIVED
MEASURE_PENDING
DEEP_ONLY
```

A numerical value without a frozen evidence status is not permission to hard-code it into strict DSP.

## 6. Branch/PR policy

Current Phase-0 research lives on:

```text
research/phase0-juno106-control
```

and is reviewed through draft PR #2.

When Phase-1 code starts, cut a separate branch from the reviewed research state:

```text
phase1/reference-engine
```

Phase-0 evidence may continue to evolve, but changes enter Phase 1 through explicit spec/calibration updates.

Do not mix experimental target optimization commits into the research branch.

## 7. Implementation policy

The implementation sequence is deliberately:

```text
evidence
 -> explicit behavior/spec
 -> high-precision reference
 -> deterministic tests
 -> target approximation
 -> FM-1 profiling
```

not:

```text
FM-1 code
 -> sounds plausible
 -> retroactively call it a model
```

This distinction is the central engineering rule of FM1JUNO.
