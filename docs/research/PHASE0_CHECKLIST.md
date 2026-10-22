# Phase 0 — JUNO-106 circuit freeze checklist

This checklist is the gate before fidelity DSP implementation.

## A. Control / firmware

- [x] Confirm master/slave CPU split.
- [x] Confirm 12 MHz slave CPU clock.
- [x] Confirm multiplexed DAC/S&H control architecture.
- [x] Confirm ~4.2 ms complete control service frame from Roland timing diagram.
- [ ] Verify exact control-frame period from ROM execution timing or direct capture.
- [x] Recover the core D7811G envelope algorithm and identify the ROM attack + decay/release tables.
- [ ] Verify complete envelope-table extraction, slider indexing, retrigger and service-loop timing against the module ROM.
- [ ] Recover LFO algorithm, waveform, rate law and delay law.
- [ ] Recover portamento law.
- [x] Confirm normal user-facing Poly 1 / Poly 2 / Solo-Unison semantics from Roland owner's documentation.
- [ ] Freeze exact Poly 1 / Poly 2 allocator, retrigger and voice-steal state machine from assigner/module firmware.

## B. DCO

- [x] Confirm 8 MHz master oscillator.
- [x] Confirm /2, /4, /8 range divider before 8253.
- [x] Confirm two 8253 devices / six counters.
- [x] Confirm key + LFO + bend + portamento + tune feed divide data.
- [x] Confirm MC5534 analogue-inspired saw/PWM/sub generation.
- [x] Confirm ~12 Vpp saw/pulse amplitude compensation target.
- [x] Confirm PWM calibration anchors.
- [x] Identify 8253 operating mode: Mode 3, binary, LSB then MSB (`36/76/B6`).
- [x] Confirm Mode-3 count relation and odd/even duty behaviour from 8253 documentation.
- [x] Locate module-ROM note-divisor table and 8.8-semitone interpolation path.
- [x] Confirm distinct ordinary count reload vs reset/control-word + reload paths.
- [ ] Finish exact pitch-build order/scaling for LFO, bender, portamento and tune.
- [ ] Determine full count-write cadence and 8253 reload timing consequences during modulation.
- [ ] Derive DCO-CV/range resistor ramp slope law from schematic.
- [ ] Freeze oscillator reset/retrigger conditions from `resetVoiceBits` / allocator tracing.

## C. Mixer / noise

- [ ] Extract saw/pulse/sub/noise source gains from schematic.
- [ ] Derive sub-level law.
- [ ] Derive noise-level law.
- [ ] Characterise noise generator spectrum and bandwidth.
- [ ] Establish pre-VCF clipping/headroom reference.

## D. 80017A VCF/VCA

- [x] Confirm IR3109 + 2x BA662 internal family/topology.
- [x] Confirm separate per-voice calibration for resonance, VCA gain, VCF frequency and VCF width.
- [ ] Freeze passive values from teardown/schematic.
- [ ] Choose first reference equations for four OTA stages.
- [ ] Derive resonance loop scaling.
- [ ] Derive VCF CV -> cutoff mapping.
- [ ] Derive VCA CV -> gain mapping.
- [ ] Define self-oscillation and saturation test vectors.

## E. HPF / global output

- [x] Confirm HPF modes: 0 bass boost, 1 flat, 2/3 HPF.
- [x] Confirm global placement after voice sum.
- [x] Confirm 106 buffering distinction from 60.
- [ ] Calculate exact mode 0 transfer function.
- [ ] Calculate modes 2/3 cutoff/Q from component values.
- [ ] Model global uPC1252 gain/headroom if audibly relevant.

## F. Chorus

- [x] Confirm dual MN3009 BBD architecture.
- [x] Confirm dual bias calibration and surrounding filters/amplifiers.
- [ ] Derive I and II oscillator/clock circuits from 106 schematic.
- [ ] Establish min/max BBD clock frequency and corresponding delay.
- [ ] Establish stereo phase relation.
- [ ] Calculate pre/de-emphasis / reconstruction bandwidth.
- [ ] Characterise noise and bias nonlinearity.
- [ ] Confirm whether any non-standard I+II state is worth DEEP-only support.

## G. FM-1 target

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
