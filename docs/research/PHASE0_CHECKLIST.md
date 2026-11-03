# Phase 0 — JUNO-106 circuit freeze checklist

Status: **`PHASE0_ARCHITECTURE_COMPLETE`**.

Phase 0 is complete when every implementation graph is frozen and every unresolved physical number is represented by a named calibration field plus an explicit verification route. Exact vintage-unit measurements do not have to exist before Phase 1 if they cannot change the graph.

## A. Control / firmware

- [x] Poly 1 / Poly 2 allocation and six-note limit frozen.
- [x] Solo-Unison sequential physical-voice behavior frozen.
- [x] ENV current-level re-attack frozen and separated from DCO timer reset.
- [x] 8.8 pitch, Tune, Bender and Portamento order frozen.
- [x] 8253 Mode-3 count/reload/reset semantics frozen.
- [x] DCO-CV octave/exponential law and RANGE separation frozen.
- [x] Integer ENV architecture frozen.
- [x] Integer LFO/rate/Delay/retrigger architecture frozen.
- [x] Digital VCF arithmetic frozen.

## B. Control timing / DAC-S&H — `CONTROL_TIMING_PHASE0_ARCHITECTURE_FROZEN`

- [x] Approximately 4.2-ms normal service-cycle family retained.
- [x] Non-atomic ROM-derived macro order frozen.
- [x] Sequential six-voice DCO service frozen.
- [x] Sequential six-voice ENV/VCA/VCF service frozen.
- [x] Per-voice timer work precedes that voice's DCO-CV S/H commit.
- [x] Late-LFO / later-pass causality frozen.
- [x] Physical 12-bit DAC boundary frozen.
- [x] DAC-bus load and MUX/S&H commit separated.
- [x] ADC/parameter resume-class interrupts separated from note-event restart class.
- [x] Note-event stack-discard/main-loop restart behavior frozen.
- [x] Timing/S&H calibration centralized.
- [x] Machine-readable schedule and evidence table added.
- [x] **CONTROL TIMING GATE CLOSED.**

Calibration backlog:

- [ ] exact mean/workload-dependent frame periods;
- [ ] exact per-slot microsecond offsets;
- [ ] interrupt latency/jitter;
- [ ] low-level ambiguous VCA/PWM MUX interleave;
- [ ] DAC settling and S/H settling/droop/source impedance/charge injection.

## C. MC5534 DCO — `MC5534_PHASE0_ARCHITECTURE_FROZEN`

- [x] 8253 edge-timed physical source architecture frozen.
- [x] held DCO CV -> RANGE-selected R -> C54-family ramp frozen.
- [x] finite positive-edge reset frozen.
- [x] SAW/PWM shared-ramp behavior frozen.
- [x] timing-domain SUB frozen.
- [x] working original-106 399k/200k/100k + ~1 nF family retained with evidence qualification.
- [x] ~12 Vp-p treated as validation region, not normalization target.
- [x] unresolved reset/headroom/S&H values centralized.
- [x] **MC5534 GATE CLOSED.**

Calibration backlog:

- [ ] direct R85/R86/R87/C54 confirmation;
- [ ] DCO-CV code->volts and S/H loading;
- [ ] reset duration/residual/recovery;
- [ ] MC5534 slew/headroom/comparator behavior;
- [ ] note/range amplitude-error surface.

## D. Source mixer / SUB / common NOISE — `SOURCE_MIXER_PHASE0_ARCHITECTURE_FROZEN`

- [x] WAVE + SUB + one common NOISE graph frozen.
- [x] one global SUB LEVEL with six physical SUB phases frozen.
- [x] one selected-transistor common NOISE source + IC14 BA662 frozen.
- [x] same instantaneous NOISE waveform fanout to all six voices frozen.
- [x] ~5-kHz healthy-noise shaping target retained.
- [x] Bank-6 / TP8 / VR32 4-Vp-p validation vector retained.
- [x] source normalization and hidden pre-VCF clamping forbidden.
- [x] source gains/impedances/headroom centralized.
- [x] **SOURCE MIXER GATE CLOSED.**

Calibration backlog:

- [ ] WAVE/SUB/NOISE injection ratios and impedances;
- [ ] SUB and NOISE analogue control curves;
- [ ] exact healthy noise spectrum/RC transfer;
- [ ] pre-VCF headroom/loading/compression.

## E. A1QH80017A VCF/VCA — `80017A_PHASE0_ARCHITECTURE_FROZEN`

- [x] four explicit IR3109-family stages frozen.
- [x] BA662 resonance/Q-compensation with input feed-forward + stage-4 feedback frozen.
- [x] explicit BA662 voice VCA frozen.
- [x] per-voice Frequency/Width/Resonance/VCA calibration surfaces frozen.
- [x] opened-original-hybrid passive evidence retained with qualification.
- [x] topology-preserving implicit/TPT reference direction frozen.
- [x] Roland anchors frozen: 4.8 Vp-p, 6 Vp-p, 248 Hz, 992 Hz, +/-10 cents.
- [x] unresolved silicon/control-law coefficients centralized.
- [x] **80017A GATE CLOSED.**

Calibration backlog:

- [ ] IR3109 nonlinear scale/buffer headroom/asymmetry;
- [ ] effective 240-vs-250-pF stage set/tolerances;
- [ ] resonance effective coefficients and BA662 control law;
- [ ] VCF code->cutoff mapping;
- [ ] voice-VCA code/CV->gain/headroom;
- [ ] optional thermal/supply dynamics after static reference passes.

## F. Global HPF / output — `GLOBAL_OUTPUT_PHASE0_ARCHITECTURE_FROZEN`

- [x] HPF 0/1/2/3 graph frozen.
- [x] 106 IC4A isolation retained.
- [x] HPF2 ideal corner 225.7516923289 Hz frozen.
- [x] HPF3 ideal corner 720.4841244540 Hz frozen.
- [x] HPF0 direct + active bass-emphasis graph frozen.
- [x] uPC1252H2 global VCA boundary frozen.
- [x] high-level headroom/distortion retained as physical behavior.
- [x] unresolved coefficients/control curves centralized.
- [x] **GLOBAL OUTPUT GATE CLOSED.**

Calibration backlog:

- [ ] exact HPF0 transfer and selector Ron;
- [ ] VCA LEVEL -> uPC1252 gain law;
- [ ] global output headroom/THD/loading.

## G. Chorus — `CHORUS_PHASE0_FROZEN`

- [x] dual MN3009 + dual MN3101 graph frozen.
- [x] strict OFF/I/II frozen.
- [x] `f_cp=f_osc/2` and `t_delay=128/f_cp` frozen.
- [x] original 1.8k/8.2k/150p timing networks frozen.
- [x] original multi-element wet filtering retained.
- [x] independent BBD bias/headroom frozen.
- [x] exact IC6 dry/wet summer gains frozen.
- [x] stereo/output/jack and separate BBD-noise boundaries frozen.
- [x] machine-readable pin/node/component evidence added.
- [x] unresolved clock/filter/noise/headroom values centralized.
- [x] **CHORUS GATE CLOSED.**

Calibration backlog:

- [ ] original Mode-I/II clock trajectories;
- [ ] exact delay excursion/channel mismatch;
- [ ] wet-path transfer/BBD insertion loss and bandwidth;
- [ ] final dry/wet/mono-jack loading;
- [ ] BBD headroom/clipping/noise/correlation.

## H. FM-1 target / deployment — `FM1_TARGET_PHASE0_ARCHITECTURE_FROZEN`

- [x] official 44.1-kHz / 128-frame API reference verified.
- [x] Dinghy/Felucca custom-DSP deployment family selected.
- [x] callback-size-independent core frozen.
- [x] 256-frame Dinghy timing deadline accounted for.
- [x] Green/yellow/red CPU acceptance bands frozen.
- [x] Dinghy integer/fixed-point and no-64-bit-division target rule frozen.
- [x] one-core six-voice `TARGET_BASE` correctness requirement frozen.
- [x] core 2 restricted to optional `TARGET_FULL` acceleration with correctness-preserving fallback.
- [x] RAM/pool/stack/code engineering margins frozen.
- [x] separate audio/timer/control/callback/UI time domains frozen.
- [x] event-aware 8253/MC5534 target strategy frozen.
- [x] 80017A quality/oversampling ladder frozen.
- [x] deterministic profiling/benchmark/release policy frozen.
- [x] **FM-1 TARGET GATE CLOSED.**

Phase-1 profiling backlog:

- [ ] actual single-core per-block ticks;
- [ ] actual linker/memory map;
- [ ] selected Q format per block;
- [ ] 80017A 1x-vs-2x error/cost;
- [ ] optional core-2 benefit/reliability;
- [ ] physical long-run overrun soak tests.

## Final gate

- [x] Every major block has an architecture-frozen reference contract.
- [x] Every unresolved physical constant is either centralized `MEASURE_PENDING` or explicitly deferred to measured target profiling.
- [x] No known generic-VA substitution is required to begin the reference engine.
- [x] Phase-1 branch boundary is documented.
- [x] **PHASE 0 ARCHITECTURE COMPLETE.**

Next branch:

```text
phase1/reference-engine
```

The next task is implementation, not further speculative circuit design.
