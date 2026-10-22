# Phase 0 worklog — 2026-10-22

## Completed

- Established JUNO-106 as primary reference target.
- Added evidence hierarchy and research policy.
- Reviewed original Roland JUNO-106 service-notes block diagram, CPU/control descriptions, module-board timing, MC5534 wave-generator description, 80017A block diagram, module schematic, jack-board chorus/output schematic and adjustment procedures.
- Established the 106-specific architecture around 8 MHz master clock, /2 /4 /8 range divider, six 8253 counters, sampled composite control voltages and MC5534 waveform formation.
- Established primary-source ~4.2 ms DAC/S&H service frame.
- Established ~12 Vpp DCO saw/pulse compensation target and PWM calibration anchors.
- Confirmed 80017A topology family with physical teardown evidence.
- Confirmed 106 HPF position 0 bass boost and position 1 flat semantics.
- Confirmed dual MN3009 chorus hardware and bias calibration path.
- Reviewed MAME Juno-106 skeleton for CPU/timer topology and ROM identity.
- Reviewed current PatttF/fm-106 FM-1 implementation as a platform/performance reference and identified its key abstraction differences from FM1JUNO.

## Architectural decisions made

1. `MODEL_106` is the fidelity baseline.
2. JUNO-60 features enter strict 106 mode only when shared topology is proven.
3. DCO is event/timer/charge-reset based in the reference architecture, not a generic six-phase-accumulator VA oscillator.
4. 106 control-rate stepping is an intentional model feature.
5. Envelope, filter and chorus constants remain evidence-tagged until provenance is clear.
6. Existing GPL FM-106 code is research/reference only until FM1JUNO licensing is deliberately chosen.

## Next research slice

Highest-value next work:

1. module ROM analysis for 8253 programming mode/count generation;
2. envelope algorithm/table provenance verification;
3. LFO + portamento control algorithm;
4. exact module-board source mixer and DCO-CV transfer extraction;
5. 106 jack-board HPF and chorus circuit calculation.

The first DSP prototype should wait until items 1–3 are substantially understood, because they define the digital-control character unique to the JUNO-106.
