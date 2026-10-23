# Phase 0 — JUNO-106 circuit freeze checklist

This checklist is the gate before fidelity DSP implementation.

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
- [x] Confirm physical LFO-trigger/MIDI performance modulation is combined after automatic DCO-LFO Delay scaling and therefore bypasses the panel Delay.
- [ ] Calibrate exact main-loop period/workload variation and physical trigger ADC threshold/hysteresis.
- [x] Recover Portamento as per-voice constant-step 8.8-pitch-domain glide with overshoot clamp.
- [x] Recover Portamento control indexing: non-zero 8-bit control -> right shift -> 0..127 coefficient index.
- [x] Confirm normal user-facing Poly 1 / Poly 2 / Solo-Unison semantics from Roland owner's documentation.
- [x] Confirm normal six-voice limit: a seventh simultaneously held key is ignored, not voice-stolen.
- [x] Freeze normal Poly 1 allocator: dynamic voice-order queue, release-to-back ordering and same-note free-voice reuse.
- [x] Freeze normal Poly 2 allocator: fixed physical first-free scan, no release-age reordering.
- [x] Freeze normal Poly 1 / Poly 2 note-off behavior and relation to natural release.
- [x] Confirm normal Solo-Unison sends the same Voice On to all six physical voices and Voice Off to all six without rewriting Poly 1/2 allocation tables.
- [x] Recover Solo-Unison `$FFD0` as a per-note-scan arbitration latch rather than persistent held-note state.
- [x] Freeze Solo-Unison sequential new-note priority, one-event-per-scan arbitration and note-off -> clear-history -> held-note reconstruction behavior.
- [x] Confirm truly simultaneous Solo-Unison changes are scan-order dependent rather than a generic timestamped note stack.
- [x] Confirm Solo-Unison serial link timing: 31.25 kbit/s; nominal 13-byte Note On burst ~= 4.16 ms and 7-byte Note Off burst ~= 2.24 ms.
- [x] Freeze normal Solo-Unison timer-reset semantics: first different note from non-running state requests independent resets; legato different-note changes while running preserve timer phase and still re-arm ENV.
- [ ] Hardware/cycle-harness verification of exact Solo-Unison fallback scan priority at keyboard range edges and burst-vs-DCO-slot reset staggering.

## B. DCO

- [x] Confirm 8 MHz master oscillator.
- [x] Confirm /2, /4, /8 range divider before 8253.
- [x] Confirm two 8253 devices / six counters.
- [x] Confirm key + LFO + bend + portamento + tune feed divide data.
- [x] Confirm MC5534 analogue-inspired saw/PWM/sub generation.
- [x] Confirm ~12 Vpp saw/pulse amplitude-compensation target.
- [x] Confirm PWM calibration anchors.
- [x] Identify 8253 operating mode: Mode 3, binary, LSB then MSB (`36/76/B6`).
- [x] Confirm Mode-3 count relation and odd/even duty behavior from 8253 documentation.
- [x] Locate module-ROM note-divisor table and 8.8-semitone interpolation path.
- [x] Confirm distinct ordinary count reload vs reset/control-word + reload paths.
- [x] Freeze pitch-build order: per-voice Portamento pitch + global Base/Tune/LFO/Bender offset -> divisor/DCO-CV conversion.
- [x] Derive Tune law in 8.8 pitch domain (~+/-127/256 semitone = ~+/-49.6 cents).
- [x] Confirm LFO pitch and Bender enter the same global 8.8 pitch offset before per-voice table conversion.
- [x] Recover exact DCO Bender normalization: `p=b*s`, `q=(p>>1)+(p>>2)+(p>>8)`, `bend8_8=q>>4`; maximum = 3063/256 = 11.965 semitones.
- [x] Recover VCF Bender normalization: `(b * sensitivity) >> 4` in the 12-bit control domain.
- [x] Confirm ordinary Mode-3 count update is deferred to the next half-cycle/output transition rather than replacing the active half-cycle immediately.
- [x] Freeze oscillator reset decision: different note + not `voiceRun` requests reset; same note and/or running voice preserves timer phase.
- [x] Confirm timer reset is independent from envelope re-attack.
- [x] Confirm reset requests are only flags in the serial ISR and are consumed later in sequential physical-voice DCO service slots.
- [x] Derive digital DCO-CV note law: approximately `round(32 * 2^(n/12))`, octave-doubling until DAC saturation, with fractional interpolation.
- [x] Confirm Roland DCO-CV zero-offset calibration: DAC forced to 0 V and VR33 trimmed to 0 V at TP3.
- [x] Confirm physical MC5534 source topology: held DCO CV -> RANGE-selected resistor -> ramp capacitor; positive DCO edge initiates capacitor discharge/reset; PWM compares the same physical ramp against held PWM CV; SUB derives from DCO timing divide-by-two.
- [x] Freeze first-principles ramp law `dV/dt = -Vdco/(Rrange*Cint)` and amplitude condition `|DeltaV| ~= |Vdco|/(Rrange*Cint*f)`.
- [x] Derive ideal RANGE-network scaling requirement: conductance 16':8':4' ~= 1:2:4, therefore resistance ratio R16:R8:R4 ~= 4:2:1 before switch/source-impedance corrections.
- [x] Establish working 106 resistor set and mapping as schematic/cross-check evidence: 16'/R85 ~= 399 kOhm, 8'/R86 ~= 200 kOhm, 4'/R87 ~= 100 kOhm. Keep configurable until a clean primary designator/value read is archived.
- [x] Triangulate `Cint ~= 1 nF` independently from ROM DCO-CV codes/divisors + Roland 0..-10 V conditioned DCO-CV range + resistor set + ~12 Vpp target.
- [x] Reject per-cycle amplitude normalization: strict model should naturally retain small note/range level error around the ~12 Vpp design target.
- [x] Require architecturally finite/configurable ramp reset rather than permanently assuming an instantaneous mathematical saw discontinuity.
- [ ] Verify full control-loop/count-write cadence under representative workloads.
- [ ] Archive a clean primary value read or direct board measurement for R85/R86/R87 and C54.
- [ ] Determine strict 106 MC5534 reset-network values, reset-transistor conduction time, residual voltage and recovery/slew behavior; do not import 60 values silently.
- [ ] Measure DCO-CV DAC/S&H loaded voltage law, per-voice droop/output impedance and 4052 on-resistance contribution.
- [ ] Measure the actual saw-amplitude error curve across keyboard/ranges and fit model tolerance/headroom.

## C. Source mixer / noise

- [x] Freeze source topology as per-voice MC5534 WAVE node + separate per-voice SUB node + one common physical NOISE source feeding all six voice input paths.
- [x] Confirm SAW and PULSE share the MC5534 ramp/timing system; do not model them as unrelated free-running oscillators.
- [x] Confirm SAW gating and PULSE/PWM disabling are separate hardware/control actions.
- [x] Recover exact MANUAL PWM integer law: `pwm_depth = raw7<<1`, `pwm_code = 0x3fff - floor(0x3fff*pwm_depth/256)`; PULSE OFF forces code 0.
- [x] Cross-check PWM firmware against Roland analogue anchors: code 0 -> comparator-off region (~-0.8 V); full reference -> ~+6 V / 50%; panel maximum predicts ~+0.69 V, close to service ~+0.6 V / 95%.
- [x] Recover SUB LEVEL firmware law: `raw7 << 7` -> `0x0000..0x3f80`.
- [x] Confirm SUB amplitude is controlled through oscillator/transistor collector-supply behavior rather than assuming a post-oscillator normalized digital gain.
- [x] Recover NOISE LEVEL firmware law: `raw7 << 7` -> `0x0000..0x3f80`.
- [x] Confirm only one common physical noise generator; strict engine must not instantiate six independent noise streams.
- [x] Confirm selected 2SC945P noise transistor in Roland parts list.
- [x] Establish ~5 kHz post-noise-VCA bandwidth target from original-hardware repair evidence; retain exact RC constants as open pending primary extraction.
- [x] Freeze Roland factory noise anchor: after VCA Gain adjustment, Bank 6 / held key / TP8 -> adjust VR32 for 4 Vp-p.
- [x] Reject enabled-source-count normalization before VCF; preserve hardware-voltage drive/headroom.
- [ ] Extract/freeze exact per-voice WAVE-to-80017A, SUB-to-80017A and common-NOISE fan-out mixer resistor/gain values from clean schematic or measurement.
- [ ] Derive/freeze SUB analogue control-voltage-to-amplitude transfer curve.
- [ ] Derive/freeze NOISE control-voltage-to-amplitude transfer curve and exact RC spectral shape.
- [ ] Establish maximum pre-VCF voltage, DC/coupling behavior and clipping/compression mechanism for individual and combined sources.
- [ ] Measure whether source-dependent loading at the 80017A input is significant.

## D. Digital VCF control

- [x] Confirm 14-bit VCF control domain and final `0x0000..0x3fff` clamp.
- [x] Recover exact Cutoff storage law: `cutoff7 << 7` -> `0x0000..0x3f80`.
- [x] Freeze calculation order: `Cutoff +/- LFO +/- Bender`, then per-voice `+/- ENV`, then signed Key Follow.
- [x] Recover shared `mul16x8_hi(value,m) = floor(value*m/256)` primitive used by VCF control paths.
- [x] Recover ENV depth law (`raw7 << 1`) and signed per-voice ENV contribution.
- [x] Recover Key Follow coordinate: `(pitch>>2)+(pitch>>3)`, Middle-C pivot `0x1680`, `raw7<<1` depth and signed above/below-C4 contribution.
- [x] Recover VCF-LFO law: doubled panel depth -> LFO-Delay onset scaling -> 16x8 magnitude multiply -> divide by two; maximum strict control magnitude = 4047 before sign.
- [x] Integrate exact VCF Bender law from Bender path.
- [ ] Verify exact ordering/spacing of six VCF S/H writes and DAC/S&H settling inside the full service frame.

## E. A1QH80017A VCF/VCA

- [x] Confirm physical hybrid contains IR3109 + 2x BA662-class devices plus passives.
- [x] Confirm four-pole 24 dB/oct low-pass structure and final BA662 voice VCA.
- [x] Confirm resonance BA662 topology includes both stage-4 feedback and input feed-forward/Q compensation; reject a generic `x-k*y4`-only resonance model.
- [x] Confirm separate per-voice calibration for Resonance, VCA Gain, VCF Frequency and VCF Width.
- [x] Freeze Roland service validation anchors: 4.8 Vp-p self-oscillation, 6 Vp-p post-VCA, 248 Hz C4 frequency anchor, 992 Hz C6 width anchor, +/-10 cent interaction tolerance.
- [x] Establish exposed shared JUNO-6/60 stage family as 68 kOhm / 240 pF and keep exact 106 hybrid passives separately qualified.
- [x] Record direct-repair measurement hypothesis of ~68 kOhm / ~550-560 ohm / ~250 pF inside opened 80017A units without promoting it to primary truth.
- [x] Confirm modern 3109-family architectural constraint that stage buffers are not identical (1/3 passive-load vs 2/4 active-load behavior).
- [x] Choose topology-preserving first reference structure: four explicit nonlinear OTA one-poles + explicit BA662 Q-compensation/resonance block + explicit BA662 VCA.
- [ ] Freeze vintage IR3109/80017A nonlinear scale, buffer headroom/asymmetry and exact 106 passive set from measurement/fitting.
- [ ] Derive/freeze numerical resonance-loop resistor/gain scaling for strict 106.
- [ ] Derive/freeze VCF DAC/S&H code -> IR3109 control-voltage/frequency mapping; use Frequency/Width calibration anchors rather than guessing mV/bit.
- [ ] Derive/freeze VCA CV -> BA662 gain mapping.
- [x] Define self-oscillation, large-signal, stage-nonlinearity, 240-vs-250-pF and 1x-vs-2x reference test vectors.
- [ ] Decide whether warmed-up thermal/supply dynamics have enough audible value to model after static reference behavior passes.

## F. HPF / global output

- [x] Confirm HPF modes: 0 bass boost, 1 flat, 2/3 HPF.
- [x] Confirm global placement after voice sum.
- [x] Confirm 106 buffering distinction from 60.
- [ ] Calculate exact mode 0 transfer function.
- [ ] Calculate modes 2/3 cutoff/Q from component values.
- [ ] Model global uPC1252 gain/headroom if audibly relevant.

## G. Chorus

- [x] Confirm global post-voice/post-HPF chorus placement on JACK BOARD.
- [x] Confirm dual MN3009 BBD architecture and dual MN3101 driver/clock architecture.
- [x] Confirm two separately bias-adjusted BBD paths with Roland TP1/TP2 + VR1/VR2 service procedure.
- [x] Freeze stereo architecture as two physical delay paths with opposite modulation polarity; nominal 180-degree relationship.
- [x] Confirm dry signal is mixed with the delayed signal; reject a 100%-wet strict 106 chorus model.
- [x] Establish 55% dry / 45% wet as a 106-focused hardware-reconstruction working calibration, pending direct original-unit gain measurement.
- [x] Confirm one physical chorus topology is reused by I and II; treat mode difference primarily as LFO/clock preset until contrary evidence appears.
- [x] Establish triangle-like driver modulation from direct physical-106 clock capture work; retain non-ideal trajectory capability rather than forcing a perfect mathematical triangle.
- [x] Freeze strict original user modes as OFF / I / II; do not import Juno-6/60 I+II fast-mode behavior into strict 106.
- [x] Require clock-domain-to-delay modeling with an explicit MN3009 clock convention; do not bury an unknown factor-of-two in a generic delay formula.
- [x] Require pre-BBD and post-BBD filtering; do not import KR106's published Juno-6 filter fit as a strict 106 constant.
- [x] Separate chorus BBD/device/clock noise from the front-panel synthesizer NOISE source.
- [x] Define a physical-106 measurement plan for driver clocks, absolute delay, dry/wet gain, filtering, bias/headroom and noise.
- [ ] Freeze exact original-106 Mode I LFO/clock rate from direct capture.
- [ ] Freeze exact original-106 Mode II LFO/clock rate from direct capture.
- [ ] Establish min/mean/max BBD clock frequency and exact clock-to-delay convention.
- [ ] Establish center/min/max delay per channel and measured L/R phase error from ideal 180 degrees.
- [ ] Calculate/freeze exact 106 pre/post BBD filter transfer functions and clock-dependent bandwidth.
- [ ] Verify original 106 dry/wet analogue voltage gains and mono-summing gain.
- [ ] Characterise factory-calibrated BBD bias/headroom/nonlinear clipping curve.
- [ ] Characterise factory-new chorus noise, clock feedthrough and L/R noise correlation.
- [ ] Decide whether any undocumented physical two-button state is stable enough to preserve as an explicitly DEEP-only curiosity.

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
