# FM1JUNO — Phase 0 Reference Architecture

Status: **authoritative living index for current Phase-0 decisions**.

`docs/CIRCUIT_MODEL_SPEC_V0.md` is retained as the early baseline/design draft. Where that older file conflicts with a specialized research document listed here, **the newer specialized document wins**.

This index intentionally stays short. Detailed evidence, arithmetic, caveats and tests live in the linked files.

## 1. Product/model policy

### Strict `MODEL_106`

Primary target. Reproduce observable JUNO-106 behavior and circuit/control architecture as closely as practical on FM-1.

Rules:

- no generic VA substitution where 106-specific behavior is known;
- six physical voices;
- no normal seventh-note voice stealing;
- firmware/control-rate stepping is preserved where it is part of the original behavior;
- analogue modeling begins at real hardware boundaries such as timer edges, DAC/S&H controls and hybrid inputs rather than reinterpreting panel parameters directly in audio rate.

### JUNO-60 evidence

Use only where topology is demonstrably shared or as a measurement method.

Allowed example: exposed JUNO-6/60 IR3109 + BA662 VCF/VCA circuit as visibility into the same IC family/topology physically confirmed inside 80017A.

Not allowed as silent 106 truth:

- IR3R01 ENV;
- master-clock DCO modulation architecture;
- HPF loading/output behavior;
- chorus constants/timing;
- any other 60-specific behavior without 106 corroboration.

### DEEP

Post-fidelity extension layer. Circuit-derived alternatives/tolerances may live here later, but DEEP must not contaminate strict `MODEL_106` calibration.

## 2. Evidence and provenance

Authoritative provenance rules:

- [`research/SOURCE_INDEX.md`](research/SOURCE_INDEX.md)
- [`research/JUNO106_EVIDENCE.md`](research/JUNO106_EVIDENCE.md)

Clean-room rule:

- use ROM disassembly to understand observable arithmetic/control behavior;
- do not copy original Roland machine code or wholesale ROM tables;
- prefer compact independently derived integer laws, equations and calibration vectors;
- GPL reference projects are not copied unless licensing is deliberately made compatible.

## 3. Normal voice assignment and retrigger

Authoritative document:

- [`research/JUNO106_ASSIGNER_RETRIGGER.md`](research/JUNO106_ASSIGNER_RETRIGGER.md)

Frozen highlights:

- Poly 1: dynamic physical-voice order, released voice moves to back, same-note free-voice reuse;
- Poly 2: fixed physical first-free scan;
- seventh simultaneously held key is ignored in normal poly modes;
- every module Voice On re-arms ENV Attack;
- ENV re-attack begins from current level;
- timer reset is separate from ENV retrigger;
- timer reset occurs for different-note + not-running voice; same-note and/or running/sustained voice preserves Mode-3 phase;
- normal Solo-Unison sends the note to all six physical voices rather than rendering one oscillator six times.

## 4. Pitch, Portamento, 8253 and DCO CV

Authoritative documents:

- [`research/JUNO106_PITCH_DCOCV.md`](research/JUNO106_PITCH_DCOCV.md)
- [`research/JUNO106_BENDER.md`](research/JUNO106_BENDER.md)
- [`research/JUNO106_ROM_CONTROL.md`](research/JUNO106_ROM_CONTROL.md)

Frozen highlights:

- pitch domain: 8.8 fixed-point semitone;
- global pitch offset: Base + Tune + signed LFO + signed Bender;
- per-voice Portamento pitch is added to that global offset;
- Portamento is constant-step firmware-rate movement in 8.8 semitone space;
- 8253 runs Mode 3 with integer count interpolation and separate ordinary reload/reset-and-load behavior;
- ordinary count changes take effect at the Mode-3 boundary, not as instant audio-rate phase-increment changes;
- digital DCO-CV law follows an independently derivable octave-doubling/exponential note law with the same pitch fraction as timer interpolation;
- RANGE is applied in the analogue current/ramp network rather than included in digital DCO-CV data;
- ideal range-network conductance scales approximately 1:2:4 for 16':8':4' before nonideal corrections.

## 5. Digital envelope

Authoritative documents:

- [`research/JUNO106_ROM_CONTROL.md`](research/JUNO106_ROM_CONTROL.md)
- relevant retrigger rules in [`research/JUNO106_ASSIGNER_RETRIGGER.md`](research/JUNO106_ASSIGNER_RETRIGGER.md)

Frozen highlights:

- 14-bit integer ENV;
- additive Attack increment;
- exponential-style Decay/Release through the original three-part 8x8 fixed-point multiply structure;
- current-level re-attack;
- firmware/control-rate behavior must be preserved before S/H reconstruction.

Still open: exact main-loop/service timing under workload and final S/H reconstruction dynamics.

## 6. LFO, LFO Delay and performance modulation

Authoritative documents:

- [`research/JUNO106_LFO_PORTAMENTO.md`](research/JUNO106_LFO_PORTAMENTO.md)
- [`research/JUNO106_LFO_TRIGGER_DELAY.md`](research/JUNO106_LFO_TRIGGER_DELAY.md)
- [`research/JUNO106_LFO_RATE_LAW.md`](research/JUNO106_LFO_RATE_LAW.md)

Frozen highlights:

- integer triangle-like magnitude accumulator with direction/polarity bits and endpoint clamp;
- full signed period consists of four integer half-ramps;
- rate coefficient mapping has an exact compact piecewise integer generator;
- high-rate frequency quantization/plateaus are real firmware behavior;
- LFO Delay is two-stage: holdoff then depth ramp;
- automatic Delay is armed only after all `voiceRun` bits become empty and restarts on the first subsequent running voice;
- overlapping/legato/sustained notes do not restart automatic Delay;
- physical LFO-trigger/MIDI performance depth is combined after automatic DCO-LFO Delay scaling and therefore bypasses the panel Delay.

Still open: exact average/workload-dependent loop timing and physical trigger ADC threshold/hysteresis.

## 7. Digital VCF control

Authoritative document:

- [`research/JUNO106_VCF_CONTROL.md`](research/JUNO106_VCF_CONTROL.md)

Frozen boundary:

```text
panel/control/voice state
    -> integer module-CPU arithmetic
    -> per-voice clamped 14-bit VCF control
    -> DAC/S&H
    -> analogue 80017A control law
```

Frozen highlights:

- Cutoff: `raw7 << 7` -> `0x0000..0x3f80`;
- global order: Cutoff +/- VCF-LFO +/- VCF-Bender;
- per voice: +/- scaled ENV, then signed Key Follow;
- ENV/Key Follow use shared `floor(value * m / 256)` 16x8 integer primitive;
- Key Follow uses `3/8` of current 8.8 voice pitch around exact Middle-C pivot `0x1680`;
- VCF-LFO panel depth is doubled, multiplied by Delay onset, multiplied by `0..0x1fff` LFO magnitude and halved into near-12-bit range;
- maximum VCF-LFO signal before sign = 4047 under nominal max values;
- final VCF control clamps to `0x0000..0x3fff` before analogue conversion.

## 8. A1QH80017A filter/VCA

Authoritative document:

- [`research/JUNO106_80017A_TOPOLOGY.md`](research/JUNO106_80017A_TOPOLOGY.md)

Frozen topology:

```text
oscillator mixer
 -> IR3109 stage 1
 -> IR3109 stage 2
 -> IR3109 stage 3
 -> IR3109 stage 4
 -> BA662 voice VCA
 -> voice sum
```

with a separate BA662 resonance/Q-compensation loop receiving:

- stage-4 feedback;
- input feed-forward on the opposite differential input.

Consequences:

- resonance changes both feedback and effective input/passband drive;
- strict model cannot be a generic four-pole `x - k*y4` filter plus one post-shaper;
- nonlinear behavior belongs at OTA/buffer/resonance/VCA nodes;
- per-voice Frequency, Width, Resonance and VCA calibration terms are mandatory.

Factory validation anchors:

- 4.8 Vp-p self-oscillation at the VCF service point;
- 6 Vp-p post-VCA under the service condition;
- 248 Hz C4 Frequency anchor;
- 992 Hz C6 Width anchor;
- repeat Frequency/Width until within +/-10 cents after warm-up.

First desktop reference structure:

- four explicit topology-preserving nonlinear one-pole OTA stages;
- explicit BA662 resonance/Q-compensation block;
- explicit BA662 voice VCA;
- test 1x vs selective 2x oversampling before committing target cost.

Still open:

- exact vintage 106 hybrid passive set / 240-vs-~250-pF question;
- vintage IR3109 nonlinear scale and stage-buffer limits;
- strict BA662 resonance and VCA transfer/control laws;
- VCF DAC/S&H code-to-control-voltage mapping;
- whether thermal/supply dynamics deserve FM-1 budget after static warmed-up behavior passes.

## 9. DCO waveform/mixer, HPF and chorus

Not yet frozen to the same depth as the digital control + 80017A topology.

Current gates are tracked in:

- [`research/PHASE0_CHECKLIST.md`](research/PHASE0_CHECKLIST.md)

Priority order from here:

1. finish MC5534/range/mixer/noise passive extraction;
2. fit/freeze strict 80017A analogue transfer/calibration equations;
3. derive exact 106 global HPF/output transfer functions;
4. derive 106 chorus clock/delay/filter network and bias/nonlinear behavior;
5. prepare FM-1 CPU/SRAM budget and only then choose final approximations.

## 10. Phase-0 gate

The authoritative gate is:

- [`research/PHASE0_CHECKLIST.md`](research/PHASE0_CHECKLIST.md)

Phase 1 may begin component-by-component when an item has either:

1. an evidence-backed frozen behavior/model; or
2. an explicit hypothesis with a named validation/measurement test.

A prototype may exist before the entire Phase-0 checklist is closed, but it must never be mislabeled as strict fidelity for unresolved blocks.