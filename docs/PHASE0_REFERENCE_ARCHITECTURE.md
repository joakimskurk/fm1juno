# FM1JUNO — Phase 0 Reference Architecture

Status: **`PHASE0_REFERENCE_ARCHITECTURE_FROZEN`**.

This is the compact integration contract for strict JUNO-106 behavior. Detailed arithmetic, evidence and measurement plans live in the linked specialized documents.

`docs/CIRCUIT_MODEL_SPEC_V0.md` is historical. Newer block-specific reference models and closure records are authoritative where they disagree.

## 1. Model policy

Strict `MODEL_106` rules:

- six physical voices;
- no normal seventh-note voice stealing;
- preserve observed firmware integer/control-rate behavior;
- preserve real hardware boundaries such as 8253 timer edges, multiplexed DAC/S&H controls, MC5534 ramp state and 80017A input/control nodes;
- preserve drive/headroom rather than normalizing every block;
- unresolved physical constants live in calibration data with evidence status and verification method;
- JUNO-60 data may illuminate shared topology but never silently become a 106 constant.

## 2. Full frozen signal/control graph

```text
keyboard / MIDI / panel
  -> Assigner106 / serial voice commands
  -> ordered module-CPU ControlScheduler106
       -> integer ENV / LFO / Portamento / pitch / VCF arithmetic
       -> 12-bit physical DAC boundary
       -> multiplexed SampleHold106 controls

six physical voices:
  8253 Mode-3 timer
    -> MC5534 ramp/reset/PWM/SUB
    -> WAVE + SUB

one common physical NOISE source
    -> BA662 noise VCA / shaping
    -> same instantaneous NOISE signal distributed to six voices

per voice:
  WAVE + SUB + common NOISE
    -> passive/source input network
    -> A1QH80017A:
         IR3109 stage 1
         IR3109 stage 2
         IR3109 stage 3
         IR3109 stage 4
         + BA662 resonance/Q-compensation loop
         -> BA662 voice VCA
    -> six-voice sum

six-voice sum
  -> HPF 0/1/2/3
  -> IC4A/output conditioning
  -> uPC1252H2 global VCA
  -> dual MN3009/MN3101 chorus
  -> original output/jack network
```

## 3. Assigner / retrigger

Authority:

- [`research/JUNO106_ASSIGNER_RETRIGGER.md`](research/JUNO106_ASSIGNER_RETRIGGER.md)
- [`research/JUNO106_SOLO_UNISON_TIMING.md`](research/JUNO106_SOLO_UNISON_TIMING.md)

Frozen:

- Poly 1 dynamic physical-voice order with release-to-back and same-note free-voice reuse;
- Poly 2 fixed physical first-free scan;
- seventh simultaneously held normal-poly key ignored;
- every Voice On re-arms Attack from current ENV level;
- timer reset is independent from ENV retrigger;
- different note + non-running voice requests reset;
- same-note and/or running voice preserves timer phase;
- Solo-Unison is six sequential physical voice commands over 31.25-kbit/s serial, not one oscillator duplicated six times.

## 4. Control scheduler / physical DAC-S&H — `CONTROL_TIMING_PHASE0_ARCHITECTURE_FROZEN`

Authority:

- [`research/JUNO106_CONTROL_SCHEDULER_REFERENCE_MODEL.md`](research/JUNO106_CONTROL_SCHEDULER_REFERENCE_MODEL.md)
- [`research/CONTROL_TIMING_PHASE0_CLOSURE.md`](research/CONTROL_TIMING_PHASE0_CLOSURE.md)
- [`research/data/juno106_control_schedule.csv`](research/data/juno106_control_schedule.csv)

Frozen:

- approximately 4.2-ms normal service-cycle family;
- not one atomic 236-Hz parameter update;
- ROM-derived macro-order from `$02EC..$07B5`;
- sequential six-voice DCO service;
- sequential six-voice ENV/VCA/VCF service;
- timer count/reset work precedes each voice's DCO-CV S/H commit;
- late LFO advance means early DCO/VCF work uses pre-advance LFO state;
- physical DAC boundary is 12-bit; common aligned 14-bit controls quantize as `code14 >> 2` before S/H;
- ADC and normal parameter interrupts return/resume;
- voice-note event path can discard the interrupted stack and restart the main loop.

Exact slot microseconds, settling and droop are calibration, not architecture.

## 5. Pitch / Portamento / 8253

Authority:

- [`research/JUNO106_PITCH_DCOCV.md`](research/JUNO106_PITCH_DCOCV.md)
- [`research/JUNO106_BENDER.md`](research/JUNO106_BENDER.md)
- [`research/JUNO106_ROM_CONTROL.md`](research/JUNO106_ROM_CONTROL.md)

Frozen:

- 8.8 fixed-point semitone pitch domain;
- global Base + Tune + signed LFO + signed Bender with per-voice Portamento contribution;
- Portamento is constant-step firmware-rate movement with overshoot clamp;
- 8253 Mode 3 with integer divisors and distinct ordinary reload vs reset/control-word + reload;
- pending ordinary count activation follows Mode-3 boundary behavior;
- DCO-CV follows recovered octave/exponential note law;
- RANGE is analogue/post-DCO-CV slope scaling, not part of digital pitch CV.

## 6. ENV / LFO

Frozen ENV:

- 14-bit integer state;
- additive Attack;
- current-level re-attack;
- original truncated multiply-style Decay/Release behavior;
- sustain/release state retained in firmware domain.

Frozen LFO:

- integer triangle-like magnitude accumulator with direction/polarity state;
- compact exact rate coefficient law;
- high-rate quantization/plateaus preserved;
- two-stage automatic Delay: holdoff + depth ramp;
- automatic Delay retrigger derives from `voiceRun` becoming empty then non-empty;
- physical LFO-trigger/MIDI performance modulation bypasses automatic panel Delay scaling.

## 7. MC5534 — `MC5534_PHASE0_ARCHITECTURE_FROZEN`

Authority:

- [`research/JUNO106_MC5534_REFERENCE_MODEL.md`](research/JUNO106_MC5534_REFERENCE_MODEL.md)
- [`research/MC5534_PHASE0_CLOSURE.md`](research/MC5534_PHASE0_CLOSURE.md)

Frozen graph:

```text
held DCO CV -> RANGE-selected R -> C54-family integrator
8253 positive edge -> finite reset event
same ramp -> SAW + PWM comparator -> WAVE
8253 timing -> divide-by-two -> SUB
```

Working component family remains around `399k / 200k / 100k` and `~1 nF` with evidence qualification. Approximate 12 Vp-p is a hardware target, never per-cycle normalization.

## 8. Source mixer / common NOISE — `SOURCE_MIXER_PHASE0_ARCHITECTURE_FROZEN`

Authority:

- [`research/JUNO106_SOURCE_MIXER_REFERENCE_MODEL.md`](research/JUNO106_SOURCE_MIXER_REFERENCE_MODEL.md)
- [`research/SOURCE_MIXER_PHASE0_CLOSURE.md`](research/SOURCE_MIXER_PHASE0_CLOSURE.md)

Frozen:

- one per-voice WAVE node plus separate timing-derived SUB;
- one shared SUB LEVEL control but six physical SUB phases;
- one selected-transistor physical NOISE source feeding IC14 BA662 and RC shaping;
- exact same instantaneous NOISE waveform distributed to all six voice paths;
- no enabled-source-count normalization or hidden pre-VCF clamp;
- Roland Bank-6 / TP8 / VR32 4-Vp-p noise adjustment retained as system validation vector.

## 9. Digital VCF control

Authority: [`research/JUNO106_VCF_CONTROL.md`](research/JUNO106_VCF_CONTROL.md).

Frozen:

```text
Cutoff +/- LFO +/- Bender
  -> per voice +/- ENV
  -> signed Key Follow
  -> clamp 0x0000..0x3fff
  -> physical 12-bit DAC/S&H boundary
```

The original integer multiply/truncation behavior is retained.

## 10. A1QH80017A — `80017A_PHASE0_ARCHITECTURE_FROZEN`

Authority:

- [`research/JUNO106_80017A_REFERENCE_MODEL.md`](research/JUNO106_80017A_REFERENCE_MODEL.md)
- [`research/80017A_PHASE0_CLOSURE.md`](research/80017A_PHASE0_CLOSURE.md)

Frozen:

- four explicit IR3109-family low-pass stages;
- explicit BA662 resonance/Q-compensation element with stage-4 feedback **and** input feed-forward;
- explicit BA662 voice VCA;
- per-voice Frequency, Width, Resonance and VCA calibration;
- topology-preserving implicit/TPT-style high-precision reference direction;
- working opened-original-hybrid passive evidence retained with status, not promoted beyond evidence.

Mandatory Roland anchors:

```text
VCF self oscillation  4.8 Vp-p
post-VCA              6.0 Vp-p
C4 Frequency          248 Hz
C6 Width              992 Hz
interaction tolerance +/-10 cents after iteration
```

Vintage silicon nonlinear scales/control laws are fit parameters.

## 11. Global HPF / output — `GLOBAL_OUTPUT_PHASE0_ARCHITECTURE_FROZEN`

Frozen:

```text
six-voice sum
 -> HPF selector
    0 bass emphasis
    1 straight
    2 47k/15n first-order HPF
    3 47k/4.7n first-order HPF
 -> IC4A/output conditioning
 -> uPC1252H2 global VCA
 -> chorus
```

Ideal RC anchors:

```text
HPF2 225.7516923289 Hz
HPF3 720.4841244540 Hz
```

HPF0 exact coefficients, VCA control curve and headroom remain calibrated physical data.

## 12. Chorus — `CHORUS_PHASE0_FROZEN`

Authority:

- [`research/JUNO106_CHORUS_REFERENCE_MODEL.md`](research/JUNO106_CHORUS_REFERENCE_MODEL.md)
- [`research/JUNO106_CHORUS_PRIMARY_SCHEMATIC.md`](research/JUNO106_CHORUS_PRIMARY_SCHEMATIC.md)
- [`research/JUNO106_CHORUS_CLOCK.md`](research/JUNO106_CHORUS_CLOCK.md)
- [`research/JUNO106_CHORUS_FILTERS_MIX.md`](research/JUNO106_CHORUS_FILTERS_MIX.md)

Frozen:

- dual MN3009 + dual MN3101;
- strict OFF/I/II;
- exact `f_cp = f_osc/2`;
- exact `t_delay = 128/f_cp`;
- timing networks A/B = `1.8k / 8.2k / 150p`;
- original multi-element wet filtering;
- independent VR1/VR2 bias/headroom;
- exact IC6 summer gains `Gdry=-0.39`, `Gwet=-39/47`;
- explicit stereo/output/jack and BBD-noise boundaries.

Mode-I/II trajectories and device loss/noise/headroom are physical calibration datasets.

## 13. FM-1 deployment — `FM1_TARGET_PHASE0_ARCHITECTURE_FROZEN`

Authority:

- [`research/FM1_TARGET_PHASE0_CLOSURE.md`](research/FM1_TARGET_PHASE0_CLOSURE.md)
- [`research/FM1_TARGET_BUDGET.md`](research/FM1_TARGET_BUDGET.md)
- [`research/data/fm1_target_budget_v0.yaml`](research/data/fm1_target_budget_v0.yaml)

Frozen:

- callback-size-independent core;
- official 44.1-kHz/128-frame source remains API reference;
- first custom-DSP adapter uses Dinghy/Felucca-family platform;
- one-core `TARGET_BASE` must retain six voices and strict behavior;
- core 2 may accelerate `TARGET_FULL` but is not a correctness dependency;
- target implementation uses fixed-point/integer domains appropriate to the chosen platform;
- no runtime 64-bit division in Dinghy baseline;
- separate audio, timer-event, JUNO-control, callback and UI/MIDI time domains;
- deterministic REF-vs-target profiling and error tests.

## 14. Remaining calibration is not hidden debt

Authoritative measurement plan:

- [`research/PHASE0_MEASUREMENT_PLAN.md`](research/PHASE0_MEASUREMENT_PLAN.md)

Every unresolved physical quantity must remain one of:

```text
PRIMARY_CONFIRMED
ROM_DERIVED
SCHEMATIC_CONFIRMED
SCHEMATIC_CROSS_CHECK
HARDWARE_MEASURED
DIRECT_REPAIR_MEASURED_WORKING_EVIDENCE
MODERN_REFERENCE_ONLY
DERIVED
MEASURE_PENDING
DEEP_ONLY
```

Phase 1 may use provisional values only through centralized calibration surfaces and must be able to report active `MEASURE_PENDING` values.

## 15. Phase transition

Phase 0 is now complete at the architecture level.

Next branch:

```text
phase1/reference-engine
```

Implementation order:

```text
repo/test scaffold
 -> exact digital/control core + ControlScheduler106
 -> Timer106
 -> high-precision MC5534
 -> common NOISE/source mixer
 -> high-precision 80017A
 -> HPF/global VCA
 -> Chorus106
 -> integrated REF
 -> fixed-point target reductions
 -> FM-1 host/hardware profiling
```

The target is a reduction of the reference, never the source of truth.
