# FM1JUNO — Phase 0 Reference Architecture

Status: **authoritative living index for current Phase-0 decisions**.

`docs/CIRCUIT_MODEL_SPEC_V0.md` is retained as the early baseline/design draft. Where that older file conflicts with a specialized research document listed here, **the newer specialized document wins**.

This index intentionally stays compact. Detailed evidence, arithmetic, caveats and tests live in the linked files.

## 1. Product/model policy

### Strict `MODEL_106`

Primary target. Reproduce observable JUNO-106 behavior and circuit/control architecture as closely as practical on FM-1.

Rules:

- no generic VA substitution where 106-specific behavior is known;
- six physical voices;
- no normal seventh-note voice stealing;
- firmware/control-rate stepping is preserved where it is part of the original behavior;
- analogue modeling begins at real hardware boundaries such as timer edges, DAC/S&H controls and hybrid inputs rather than reinterpreting panel parameters directly at audio rate;
- unresolved analogue constants live in centralized calibration data as `MEASURE_PENDING`, never as scattered magic numbers.

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

## 2. Evidence, provenance and measurement

Authoritative provenance rules:

- [`research/SOURCE_INDEX.md`](research/SOURCE_INDEX.md)
- [`research/JUNO106_EVIDENCE.md`](research/JUNO106_EVIDENCE.md)

Hardware/validation plan:

- [`research/PHASE0_MEASUREMENT_PLAN.md`](research/PHASE0_MEASUREMENT_PLAN.md)

Clean-room rule:

- use ROM disassembly to understand observable arithmetic/control behavior;
- do not copy original Roland machine code or wholesale ROM tables;
- prefer compact independently derived integer laws, equations and calibration vectors;
- GPL reference projects are not copied unless licensing is deliberately made compatible.

Phase-0 measurement rule:

- raw captures are evidence;
- model coefficients are derived artifacts;
- one vintage unit is a calibration target, not universal truth;
- unknown constants may enter Phase 1 only when paired with an explicit measurement/verification test.

## 3. Normal voice assignment, Solo-Unison and retrigger

Authoritative documents:

- [`research/JUNO106_ASSIGNER_RETRIGGER.md`](research/JUNO106_ASSIGNER_RETRIGGER.md)
- [`research/JUNO106_SOLO_UNISON_TIMING.md`](research/JUNO106_SOLO_UNISON_TIMING.md)

Frozen highlights:

- Poly 1: dynamic physical-voice order, released voice moves to back, same-note free-voice reuse;
- Poly 2: fixed physical first-free scan;
- seventh simultaneously held key is ignored in normal poly modes;
- every module Voice On re-arms ENV Attack;
- ENV re-attack begins from current level;
- timer reset is separate from ENV retrigger;
- timer reset occurs for different-note + not-running voice; same-note and/or running/sustained voice preserves Mode-3 phase;
- Solo-Unison sends the same note as six sequential physical Voice On commands rather than rendering one oscillator six times;
- assigner->module link runs at 31.25 kbit/s, so unison reset/control timing is inherently staggered rather than one atomic six-voice event.

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
- ordinary count changes take effect at a Mode-3 boundary rather than as instant audio-rate phase-increment changes;
- digital DCO-CV law follows an independently derivable octave-doubling/exponential note law with the same pitch fraction as timer interpolation;
- RANGE is applied in the analogue current/ramp network rather than included in digital DCO-CV data.

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

Still measurement/calibration-sensitive:

- exact main-loop/service timing under workload;
- final DAC/S&H settling/droop reconstruction.

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
- physical LFO-trigger/MIDI performance depth is combined after automatic DCO-LFO Delay scaling and therefore bypasses panel Delay.

## 7. MC5534 source waveform and oscillator mixer

Authoritative documents:

- [`research/JUNO106_MC5534_RAMP.md`](research/JUNO106_MC5534_RAMP.md)
- [`research/JUNO106_SOURCE_MIXER_NOISE.md`](research/JUNO106_SOURCE_MIXER_NOISE.md)

Frozen source architecture:

```text
8253 edge timing
 -> MC5534 finite-reset ramp
 -> SAW
 -> shared-ramp PWM comparator -> PULSE
 -> timing-domain /2 -> SUB

one common physical NOISE generator

per voice:
  WAVE + SUB + common NOISE
    -> 80017A input
```

Frozen/working physical model:

- DCO CV is a held analogue slope control, not an audio-rate oscillator frequency parameter;
- positive DCO edge initiates ramp reset;
- RANGE selects the ramp-current resistor after DCO CV generation;
- working 106 range set: ~399 kOhm / 200 kOhm / 100 kOhm for 16'/8'/4';
- ramp capacitance triangulates strongly to ~1 nF;
- ideal law `dV/dt = -Vdco/(R*C)` naturally explains the ~12 Vp-p amplitude target;
- strict mode must retain small note/range amplitude error instead of normalizing each cycle;
- PWM derives from the same physical ramp and held PWM CV;
- SUB/NOISE digital level controls use the recovered 14-bit-style control domain;
- one common noise source must feed all six voices; never instantiate six independent noise generators;
- enabled-source-count normalization before the VCF is forbidden.

Still measurement-sensitive:

- exact MC5534 reset transient/residual;
- loaded DCO-CV/S&H voltage law and droop;
- exact source-mixer gains/headroom;
- SUB and NOISE analogue gain law;
- noise spectrum/filter constants.

## 8. Digital VCF control

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

## 9. A1QH80017A filter/VCA

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

Still measurement-sensitive:

- exact vintage hybrid passive set / 240-vs-~250-pF question;
- vintage IR3109 nonlinear scale and stage-buffer limits;
- strict BA662 resonance and VCA transfer/control laws;
- VCF DAC/S&H code-to-control-voltage/frequency mapping;
- whether thermal/supply dynamics deserve FM-1 budget after static warmed-up behavior passes.

## 10. Global HPF and output VCA

Authoritative document:

- [`research/JUNO106_HPF_GLOBAL_OUTPUT.md`](research/JUNO106_HPF_GLOBAL_OUTPUT.md)

Frozen topology:

```text
six voice outputs
 -> global sum
 -> HPF selector
      0 = bass-boost network
      1 = straight/flat path
      2 = HPF
      3 = stronger HPF
 -> buffer
 -> global uPC1252-family VCA / VCA LEVEL
 -> chorus
```

Rules:

- strict 106 must not import JUNO-60 load-dependent HPF/output behavior;
- HPF 0 is not 'cutoff=0 Hz'; it is a distinct bass-boost transfer;
- global VCA remains separate from per-voice BA662 VCAs;
- VCA LEVEL uses recovered digital control scaling but final uPC1252 gain/headroom law is analogue/calibration work.

Still measurement-sensitive:

- exact four HPF transfer functions;
- global VCA gain-vs-control law;
- headroom/nonlinearity.

## 11. JUNO-106 chorus

Authoritative documents:

- [`research/JUNO106_CHORUS.md`](research/JUNO106_CHORUS.md)
- [`research/JUNO106_CHORUS_CLOCK.md`](research/JUNO106_CHORUS_CLOCK.md)
- [`research/JUNO106_CHORUS_FILTERS.md`](research/JUNO106_CHORUS_FILTERS.md)

Frozen physical architecture:

```text
global mono pre-chorus signal
 -> dry path -> L/R summers
 -> wet split
      -> MN3009 path A <- MN3101 clock A
      -> MN3009 path B <- MN3101 clock B
 -> wet analogue filtering/buffering
 -> L/R dry+wet outputs
```

Frozen highlights:

- two physical MN3009 BBD paths and two MN3101 clock/driver paths;
- opposite channel modulation, nominal 180 degrees;
- triangle-like control trajectory with allowance for real hardware imperfection;
- dry+wet routing, not 100%-wet;
- OFF/I/II are strict original user modes;
- Juno-6/60 I+II constants are not imported into strict 106;
- explicit clock domains are mandatory:
  - `f_osc` = MN3101 internal oscillator;
  - `f_cp` = each CP1/CP2 phase output;
  - `f_cp = f_osc/2`;
- MN3009 exact delay identity when `f_cp` is physical CP frequency:
  - `t_delay = 256/(2*f_cp) = 128/f_cp`;
- original R124/R137 = 8.2 kOhm is strongly cross-checked;
- working external clock network ~1.8 kOhm / 8.2 kOhm / 150 pF per channel remains schematic/cross-check evidence pending a clean original read;
- wet path is a multi-section analogue network, not one arbitrary 10–14 kHz LPF;
- reference engine keeps BBD input bias, output-pair combiner, pre/post filtering, BBD headroom and chorus noise as separate concepts;
- clone-only anti-thump/MN3007 improvements must not leak into strict model.

Still measurement-sensitive:

- original Mode I and II clock trajectories/rates;
- center/min/max delay and A/B mismatch;
- exact wet-path filter transfer vs clock;
- dry/wet analogue gains and mono-sum gain;
- BBD bias/headroom curve;
- factory-new BBD noise and clock feedthrough.

## 12. FM-1 target policy

Current target facts:

- 44.1 kHz;
- 128-frame audio blocks;
- 240x240 display;
- strict real-time/SRAM placement constraints;
- existing six-voice FM-106 proves broad platform feasibility but is a performance/integration reference, not a fidelity source.

Implementation policy:

1. build the desktop/reference engine by physical blocks;
2. preserve deterministic test vectors at each block boundary;
3. profile one correct/observable version before approximating it;
4. choose fixed-point/mixed precision per block only after error/CPU measurement;
5. reduce linear analogue sub-networks analytically where transfer equivalence is verified;
6. retain event/control-rate semantics even when the analogue audio block is simplified;
7. no global 4x oversampling assumption — use event-aware anti-aliasing and selective oversampling first.

## 13. Phase-0 gate and Phase-1 start condition

Authoritative gate:

- [`research/PHASE0_CHECKLIST.md`](research/PHASE0_CHECKLIST.md)

Authoritative unresolved-parameter test plan:

- [`research/PHASE0_MEASUREMENT_PLAN.md`](research/PHASE0_MEASUREMENT_PLAN.md)

Phase 1 may begin **component-by-component** when an item has either:

1. an evidence-backed frozen behavior/model; or
2. an explicit hypothesis/default with a named validation/measurement test.

This means the reference engine may now start before every original-unit measurement exists, provided unresolved constants are:

```text
centralized
marked MEASURE_PENDING
covered by deterministic tests
replaceable without architecture changes
```

A prototype may exist before the entire Phase-0 checklist is numerically closed, but it must never be mislabeled as final strict fidelity for unresolved blocks.
