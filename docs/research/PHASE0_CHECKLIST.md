# Phase 0 — JUNO-106 circuit freeze checklist

This checklist is the gate before fidelity DSP implementation.

The rule is now explicit:

- **architecture gates** must be closed before Phase 1;
- exact vintage-unit constants may remain `MEASURE_PENDING` only when they have a named calibration field and an explicit verification route;
- block-specific closure records override older research wording where they conflict.

## A. Control / firmware behavior

### A1. Assigner / retrigger / pitch / ENV / LFO

- [x] Confirm master/voice-CPU split and module CPU clock domain.
- [x] Freeze normal Poly 1 / Poly 2 six-voice allocation and no normal seventh-note stealing.
- [x] Freeze Solo-Unison as sequential six-voice Voice On/Off behavior over the 31.25-kbit/s serial link.
- [x] Freeze Voice On ENV re-attack from current level.
- [x] Separate ENV retrigger from DCO timer reset.
- [x] Freeze same-note/running-voice timer-phase preservation and different-note/non-running reset request behavior.
- [x] Freeze 8.8-semitone pitch construction, Tune, Bender and firmware-rate Portamento order.
- [x] Freeze 8253 Mode-3 behavior, ordinary pending reload and reset/control-word + reload paths.
- [x] Freeze DCO-CV octave/exponential law and RANGE separation.
- [x] Freeze 14-bit integer ENV architecture: additive Attack and original truncated multiply-style Decay/Release.
- [x] Freeze integer LFO accumulator/rate law, two-stage Delay and `voiceRun` retrigger semantics.
- [x] Freeze physical LFO-trigger/MIDI performance modulation bypass of automatic DCO LFO Delay scaling.

### A2. Control scheduler / physical DAC-S&H — `CONTROL_TIMING_PHASE0_ARCHITECTURE_FROZEN`

- [x] Retain Roland's approximately 4.2-ms normal multiplexed service-cycle family.
- [x] Reject one atomic ~236-Hz control callback.
- [x] Freeze ROM-derived macro-order from main-loop `$02EC..$07B5`.
- [x] Freeze early latch/RES/LFO-delay/global-VCA region.
- [x] Freeze SUB service before sequential DCO-voice service.
- [x] Freeze six sequential DCO services; each voice timer write/reset precedes that voice's DCO-CV S/H commit.
- [x] Freeze later VCF-base plus sequential ENV/VCA/VCF voice region.
- [x] Freeze late ADC/performance processing, late LFO advance and final NOISE-level S/H service.
- [x] Freeze physical DAC as 12-bit at the analogue boundary; common aligned 14-bit controls quantize as `code14 >> 2` before S/H.
- [x] Freeze DAC-bus load and destination MUX/S&H commit as separate conceptual events.
- [x] Freeze normal ADC/serial-parameter interrupts as return/resume events.
- [x] Freeze note-event restart class: voice-note handling can discard the interrupted stack and restart the main loop at `$02EB`.
- [x] Centralize exact timing/S&H calibration in `data/juno106_control_timing_calibration_v0.yaml`.
- [x] Add machine-readable schedule `data/juno106_control_schedule.csv`.
- [x] **CONTROL TIMING PHASE-0 ARCHITECTURE GATE CLOSED.**

### Control / timing backlog — not architecture blockers

- [ ] Cycle-harness or hardware-measure exact mean frame period and workload/branch dependence.
- [ ] Measure exact microsecond offsets of DCO-CV, VCF-CV, VCA, PWM, SUB, NOISE, RES and global-VCA commits.
- [ ] Quantify ADC/serial interrupt latency and slot displacement.
- [ ] Verify ambiguous low-level VCA/PWM MUX interleave/channel numbering.
- [ ] Measure DAC settling, S/H settling/droop/source impedance and mux charge injection.
- [ ] Complete ROM table transcription/index verification where exact data are required by Phase-1 regression vectors.
- [ ] Hardware/cycle-harness verify remaining Solo-Unison keyboard-edge and burst-vs-slot timing cases.

## B. DCO / MC5534 — `MC5534_PHASE0_ARCHITECTURE_FROZEN`

- [x] Confirm 8-MHz DCO master oscillator and /2, /4, /8 RANGE clock relationship before 8253.
- [x] Freeze two 8253 devices / six Mode-3 counters.
- [x] Freeze physical MC5534 graph: held DCO CV -> RANGE-selected slope resistor -> C54-family ramp -> positive-edge finite reset.
- [x] Freeze first-principles ramp law `dV/dt = -Vdco/(Rrange*Cint)`.
- [x] Freeze RANGE mapping: 16' highest R, 8' middle R, 4' lowest R.
- [x] Retain working 106 family around R85 ~399k, R86 ~200k, R87 ~100k, C54 ~1nF with evidence qualification.
- [x] Require finite/configurable reset rather than permanent instantaneous discontinuity.
- [x] Freeze SAW/PWM shared physical ramp and held PWM-CV comparator boundary.
- [x] Freeze SUB as timing-domain divide-by-two.
- [x] Keep ~12 Vp-p as hardware validation region, not per-cycle normalization.
- [x] Centralize unresolved MC5534 constants in `data/juno106_mc5534_calibration_v0.yaml`.
- [x] **MC5534 PHASE-0 ARCHITECTURE GATE CLOSED.**

### MC5534 calibration backlog

- [ ] Archive clean primary/direct R85/R86/R87/C54 values.
- [ ] Measure loaded DCO-CV code->volts, S/H impedance/droop and selector Ron.
- [ ] Capture reset duration/residual/recovery across note/range.
- [ ] Fit MC5534 buffer slew/headroom/asymmetry and comparator recovery if audibly relevant.
- [ ] Measure actual saw/pulse amplitude-error surface across keyboard/ranges.

## C. Source mixer / SUB / common NOISE — `SOURCE_MIXER_PHASE0_ARCHITECTURE_FROZEN`

- [x] Freeze per-voice WAVE + per-voice SUB + one common physical NOISE source before six 80017A inputs.
- [x] Confirm SAW/PULSE share MC5534 ramp/timing state and retain distinct gate/disable controls.
- [x] Freeze recovered MANUAL PWM integer law and held-CV boundary.
- [x] Freeze SUB LEVEL and NOISE LEVEL `raw7 << 7` digital laws.
- [x] Freeze one selected-transistor common NOISE source and dedicated IC14 BA662 noise VCA.
- [x] Retain post-noise-VCA RC shaping with ~5-kHz healthy-unit fitting target.
- [x] Retain Roland Bank-6 / TP8 / VR32 4-Vp-p system calibration vector.
- [x] Freeze same instantaneous NOISE waveform fanout to all six voices.
- [x] Reject enabled-source-count normalization and hidden pre-VCF clamping.
- [x] Centralize source gains/curves/impedances/headroom in `data/juno106_source_mixer_calibration_v0.yaml`.
- [x] **SOURCE MIXER PHASE-0 ARCHITECTURE GATE CLOSED.**

### Source-mixer calibration backlog

- [ ] Measure exact WAVE->80017A injection gain/impedance.
- [ ] Measure SUB->80017A injection and level-control curve.
- [ ] Measure common-NOISE fanout and BA662 noise-VCA curve.
- [ ] Measure healthy noise RC transfer/spectrum.
- [ ] Measure source-node DC/coupling, maximum pre-VCF excursion and source-combination compression/loading.

## D. Digital VCF control

- [x] Freeze 14-bit VCF arithmetic and final `0x0000..0x3fff` clamp.
- [x] Freeze Cutoff storage `raw7 << 7`.
- [x] Freeze calculation order: Cutoff +/- LFO +/- Bender, then per-voice +/- ENV, then signed Key Follow.
- [x] Freeze original truncated multiply primitive used by VCF modulation paths.
- [x] Freeze ENV-depth, Key-Follow coordinate/pivot, VCF-LFO and VCF-Bender arithmetic.
- [x] Freeze sequential six-voice VCF S/H service as part of `CONTROL_TIMING_PHASE0_ARCHITECTURE_FROZEN`.

Exact physical slot offsets and S/H settling now live only in the control-timing calibration backlog.

## E. A1QH80017A VCF/VCA — `80017A_PHASE0_ARCHITECTURE_FROZEN`

- [x] Confirm IR3109-family four-stage low-pass plus two BA662-class devices inside the hybrid.
- [x] Freeze four explicit IR3109-family stages and final BA662 voice VCA.
- [x] Freeze resonance BA662 topology with stage-4 feedback plus input feed-forward/Q compensation.
- [x] Require per-voice Frequency, Width, Resonance and VCA calibration surfaces.
- [x] Freeze Roland validation anchors: 4.8 Vp-p self-oscillation, 6 Vp-p post-VCA, 248-Hz C4, 992-Hz C6 and +/-10-cent interaction tolerance.
- [x] Retain opened-original-hybrid working evidence around ~68k stages, ~550-560-ohm shunts and ~250-pF capacitors.
- [x] Retain measured resonance-network family around ~1.5k / 5.1k / 100k / 3.9k with evidence qualification.
- [x] Retain measured voice-VCA family around ~4.7k / ~560 ohm / 47k plus unresolved small capacitor.
- [x] Keep AS3109/AS662 numbers as modern sanity references only.
- [x] Freeze topology-preserving implicit/TPT reference-solver direction and native self-oscillation requirement.
- [x] Centralize unresolved analogue coefficients in `data/juno106_80017a_calibration_v0.yaml`.
- [x] **80017A PHASE-0 ARCHITECTURE GATE CLOSED.**

### 80017A calibration backlog

- [ ] Fit vintage IR3109 nonlinear scale and stage-buffer headroom/asymmetry/slew.
- [ ] Fit stage capacitance/tolerance around 240-vs-250-pF evidence.
- [ ] Fit resonance passive network -> effective coefficients and BA662 control/current law.
- [ ] Measure/freeze VCF DAC/S&H code -> per-voice cutoff mapping using 248/992-Hz anchors.
- [ ] Measure/freeze BA662 voice-VCA control -> gain/headroom and offset/thump behavior.
- [ ] Decide whether slow thermal/supply dynamics justify target complexity after static behavior passes.
- [ ] Choose 1x vs selective 2x only after desktop A/B and FM-1 profiling.

## F. HPF / global output — `GLOBAL_OUTPUT_PHASE0_ARCHITECTURE_FROZEN`

- [x] Freeze HPF modes: 0 bass emphasis, 1 straight, 2/3 first-order HPF.
- [x] Freeze global placement and 106 IC4A isolation distinction from JUNO-60.
- [x] Derive HPF2 47k/15n ideal corner = 225.7516923289 Hz.
- [x] Derive HPF3 47k/4.7n ideal corner = 720.4841244540 Hz.
- [x] Freeze HPF0 as direct + two-pole/two-zero active bass-emphasis contribution.
- [x] Freeze global uPC1252H2 VCA boundary and separate Roland external-control network.
- [x] Retain high-VCA-level distortion/headroom as physical behavior.
- [x] Centralize HPF0/VCA/output calibration in `data/juno106_global_output_calibration_v0.yaml`.
- [x] **GLOBAL OUTPUT PHASE-0 ARCHITECTURE GATE CLOSED.**

### Global-output calibration backlog

- [ ] Measure/freeze exact HPF0 transfer/shelf gain and actual selector Ron.
- [ ] Measure/freeze VCA LEVEL code -> external-control -> uPC1252 gain law.
- [ ] Measure global VCA/output headroom, THD, IC4A contribution and downstream chorus loading.

## G. Chorus — `CHORUS_PHASE0_FROZEN`

- [x] Freeze global post-voice/post-HPF/global-VCA placement.
- [x] Freeze dual MN3009 + dual MN3101 stereo architecture.
- [x] Freeze independent VR1/VR2 bias-adjusted BBD paths.
- [x] Freeze opposite-polarity stereo modulation and strict OFF/I/II state.
- [x] Freeze `f_cp = f_osc/2` and MN3009 `t_delay = 128/f_cp`.
- [x] Map original timing networks A/B = `1.8k / 8.2k / 150p`.
- [x] Retain original multi-element BBD filtering rather than a generic one-pole substitute.
- [x] Derive exact IC6 summer law `Gdry=-0.39`, `Gwet=-39/47`.
- [x] Retain BBD bias/headroom/nonlinearity and separate BBD/device/clock noise.
- [x] Retain post-IC6 output/mute/jack loading and explicit mono-jack law.
- [x] Centralize all provisional/measured chorus constants in `data/juno106_chorus_calibration_v0.yaml`.
- [x] Add machine-readable pin/node/component evidence and verified output-summer SPICE reference.
- [x] **CHORUS PHASE-0 ARCHITECTURE GATE CLOSED.**

### Chorus calibration backlog

- [ ] Capture exact original-106 Mode-I and Mode-II clock/LFO trajectories.
- [ ] Measure min/mean/max `f_cp(t)`, A/B phase/mismatch and absolute delay excursion.
- [ ] Measure wet-path complex transfer and clock-dependent BBD insertion loss/bandwidth.
- [ ] Measure completed-output dry/wet contributions and mono-normal jack loading.
- [ ] Characterize BBD headroom/clipping and factory-new BBD/device/clock noise/correlation.

## H. FM-1 target — remaining Phase-0 deployment gate

- [x] Confirm 44.1-kHz / 128-frame open-SDK audio contract.
- [x] Confirm 240x240 display.
- [x] Confirm strict SRAM/audio-callback placement constraints.
- [x] Identify existing six-voice FM-106 as performance/platform reference only.
- [ ] Establish single-core baseline CPU budget by frozen reference block.
- [ ] Establish SRAM/stack/state/delay-memory budgets with safety margins.
- [ ] Choose float/fixed/mixed precision policy per block from measurable error/cost.
- [ ] Define selective oversampling/event-AA policy and worst-case six-voice cost.
- [ ] Define dual-core strategy only if single-core profiling proves it necessary/valuable.
- [ ] Freeze the Phase-1 reference-engine deployment contract.

## Gate

Phase 0 architecture is complete when every block above is either:

1. **architecture frozen**, with unresolved vintage constants represented by named calibration fields plus explicit verification tests; or
2. intentionally deferred as a Phase-1 implementation/measurement fit without changing the graph.

At this point the remaining project-wide Phase-0 blocker is **Section H: FM-1 target budget/deployment policy**.

No generic VA assumption is allowed to silently cross this gate.
