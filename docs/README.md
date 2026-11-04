# FM1JUNO documentation map

This file is the entry point for technical work.

## 1. Authority order

Use this order for implementation decisions:

1. newest block-specific reference model / closure record;
2. [`PHASE0_REFERENCE_ARCHITECTURE.md`](PHASE0_REFERENCE_ARCHITECTURE.md);
3. [`research/PHASE0_CHECKLIST.md`](research/PHASE0_CHECKLIST.md);
4. [`research/PHASE0_MEASUREMENT_PLAN.md`](research/PHASE0_MEASUREMENT_PLAN.md);
5. [`research/FM1_TARGET_BUDGET.md`](research/FM1_TARGET_BUDGET.md) plus target closure;
6. [`PHASE1_REFERENCE_ENGINE_PLAN.md`](PHASE1_REFERENCE_ENGINE_PLAN.md);
7. older baseline drafts only for historical context.

If documents conflict, do not average them. The newer specialized document wins unless it explicitly labels a statement as hypothesis/open.

## 2. Primary model policy

Primary target is **JUNO-106**.

JUNO-60 evidence is used only when topology is demonstrably shared or as a measurement/modeling aid. Strict `MODEL_106` must not silently inherit 60-specific envelope, DCO-clock, HPF/output, MC5534 reset or chorus constants.

## 3. Phase-0 state

All major implementation graphs are now architecture-frozen:

```text
CONTROL_TIMING_PHASE0_ARCHITECTURE_FROZEN
MC5534_PHASE0_ARCHITECTURE_FROZEN
SOURCE_MIXER_PHASE0_ARCHITECTURE_FROZEN
80017A_PHASE0_ARCHITECTURE_FROZEN
GLOBAL_OUTPUT_PHASE0_ARCHITECTURE_FROZEN
CHORUS_PHASE0_FROZEN
FM1_TARGET_PHASE0_ARCHITECTURE_FROZEN
```

Remaining unknown vintage-unit numbers are calibration/profile data with named fields and explicit verification routes. They are not permission to invent generic VA constants.

## 4. Control / firmware

Behavioral sources:

- [`research/JUNO106_ASSIGNER_RETRIGGER.md`](research/JUNO106_ASSIGNER_RETRIGGER.md)
- [`research/JUNO106_SOLO_UNISON_TIMING.md`](research/JUNO106_SOLO_UNISON_TIMING.md)
- [`research/JUNO106_ROM_CONTROL.md`](research/JUNO106_ROM_CONTROL.md)
- [`research/JUNO106_LFO_PORTAMENTO.md`](research/JUNO106_LFO_PORTAMENTO.md)
- [`research/JUNO106_LFO_RATE_LAW.md`](research/JUNO106_LFO_RATE_LAW.md)
- [`research/JUNO106_LFO_TRIGGER_DELAY.md`](research/JUNO106_LFO_TRIGGER_DELAY.md)
- [`research/JUNO106_PITCH_DCOCV.md`](research/JUNO106_PITCH_DCOCV.md)
- [`research/JUNO106_BENDER.md`](research/JUNO106_BENDER.md)
- [`research/JUNO106_VCF_CONTROL.md`](research/JUNO106_VCF_CONTROL.md)

### Control timing / DAC-S&H

- [`research/JUNO106_CONTROL_SCHEDULER_REFERENCE_MODEL.md`](research/JUNO106_CONTROL_SCHEDULER_REFERENCE_MODEL.md) — authoritative scheduler contract.
- [`research/CONTROL_TIMING_PHASE0_CLOSURE.md`](research/CONTROL_TIMING_PHASE0_CLOSURE.md) — closure gate.
- [`research/data/juno106_control_timing_calibration_v0.yaml`](research/data/juno106_control_timing_calibration_v0.yaml) — timing/S&H calibration surface.
- [`research/data/juno106_control_timing_evidence.csv`](research/data/juno106_control_timing_evidence.csv) — evidence table.
- [`research/data/juno106_control_schedule.csv`](research/data/juno106_control_schedule.csv) — machine-readable ROM macro schedule.

Frozen scheduler rules include ordered non-atomic service, sequential six-voice DCO and ENV/VCA/VCF work, 12-bit physical DAC quantization before analogue holds, late-LFO causality, normal returning parameter/ADC interrupts and note-event main-loop restart behavior.

## 5. MC5534 DCO wave generator

- [`research/JUNO106_MC5534_REFERENCE_MODEL.md`](research/JUNO106_MC5534_REFERENCE_MODEL.md)
- [`research/JUNO106_MC5534_RAMP.md`](research/JUNO106_MC5534_RAMP.md)
- [`research/MC5534_PHASE0_CLOSURE.md`](research/MC5534_PHASE0_CLOSURE.md)
- [`research/data/juno106_mc5534_calibration_v0.yaml`](research/data/juno106_mc5534_calibration_v0.yaml)
- [`research/data/juno106_mc5534_evidence.csv`](research/data/juno106_mc5534_evidence.csv)

The graph retains 8253 event timing, held DCO CV, RANGE-selected ramp slope, finite reset, shared-ramp SAW/PWM and timing-domain SUB.

## 6. Source mixer / SUB / common NOISE

- [`research/JUNO106_SOURCE_MIXER_REFERENCE_MODEL.md`](research/JUNO106_SOURCE_MIXER_REFERENCE_MODEL.md)
- [`research/JUNO106_SOURCE_MIXER_NOISE.md`](research/JUNO106_SOURCE_MIXER_NOISE.md)
- [`research/SOURCE_MIXER_PHASE0_CLOSURE.md`](research/SOURCE_MIXER_PHASE0_CLOSURE.md)
- [`research/data/juno106_source_mixer_calibration_v0.yaml`](research/data/juno106_source_mixer_calibration_v0.yaml)
- [`research/data/juno106_source_mixer_evidence.csv`](research/data/juno106_source_mixer_evidence.csv)
- [`research/netlists/juno106_source_mixer_topology.csv`](research/netlists/juno106_source_mixer_topology.csv)

One physical common NOISE waveform fans to all six filters. Source combinations are never normalized by enabled-source count.

## 7. A1QH80017A VCF/VCA

- [`research/JUNO106_80017A_REFERENCE_MODEL.md`](research/JUNO106_80017A_REFERENCE_MODEL.md)
- [`research/JUNO106_80017A_TOPOLOGY.md`](research/JUNO106_80017A_TOPOLOGY.md)
- [`research/80017A_PHASE0_CLOSURE.md`](research/80017A_PHASE0_CLOSURE.md)
- [`research/data/juno106_80017a_calibration_v0.yaml`](research/data/juno106_80017a_calibration_v0.yaml)
- [`research/data/juno106_80017a_evidence.csv`](research/data/juno106_80017a_evidence.csv)

The strict graph contains four explicit IR3109-family stages, an explicit BA662 resonance/Q-compensation block with input feed-forward plus stage-4 feedback, and an explicit BA662 voice VCA.

## 8. Global HPF / output

- [`research/JUNO106_HPF_GLOBAL_OUTPUT.md`](research/JUNO106_HPF_GLOBAL_OUTPUT.md)
- [`research/JUNO106_GLOBAL_OUTPUT_REFERENCE_MODEL.md`](research/JUNO106_GLOBAL_OUTPUT_REFERENCE_MODEL.md)
- [`research/GLOBAL_OUTPUT_PHASE0_CLOSURE.md`](research/GLOBAL_OUTPUT_PHASE0_CLOSURE.md)
- [`research/data/juno106_global_output_calibration_v0.yaml`](research/data/juno106_global_output_calibration_v0.yaml)
- [`research/data/juno106_global_output_evidence.csv`](research/data/juno106_global_output_evidence.csv)
- [`research/netlists/juno106_hpf23_component_reference.cir`](research/netlists/juno106_hpf23_component_reference.cir)

## 9. Chorus

- [`research/JUNO106_CHORUS.md`](research/JUNO106_CHORUS.md)
- [`research/JUNO106_CHORUS_REFERENCE_MODEL.md`](research/JUNO106_CHORUS_REFERENCE_MODEL.md)
- [`research/JUNO106_CHORUS_CLOCK.md`](research/JUNO106_CHORUS_CLOCK.md)
- [`research/JUNO106_CHORUS_FILTERS_MIX.md`](research/JUNO106_CHORUS_FILTERS_MIX.md)
- [`research/JUNO106_CHORUS_PRIMARY_SCHEMATIC.md`](research/JUNO106_CHORUS_PRIMARY_SCHEMATIC.md)
- [`research/JUNO106_CHORUS_TRANSCRIPTION_COMPLETENESS.md`](research/JUNO106_CHORUS_TRANSCRIPTION_COMPLETENESS.md)
- [`research/data/juno106_chorus_component_evidence.csv`](research/data/juno106_chorus_component_evidence.csv)
- [`research/data/juno106_chorus_calibration_v0.yaml`](research/data/juno106_chorus_calibration_v0.yaml)
- [`research/netlists/juno106_chorus_primary_pinmap.csv`](research/netlists/juno106_chorus_primary_pinmap.csv)
- [`research/netlists/juno106_chorus_nodes.csv`](research/netlists/juno106_chorus_nodes.csv)
- [`research/netlists/juno106_chorus_output_mix_reference.cir`](research/netlists/juno106_chorus_output_mix_reference.cir)

## 10. FM-1 target / deployment

- [`research/FM1_TARGET_BUDGET.md`](research/FM1_TARGET_BUDGET.md) — detailed target strategy and benchmark scenes.
- [`research/FM1_TARGET_PHASE0_CLOSURE.md`](research/FM1_TARGET_PHASE0_CLOSURE.md) — authoritative deployment closure; state `FM1_TARGET_PHASE0_ARCHITECTURE_FROZEN`.
- [`research/data/fm1_target_budget_v0.yaml`](research/data/fm1_target_budget_v0.yaml) — machine-readable target contract.

Frozen deployment policy:

- callback-size-independent synthesis core;
- first custom-DSP adapter from the Dinghy/Felucca family;
- official FM1 Open remains authoritative API/hardware reference but its native synth engine is a separate binary SDK boundary;
- one-core `TARGET_BASE` must retain all six voices and strict behavioral semantics;
- core 2 may accelerate `TARGET_FULL` but is never a correctness dependency;
- fixed-point/integer target, no runtime 64-bit division in the Dinghy baseline;
- explicit CPU/memory release bands and deterministic REF-vs-target testing.

## 11. Measurement / evidence

- [`research/PHASE0_MEASUREMENT_PLAN.md`](research/PHASE0_MEASUREMENT_PLAN.md)
- [`research/SOURCE_INDEX.md`](research/SOURCE_INDEX.md)
- [`research/JUNO106_EVIDENCE.md`](research/JUNO106_EVIDENCE.md)
- [`research/IMPLEMENTATION_REFERENCES.md`](research/IMPLEMENTATION_REFERENCES.md)

A numerical value without a frozen evidence status is not permission to hard-code it into strict DSP.

## 12. Historical baselines

These remain useful for project history but are not current authority where newer block specs disagree:

- [`ARCHITECTURE.md`](ARCHITECTURE.md)
- [`PLAN.md`](PLAN.md)
- [`REFERENCES.md`](REFERENCES.md)
- [`CIRCUIT_MODEL_SPEC_V0.md`](CIRCUIT_MODEL_SPEC_V0.md)

## 13. Phase transition

**Live Phase-1 implementation handoff:** [`PHASE1_IMPLEMENTATION_STATUS.md`](PHASE1_IMPLEMENTATION_STATUS.md). This tracks built/tested code, approximate divisor-profile fidelity, exactness gaps and the next work slice. It does not override Phase-0 behavioral authority.

**Timestamped parameter and replay interface:** [`PHASE1_RAW_PARAMETER_EVENTS.md`](PHASE1_RAW_PARAMETER_EVENTS.md). This is the stable ID/7-bit/8-bit/flag registry for version-1 `PARAMETER` event scripts, ROM macro service causality, and `MEASURE_PENDING` boundaries.

**8253 Mode-3 edge-validation contract:** [`PHASE1_8253_EDGE_VALIDATION.md`](PHASE1_8253_EDGE_VALIDATION.md). This tracks independently tested normal-count edge sequences, count 0/65536, the supported exceptional-count boundary, and still-unverified physical GATE/RANGE phase.

**Native switch/ADC/IC29 serial protocol:** [`PHASE1_NATIVE_PANEL_SERIAL_ADC.md`](PHASE1_NATIVE_PANEL_SERIAL_ADC.md). This is the frozen clean-room native input contract, packed bit truth tables, ADC integer conditioning, running-status command map, and remaining CPU-cycle validation limits.

**Independent uPD7811 ADC/VCF ALU flag oracle:** [`PHASE1_UPD7811_FLAG_VALIDATION.md`](PHASE1_UPD7811_FLAG_VALIDATION.md). Exhaustive 8-bit ADC history-pair checks and VCF carry/borrow integer tests, with strict full-ROM execution gate still open.

**Original IC29 opcode execution:** [`PHASE1_IC29_OPCODE_EXECUTION.md`](PHASE1_IC29_OPCODE_EXECUTION.md). Runtime-pinned public IC29 machine bytes (ADC + VCF clamp) executed in independent bounded uPD7811 instruction-window interpreters, directly diffed against compiled production control, with explicit MAME/full-ROM limits.

Phase-0 research branch:

```text
research/phase0-juno106-control
```

Phase-1 implementation branch:

```text
phase1/reference-engine
```

Phase-1 follows:

```text
evidence -> explicit behavior/spec -> high-precision REF -> deterministic tests -> target approximation -> FM-1 profiling
```

Never:

```text
FM-1 code -> sounds plausible -> retroactively call it a model
```
