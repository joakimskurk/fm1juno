# FM1JUNO — Phase 1 Reference Engine Plan

Status: implementation plan. **No fidelity claim is implied until the acceptance gates below pass.**

Primary objective: build a deterministic, observable JUNO-106 reference engine whose behavior is traceable to the Phase-0 evidence set, then reduce that engine onto FM-1 without changing the instrument architecture.

## 0. Core principle

There will be two implementations with different jobs:

```text
REFERENCE
  high precision
  easy to inspect
  allowed to be slow
  may oversample heavily
  defines expected behavior

TARGET
  FM-1 fixed-point/integer implementation
  realtime constrained
  compared continuously against REFERENCE
```

The target is **not** allowed to become the source of truth merely because it runs on hardware.

The exact digital JUNO control logic is shared where practical because it is already integer/firmware-domain behavior. The analogue/audio model may have separate reference and optimized implementations.

---

## 1. Proposed repository layout

```text
docs/
  PHASE0_REFERENCE_ARCHITECTURE.md
  PHASE1_REFERENCE_ENGINE_PLAN.md
  research/

calibration/
  README.md
  juno106_nominal.yaml
  measurement_pending.yaml
  generated/
    juno106_calibration.h
    juno106_calibration.hpp

src/
  control106/
    assigner106.c/.h
    envelope106.c/.h
    lfo106.c/.h
    portamento106.c/.h
    pitch106.c/.h
    control_scheduler106.c/.h

  timer106/
    timer8253_106.c/.h

  common/
    fixed_time.c/.h
    event_queue.c/.h
    deterministic_noise.c/.h
    trace.c/.h

reference/
  wave5534_ref.cpp/.hpp
  mixer106_ref.cpp/.hpp
  filter80017_ref.cpp/.hpp
  vca80017_ref.cpp/.hpp
  hpf106_ref.cpp/.hpp
  global_vca106_ref.cpp/.hpp
  chorus106_ref.cpp/.hpp
  engine106_ref.cpp/.hpp

  render/
    wav_writer.cpp
    render_script.cpp

platform/
  host/
    main.cpp
    host_audio.cpp

  fm1_dinghy/              # created only when target phase starts
    adapter.c
    ui.c

  fm1_sdk/                 # optional future adapter, no dependency from core

target/
  wave5534_fx.c/.h
  filter80017_fx.c/.h
  chorus106_fx.c/.h
  engine106_fx.c/.h

tests/
  unit/
  vectors/
  audio/
  scripts/

tools/
  gen_calibration.py
  compare_render.py
  analyze_trace.py
```

Names may evolve, but the architectural separation should not.

---

## 2. Language boundary

### Exact control core

Use portable C with explicit-width integers.

Reason:

- behavior is already integer/fixed-point in the original 106;
- same source can run on host and FM-1;
- avoids two subtly different assigner/LFO/ENV implementations.

Rules:

- no dependency on FM-1 headers;
- no heap allocation;
- no hidden wall clock;
- state passed explicitly;
- no copied Roland ROM machine code.

### High-precision analogue reference

Use host-only C++ with `double` where convenient.

Reason:

- easier physical units and numerical experiments;
- topology can be changed during fitting without target constraints;
- high-rate truth renders can be compared to cheaper candidates.

### FM-1 target

Portable C / fixed-point and integer operations compatible with pi32v2.

Do not force target restrictions into the desktop truth model.

---

## 3. Calibration data is code input, not scattered constants

Create one machine-readable nominal calibration source.

Example conceptual entry:

```yaml
mc5534:
  ramp_cap_f:
    value: 1.0e-9
    status: SCHEMATIC_CROSS_CHECK
  range_r_ohm:
    value: [399000, 200000, 100000]
    status: SCHEMATIC_CROSS_CHECK
  reset_tau_s:
    value: null
    status: MEASURE_PENDING
```

Every unresolved analogue number must carry:

```text
value/default
status
evidence reference
measurement-plan section
```

Generated target headers contain numeric constants plus a build-time unresolved-calibration report.

Never hide provisional numbers inside DSP source files.

---

## 4. Common status vocabulary

Use a small frozen status set:

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

A strict-model report should be able to list every active `MEASURE_PENDING` parameter.

---

## 5. Absolute sample timeline

The engine owns a monotonically increasing sample timestamp:

```text
sample_index
```

Sub-sample events use a fixed fractional representation, e.g.:

```text
Q32.32 sample time in reference
32-bit fractional/sample-compatible representation on target
```

Events include:

- 8253 output transition;
- pending Mode-3 count activation;
- timer reset;
- MC5534 reset start/end;
- pulse comparator crossing;
- SUB transition;
- 106 control-service event;
- serial Voice On/Off event in timing tests.

Outer audio block boundaries are never synthesis events by themselves.

---

## 6. Event ordering must be explicit

When events share a timestamp, define deterministic priority.

Initial proposed order:

```text
1. incoming voice/control command
2. timer control-word reset/load
3. timer edge / pending-count activation
4. MC5534 reset trigger
5. control S/H update
6. comparator/sub derived transition
7. sample observation/render
```

This order is provisional until all hardware semantics are checked. The important requirement is that it exists in one place and is testable.

Do not rely on C function-call accident to define simultaneous-event behavior.

---

## 7. Phase 1A — repository/test scaffold

Deliverables:

- host build;
- unit-test runner;
- WAV render tool;
- deterministic event-script format;
- trace writer;
- calibration generator;
- CI build for host tests.

### Event-script format

A tiny text/JSON representation is enough:

```text
sample 0:     set patch state
sample 100:   note_on C4
sample 44100: note_off C4
```

For sub-sample serial/timer tests, allow fractional timestamps.

### Trace channels

Reference engine can expose named taps:

```text
voiceN.pitch88
voiceN.timer_count
voiceN.timer_out
voiceN.dco_cv
voiceN.saw_v
voiceN.pulse
voiceN.sub
voiceN.env14
voiceN.vcf_code
voiceN.vcf_stage1..4
voiceN.vca_out
voice_sum
hpf_out
global_vca_out
chorus_clock_l/r
chorus_delay_l/r
out_l/r
```

No need to retain these in target release builds.

### Exit gate

- identical host test result on repeated runs;
- trace format versioned;
- no platform-specific symbols inside core.

---

## 8. Phase 1B — exact digital/control core

Implement first because this is the best-understood part and creates input state for every analogue block.

### 8.1 `Assigner106`

Implement:

- Poly 1 dynamic order;
- same-note free-voice reuse;
- release-to-back behavior;
- Poly 2 physical first-free behavior;
- strict six-held-note limit;
- Solo-Unison six physical voice commands;
- sustain semantics;
- no normal voice stealing.

Tests:

- every sequence from `JUNO106_ASSIGNER_RETRIGGER.md`;
- seventh-key ignored;
- same-note reuse;
- release order;
- sustain transitions;
- Solo-Unison scan arbitration.

### 8.2 `Envelope106`

Implement:

- 14-bit state;
- current-level attack retrigger;
- additive attack;
- original truncated three-part decay/release multiply;
- sustain transition behavior.

Coefficient generation must be clean-room/behavioral. Do not paste a ROM blob into source.

Tests:

- attack from zero;
- attack from non-zero release level;
- decay to multiple sustain values;
- release from multiple levels;
- exact integer edge/clamp vectors.

### 8.3 `Lfo106`

Implement recovered integer accumulator:

- 0..0x1fff magnitude;
- direction bit;
- polarity bit;
- endpoint clamp;
- exact compact rate-coefficient law;
- Delay holdoff + depth attack;
- performance-trigger depth path.

Tests include high-rate quantization/plateaus.

### 8.4 `Portamento106`

Implement per-voice 8.8 semitone constant-step glide and overshoot clamp.

### 8.5 `Pitch106`

Implement:

- Base/Tune/LFO/Bender order;
- per-voice portamento pitch;
- 8.8 pitch domain;
- note-divisor interpolation behavior;
- DCO-CV code law.

### Exit gate

Every digital/control test must be exact integer equality, not audio tolerance.

---

## 9. Phase 1C — `Timer106` / 8253 event model

This is the bridge between firmware behavior and analogue source timing.

Per physical voice:

```text
range_clock
current_count
pending_count
high_or_low
output_state
next_transition_time
```

Implement:

- Mode 3 even/odd half-cycle lengths;
- LSB/MSB load semantics at behavioral level;
- ordinary pending count update;
- update becoming active at correct output boundary;
- reset/control-word + fresh load;
- phase preservation on non-reset pitch changes.

### Required tests

- even N waveform timing;
- odd N asymmetric high/low timing;
- reload in first/second half-cycle;
- reset vs ordinary reload;
- repeated same note;
- legato different note while running;
- different note from released/non-running voice;
- range changes if/when original semantics are frozen.

### Exit gate

Edge timestamps agree exactly with a reference 8253 behavioral vector in timer-clock units and within fixed-point rounding tolerance in audio-sample units.

---

## 10. Phase 1D — MC5534 high-precision reference

First analogue/audio block.

Reference keeps physical voltage-like state:

```text
held DCO CV volts
range R
Cint
ramp capacitor voltage
reset state
PWM threshold volts
```

### Version D0

Diagnostic ideal reset + exact linear ramp.

Purpose: validate amplitude compensation equations and event timing.

### Version D1

Finite configurable reset model with `MEASURE_PENDING` constants.

### Version D2

High-rate/oversampled truth render for reset/comparator transient experiments.

### Pulse

Do not create a second oscillator.

Solve comparator crossing against the same ramp.

### SUB

Timing-domain divide-by-two, phase tied to DCO events.

### Exit gate

- ~12 Vp-p neighborhood emerges from physical law rather than normalization;
- range-equivalent pitches agree closely without forced gain matching;
- PWM service anchors reproducible;
- phase/retrigger tests preserve Timer106 behavior.

---

## 11. Phase 1E — common NOISE and source mixer

### Noise

One deterministic physical source for the whole instrument.

Reference pipeline:

```text
raw stochastic source
 -> transistor/noise shaping candidate
 -> level/VCA law
 -> bandwidth network
 -> common signal distributed to six voices
```

Until measured, shaping/gain constants are `MEASURE_PENDING`.

### Mixer

Maintain explicit hardware-voltage gains:

```text
WAVE gain
SUB gain
NOISE gain
```

No automatic normalization by active source count.

### Exit gate

- all six voices receive the same underlying noise waveform;
- individual filtering makes outputs diverge as expected;
- combined oscillator-source drive can exceed single-source level naturally.

---

## 12. Phase 1F — 80017A high-precision reference

This is the main fidelity and CPU-risk block.

Reference topology:

```text
input
 -> nonlinear OTA integrator 1
 -> nonlinear OTA integrator 2
 -> nonlinear OTA integrator 3
 -> nonlinear OTA integrator 4
 -> BA662 voice VCA

resonance BA662:
  stage-4 feedback
  + opposite differential input feed-forward/Q compensation
```

### Build in layers

#### F0 — linear topology

Four explicit stages + correct feedback/feed-forward topology.

Use for control-law fitting and stability.

#### F1 — self-oscillation calibrated model

Fit to:

```text
248 Hz C4 service anchor
992 Hz C6 service anchor
4.8 Vp-p self oscillation
```

#### F2 — nonlinear OTA/buffer model

Add stage nonlinearities and asymmetric/headroom behavior only where evidence or hardware fitting supports it.

#### F3 — BA662 VCA

Fit 6 Vp-p service anchor and control/gain measurements.

### Oversampling experiments

Compare:

```text
1x
2x filter only
4x reference truth
```

4x is a reference experiment, not the default FM-1 plan.

### Exit gate

- correct small-signal 24 dB/oct family;
- stable self-oscillation;
- calibration anchors met;
- resonance passband/Q-compensation behavior qualitatively and quantitatively correct;
- large-signal tests have a documented error envelope.

---

## 13. Phase 1G — voice sum, HPF and global VCA

### HPF

Implement four independent measured/fitted transfer functions:

```text
0 bass boost
1 flat
2 HPF
3 stronger HPF
```

Before measurement, topology-correct candidates live behind calibration data.

### Global VCA

Keep separate from per-voice VCA.

Fit:

```text
VCA LEVEL code -> dB gain
headroom
nonlinearity
```

### Exit gate

Relative HPF sweeps match hardware/derived transfer targets; no 60 load-dependent behavior in strict 106.

---

## 14. Phase 1H — 106 chorus reference

Build from physical clock outward.

### H0 — exact clock-domain semantics

Implement:

```text
f_cp = f_osc / 2
t_delay = 128 / f_cp
```

with explicit clock type.

### H1 — provisional clock curves

Use centralized `MEASURE_PENDING` I/II curves, not Juno-60 rates.

### H2 — BBD-equivalent fractional delay

Two physical channels with opposite modulation.

### H3 — pre/post transfer

Implement the explicit multi-section reference network once the original netlist/fitting is complete.

### H4 — bias/headroom/noise

Separate:

- input bias clipping;
- BBD bandwidth/insertion loss;
- device noise;
- clock feedthrough;
- dry/wet summer.

### Exit gate

- delay obeys physical clock identity;
- L/R modulation opposite;
- dry+wet routing correct;
- exact 106 I/II rates are either measured or still visibly `MEASURE_PENDING`;
- no hidden Juno-6/60 constants.

---

## 15. Phase 1I — integrated reference instrument

Only after individual block tests pass.

Signal chain:

```text
Assigner/control
 -> six Timer106
 -> six MC5534 source paths
 -> common NOISE distribution
 -> six source mixers
 -> six 80017A VCF/VCA
 -> voice sum
 -> HPF
 -> global VCA
 -> chorus
 -> output
```

### Integrated tests

- factory-style service patches;
- Poly 1 release sequence;
- Poly 2 portamento sequence;
- Solo-Unison repeated/legato phrase;
- PWM manual/LFO;
- cutoff ENV normal/inverted;
- high resonance/self oscillation;
- noise-only patch;
- chorus OFF/I/II.

### Render artifacts

Generate short deterministic WAV + trace pairs for review.

Do not commit huge WAV libraries. Keep a compact golden set and generate the rest in CI/local runs.

---

## 16. Block-size invariance test

This is mandatory before target work.

Render the same 10-second event script using outer blocks:

```text
16
32
64
128
256
```

Expected:

- exact digital state traces identical at corresponding sample times;
- audio difference at machine epsilon / documented interpolation tolerance;
- no LFO/ENV phase drift;
- no chorus phase drift;
- no allocation difference.

If output changes materially with outer block size, the implementation has accidentally quantized instrument behavior to the host callback.

---

## 17. Target reduction methodology

For each analogue block:

1. freeze a reference test set;
2. implement first target candidate;
3. compare waveform/spectrum/control response;
4. profile FM-1 cost;
5. optimize one dimension at a time;
6. reject optimizations that exceed error budget;
7. record the chosen approximation and evidence.

No whole-engine rewrite for performance.

---

## 18. Target error metrics

Use block-specific metrics rather than one waveform RMS number.

### DCO/source

- frequency/timing error;
- transition timestamp error;
- Vp-p error;
- harmonic spectrum error;
- PWM duty error.

### VCF

- cutoff cents error;
- resonance peak dB/Q;
- self-osc frequency/amplitude;
- THD/harmonic ratios;
- transient settling.

### VCA

- gain dB error;
- attack/release envelope output error;
- clipping threshold.

### Chorus

- clock/delay trajectory error;
- L/R phase;
- frequency response;
- dry/wet gain;
- THD;
- noise spectrum.

---

## 19. Target implementation sequence

After the integrated reference passes:

```text
T1 exact control + Timer106 on FM-1 host-compatible C
T2 fixed-point/event MC5534 source
T3 first 1x fixed-point 80017A
T4 six voices single-core benchmark
T5 optimize filter nonlinearities/LUTs
T6 global HPF/VCA
T7 chorus
T8 full six-voice benchmark
T9 selective 2x filter test
T10 dual-core 3+3 split if needed
T11 UI/MIDI/presets
T12 long hardware stress run
```

Do not introduce dual-core before T4 gives a meaningful single-core cost breakdown.

---

## 20. First code commit gate

Before the first implementation commit, the branch should contain:

- this plan;
- current `PHASE0_REFERENCE_ARCHITECTURE.md`;
- `PHASE0_MEASUREMENT_PLAN.md`;
- `FM1_TARGET_BUDGET.md`;
- source/provenance rules;
- a new implementation branch cut from the reviewed Phase-0 research state.

Recommended branch:

```text
phase1/reference-engine
```

Phase-0 research continues to accept measurement/evidence updates independently. Phase-1 merges those changes through reviewed calibration/spec updates rather than silently rewriting its assumptions.

---

## 21. Recommended first implementation PR

Keep the first code PR deliberately narrow:

```text
PR: Phase 1A/1B — host scaffold + exact control core
```

Contains:

- build/test harness;
- calibration schema/generator;
- Assigner106;
- Envelope106 arithmetic skeleton + validated coefficient generator work;
- Lfo106;
- Portamento106;
- Pitch106;
- deterministic vector tests;
- no VCF, chorus or FM-1 adapter yet.

That gives us a trustworthy control machine before audio modeling begins.

---

## 22. Definition of 'reference engine ready'

Phase 1 reference engine is ready for FM-1 reduction when:

- strict digital behavior passes exact vectors;
- timer event behavior passes edge vectors;
- every analogue block has a high-precision implementation;
- every provisional analogue constant is centralized and visibly marked;
- integrated renders are deterministic;
- block-size invariance passes;
- all named internal taps can be traced;
- hardware measurements can update calibration without architectural changes;
- no source block relies on a generic VA substitute where a 106-specific model has already been established.
