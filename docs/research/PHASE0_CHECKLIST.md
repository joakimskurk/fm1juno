# Phase 0 — JUNO-106 circuit freeze checklist

This checklist is the gate before fidelity DSP implementation.

Newer block-specific closure records supersede older unchecked wording where explicitly stated.

## A. Control / firmware

- [x] Confirm master/slave CPU split.
- [x] Confirm 12 MHz slave CPU clock.
- [x] Confirm multiplexed DAC/S&H control architecture.
- [x] Confirm ~4.2 ms complete control service frame from Roland timing diagram.
- [ ] Verify exact control-frame period from ROM execution timing or direct capture.
- [x] Map the ordered module main-loop pipeline so the ~4.2 ms frame is not treated as one atomic control update.
- [x] Confirm prior-LFO/current-frame causality: early pitch/VCF calculation uses the stored LFO state before the accumulator is advanced near the end of the pass.
- [x] Confirm panel Tune/Bender/Portamento/LFO-trigger ADC processing occurs after current-pass DCO/VCF servicing and therefore naturally feeds a later pass.
- [ ] Calibrate fractional in-frame timing of the important DAC/S&H and per-voice DCO slots; quantify serial/ADC interrupt jitter.
- [x] Recover the core D7811G envelope algorithm and identify the ROM attack + decay/release tables.
- [x] Confirm Voice On re-arms Attack and that Attack continues from the current envelope level rather than forcing ENV to zero.
- [ ] Verify complete envelope-table extraction, slider indexing and exact service-loop timing against the module ROM.
- [x] Recover LFO accumulator/direction/polarity algorithm and rate mapping.
- [x] Replace the 128-entry LFO-rate dependency with an exact compact piecewise integer coefficient law.
- [x] Recover two-stage LFO Delay structure: holdoff + digital depth ramp.
- [x] Freeze normal automatic LFO-Delay retrigger semantics: arm only when `voiceRun` becomes empty; restart on first subsequent running voice; legato/overlap/sustain do not retrigger.
- [x] Confirm physical LFO-trigger/MIDI performance modulation is combined after automatic DCO LFO Delay scaling and therefore bypasses the panel Delay.
- [ ] Calibrate exact main-loop period/workload variation and physical trigger ADC threshold/hysteresis.
- [x] Recover Portamento as per-voice constant-step 8.8-pitch-domain glide with overshoot clamp.
- [x] Recover Portamento control indexing: non-zero 8-bit control -> right shift -> 0..127 coefficient index.
- [x] Confirm normal user-facing Poly 1 / Poly 2 / Solo-Unison semantics from Roland owner's documentation.
- [x] Confirm normal six-voice limit: a seventh simultaneously held key is ignored, not voice-stolen.
- [x] Freeze normal Poly 1 allocator: dynamic physical-voice order, released voice moves to back, same-note free-voice reuse.
- [x] Freeze normal Poly 2 allocator: fixed physical first-free scan, no release-age reordering.
- [x] Freeze normal Poly 1 / Poly 2 note-off behavior and relation to natural release.
- [x] Confirm normal Solo-Unison sends the same Voice On to all six physical voices and Voice Off to all six without rewriting Poly 1/2 allocation tables.
- [x] Recover Solo-Unison `$FFD0` as a per-note-scan arbitration latch rather than persistent held-note state.
- [x] Freeze Solo-Unison sequential new-note priority, one-event-per-scan arbitration and note-off -> clear-history -> held-note reconstruction behavior.
- [x] Confirm truly simultaneous Solo-Unison changes are scan-order dependent rather than a generic timestamped note stack.
- [x] Confirm Solo-Unison serial link timing: 31.25 kbit/s; nominal 13-byte Note On burst ~= 4.16 ms and 7-byte Note Off burst ~= 2.24 ms.
- [x] Freeze normal Solo-Unison timer-reset semantics: first different note from non-running state requests independent resets; legato different-note changes while running preserve timer phase and still re-arm ENV.
- [ ] Hardware/cycle-harness verification of exact Solo-Unison fallback scan priority at keyboard range edges and burst-vs-DCO-slot reset staggering.

## B. DCO / MC5534 — `MC5534_PHASE0_ARCHITECTURE_FROZEN`

Architecture / implementation gate:

- [x] Confirm 8 MHz master oscillator.
- [x] Confirm /2, /4, /8 range divider before 8253.
- [x] Confirm two 8253 devices / six counters.
- [x] Confirm key + LFO + bend + portamento + tune feed divide data.
- [x] Identify 8253 Mode 3, binary, LSB then MSB (`36/76/B6`).
- [x] Confirm ordinary pending count vs reset/control-word + reload paths.
- [x] Freeze pitch-build order in 8.8 semitone space.
- [x] Recover exact DCO Bender normalization and DCO-CV octave/exponential law.
- [x] Confirm DCO-CV zero calibration at TP3.
- [x] Freeze physical MC5534 graph: held DCO CV -> RANGE-selected resistor -> C54-family ramp -> positive-edge finite reset.
- [x] Freeze first-principles ramp law `dV/dt = -Vdco/(Rrange*Cint)`.
- [x] Freeze RANGE mapping: 16' highest R, 8' middle R, 4' lowest R.
- [x] Establish working original-106 value family: R85 ~= 399k, R86 ~= 200k, R87 ~= 100k, C54 ~= 1nF, with evidence qualification retained.
- [x] Require finite/configurable reset rather than permanent instantaneous discontinuity.
- [x] Freeze SAW/PWM shared physical ramp and held PWM-CV comparator boundary.
- [x] Freeze SUB as timing-domain divide-by-two.
- [x] Keep ~12 Vp-p as an approximate hardware validation target, not per-cycle normalization.
- [x] Centralize all unresolved MC5534 constants in `data/juno106_mc5534_calibration_v0.yaml`.
- [x] Define deterministic note/range/reset/PWM/unison validation vectors.
- [x] **MC5534 PHASE-0 ARCHITECTURE GATE CLOSED.**

### MC5534 calibration backlog — `MEASURE_PENDING`, not architecture blockers

- [ ] Verify full control-loop/count-write cadence under representative workloads.
- [ ] Archive clean primary/direct values for R85/R86/R87 and C54.
- [ ] Measure loaded DCO-CV code -> volts law, S/H source impedance/droop and selector Ron.
- [ ] Capture reset duration/residual/recovery across note/range.
- [ ] Fit MC5534 buffer slew/headroom/asymmetry and comparator recovery if audibly relevant.
- [ ] Measure the actual saw/pulse amplitude-error surface across keyboard/ranges.

## C. Source mixer / noise — `SOURCE_MIXER_PHASE0_ARCHITECTURE_FROZEN`

Architecture / implementation gate:

- [x] Freeze per-voice WAVE + per-voice SUB + one common physical NOISE source before six 80017A inputs.
- [x] Confirm SAW and PULSE share MC5534 ramp/timing state.
- [x] Confirm SAW gate and PULSE/PWM disable are distinct controls.
- [x] Recover exact MANUAL PWM integer law and held-CV boundary.
- [x] Recover SUB LEVEL and NOISE LEVEL `raw7 << 7` laws.
- [x] Freeze one selected-transistor common NOISE source and dedicated IC14 BA662 noise VCA.
- [x] Retain post-noise-VCA RC shaping with ~5 kHz healthy-unit fitting target.
- [x] Freeze Roland Bank-6 / TP8 / VR32 4 Vp-p system calibration vector.
- [x] Freeze same instantaneous NOISE waveform fanout to all six voices.
- [x] Retain source impedance/coupling/headroom boundary instead of an ideal normalized adder.
- [x] Reject enabled-source-count normalization and hidden pre-VCF clamping.
- [x] Centralize source gains/curves/impedances/headroom in `data/juno106_source_mixer_calibration_v0.yaml`.
- [x] **SOURCE MIXER PHASE-0 ARCHITECTURE GATE CLOSED.**

### Source-mixer calibration backlog — `MEASURE_PENDING`

- [ ] Measure/freeze exact WAVE -> 80017A injection gain/impedance.
- [ ] Measure/freeze exact SUB -> 80017A injection gain/impedance and level-control curve.
- [ ] Measure/freeze exact common-NOISE fanout gain/impedance and BA662 control curve.
- [ ] Measure healthy noise RC transfer/spectrum.
- [ ] Measure source-node DC/coupling, maximum pre-VCF excursion and source-combination compression/loading.

## D. Digital VCF control

- [x] Confirm 14-bit VCF control domain and final `0x0000..0x3fff` clamp.
- [x] Recover exact Cutoff storage law: `cutoff7 << 7` -> `0x0000..0x3f80`.
- [x] Freeze calculation order: `Cutoff +/- LFO +/- Bender`, then per-voice `+/- ENV`, then signed Key Follow.
- [x] Recover shared `mul16x8_hi(value,m) = floor(value*m/256)` primitive used by VCF control paths.
- [x] Recover ENV depth law (`raw7 << 1`) and signed per-voice ENV contribution.
- [x] Recover Key Follow coordinate: `(pitch>>2)+(pitch>>3)`, Middle-C pivot `0x1680`, `raw7<<1` depth and signed above/below-C4 contribution.
- [x] Recover VCF-LFO law and integrate exact VCF Bender law.
- [ ] Verify exact ordering/spacing of six VCF S/H writes and DAC/S&H settling inside the full service frame.

## E. A1QH80017A VCF/VCA — `80017A_PHASE0_ARCHITECTURE_FROZEN`

Architecture / implementation gate:

- [x] Confirm physical hybrid contains IR3109 + 2x BA662-class devices plus passives.
- [x] Freeze four explicit IR3109-family low-pass stages and final BA662 voice VCA.
- [x] Freeze resonance BA662 topology with both stage-4 feedback and input feed-forward/Q compensation.
- [x] Require separate per-voice Frequency, Width, Resonance and VCA calibration surfaces.
- [x] Freeze Roland service anchors: 4.8 Vp-p self-oscillation, 6 Vp-p post-VCA, 248 Hz C4, 992 Hz C6, +/-10 cents after iterative Frequency/Width adjustment.
- [x] Retain working opened-hybrid passive evidence: ~68k stages, ~550-560 ohm shunts, ~250 pF capacitors, with evidence qualification.
- [x] Retain measured resonance-network family: ~1.5k local shunts, ~5.1k input feed-forward, ~100k stage-4 feedback, ~3.9k additional input path.
- [x] Retain measured voice-VCA family: ~4.7k input, ~560-ohm local shunt, ~47k output/load branch with unresolved small capacitor.
- [x] Keep modern AS3109/AS662 electrical numbers as sanity references only, not vintage truth.
- [x] Freeze topology-preserving implicit/TPT reference solver direction and native self-oscillation requirement.
- [x] Define self-oscillation, large-signal, stage-nonlinearity, 240-vs-250-pF and 1x-vs-2x reference test vectors.
- [x] Centralize all unresolved analogue coefficients in `data/juno106_80017a_calibration_v0.yaml`.
- [x] **80017A PHASE-0 ARCHITECTURE GATE CLOSED.**

### 80017A calibration backlog — `MEASURE_PENDING`, not architecture blockers

- [ ] Fit vintage IR3109 differential nonlinear scale and stage-buffer headroom/asymmetry/slew.
- [ ] Fit effective stage capacitance/tolerance distribution around the 240-vs-250-pF evidence.
- [ ] Fit resonance passive network -> effective feed-forward/feedback/direct coefficients and BA662 control/current law.
- [ ] Measure/freeze VCF DAC/S&H code -> per-voice cutoff mapping using the 248/992-Hz anchors.
- [ ] Measure/freeze BA662 voice-VCA control -> gain/headroom curve and offset/thump behavior.
- [ ] Decide whether slow warmed-up thermal/supply dynamics justify model complexity after static behavior passes.
- [ ] Choose 1x vs selective 2x deployment only after desktop A/B and target profiling.

## F. HPF / global output — `GLOBAL_OUTPUT_PHASE0_ARCHITECTURE_FROZEN`

- [x] Confirm HPF modes: 0 bass boost, 1 flat, 2/3 first-order HPF.
- [x] Confirm global placement after voice sum and 106 IC4A buffering distinction from 60.
- [x] Derive HPF2 ideal 47k/15n corner = 225.7516923289 Hz.
- [x] Derive HPF3 ideal 47k/4.7n corner = 720.4841244540 Hz.
- [x] Correct older `cutoff/Q` wording: Q is not applicable to first-order HPF2/3 branches.
- [x] Freeze HPF0 architecture as direct + two-pole/two-zero bass-emphasis contribution into IC4A.
- [x] Freeze global uPC1252H2 current-mode VCA boundary and separate Roland external-control network.
- [x] Retain high-VCA-level distortion/headroom as physical behaviour.
- [x] Centralize HPF0/VCA/output calibration in `data/juno106_global_output_calibration_v0.yaml`.
- [x] **GLOBAL OUTPUT PHASE-0 ARCHITECTURE GATE CLOSED.**

### Global-output calibration backlog — `MEASURE_PENDING`

- [ ] Measure/freeze exact HPF0 transfer coefficients/shelf gain and actual selector Ron.
- [ ] Measure/freeze VCA LEVEL code -> external-control -> uPC1252 gain law.
- [ ] Measure global VCA/output headroom, THD, IC4A contribution and downstream chorus loading.

## G. Chorus — `CHORUS_PHASE0_FROZEN`

Architecture / implementation gate:

- [x] Confirm global post-voice/post-HPF/global-VCA chorus placement on JACK BOARD.
- [x] Confirm dual MN3009 BBD architecture and dual MN3101 driver/clock architecture.
- [x] Confirm two separately bias-adjusted BBD paths with Roland TP1/TP2 + VR1/VR2 service procedure.
- [x] Freeze stereo architecture as two physical delay paths with opposite modulation polarity; nominal 180-degree relationship.
- [x] Confirm dry signal is mixed with delayed signal; reject a 100%-wet strict 106 chorus model.
- [x] Freeze strict original user modes as OFF / I / II; do not import Juno-6/60 I+II behavior into strict 106.
- [x] Establish triangle-like/non-ideal driver trajectory requirement from physical-106 capture work.
- [x] Freeze unambiguous clock domains: `f_cp = f_osc/2` and MN3009 `t_delay = 128/f_cp`.
- [x] Map original mirrored MN3101 timing networks: A `R123=1.8k / R124=8.2k / C53=150p`; B `R136=1.8k / R137=8.2k / C57=150p`.
- [x] Map original mirrored BBD audio/filter component families from the Roland jack-board schematic.
- [x] Require multi-element pre/post BBD filtering; reject a generic Juno-6-derived one-pole filter as strict 106 truth.
- [x] Derive exact original IC6 dry/wet summer law: `Gdry=-39/100=-0.39`, `Gwet=-39/47=-0.829787234...` per channel before later output/loading effects.
- [x] Keep approximately 55% dry / 45% wet only as a completed-output 106-reconstruction calibration target, not normalized DSP coefficients.
- [x] Correct output-region schematic transcription: `R54/R57 = 1.5 kOhm`, superseding the earlier 15-kOhm misread.
- [x] Freeze separate BBD bias/headroom/nonlinearity state and factory symmetrical-clipping calibration rule.
- [x] Separate BBD/device/clock noise from the front-panel synthesizer NOISE source.
- [x] Retain post-IC6 output/mute/jack loading as its own physical reference block.
- [x] Define stereo renderer and explicit original-jack-law mono renderer contracts.
- [x] Centralize all provisional/measured chorus constants in `data/juno106_chorus_calibration_v0.yaml`.
- [x] Add verified original-output-summer SPICE subcircuit and deterministic reference invariants.
- [x] Define a physical-106 measurement plan for every remaining clock/delay/filter/gain/headroom/noise constant.
- [x] Freeze implementation contract in `JUNO106_CHORUS_REFERENCE_MODEL.md`.
- [x] **CHORUS PHASE-0 ARCHITECTURE GATE CLOSED.**

### Chorus calibration backlog — `MEASURE_PENDING`, not architecture blockers

- [ ] Capture exact original-106 Mode-I clock/LFO trajectory and rate.
- [ ] Capture exact original-106 Mode-II clock/LFO trajectory and rate.
- [ ] Measure min/mean/max `f_cp(t)` per channel/mode and A/B phase/mismatch.
- [ ] Validate center/min/max absolute audio delay against the exact `128/f_cp` identity.
- [ ] Measure complex wet-path transfer and clock-dependent BBD insertion loss/bandwidth.
- [ ] Measure completed-output dry/wet voltage contributions and exact mono-normal jack gain/loading.
- [ ] Characterise factory-calibrated BBD positive/negative headroom and nonlinear clipping curve.
- [ ] Characterise factory-new BBD/device/clock noise spectra and L/R correlation.

## H. FM-1 target

- [x] Confirm 44.1 kHz / 128-frame open-SDK audio contract.
- [x] Confirm 240x240 display.
- [x] Confirm strict SRAM/audio-callback placement constraints.
- [x] Identify existing six-voice FM-106 as performance/platform reference.
- [ ] Establish single-core baseline budget for reference blocks.
- [ ] Establish dual-core strategy only after single-core block profiling.
- [ ] Decide fixed-point vs mixed precision per block.
- [ ] Define CPU, SRAM and delay-memory budgets with safety margins.

## Gate

Phase 1 fidelity implementation begins only when unresolved items either:

1. have evidence-backed answers, or
2. have a written hypothesis plus an explicit measurement/verification test.

No generic VA assumption is allowed to silently cross this gate.
