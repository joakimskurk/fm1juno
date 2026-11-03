# JUNO-106 control timing / DAC-S&H — Phase 0 closure

State: **`CONTROL_TIMING_PHASE0_ARCHITECTURE_FROZEN`**.

This closure record supersedes older Phase-0 wording that treated exact microsecond timing as an architecture blocker.

## Architecture gate

- [x] Keep the approximately 4.2 ms Roland service-cycle family as part of strict behavior.
- [x] Reject a single atomic 236-Hz control callback.
- [x] Freeze ordered main-loop macro-causality from ROM addresses `$02EC..$07B5`.
- [x] Freeze separate early latch/RES/LFO-delay/global-VCA region.
- [x] Freeze SUB service before the six sequential DCO voice services.
- [x] Freeze per-voice timer count/reset work before each voice's DCO-CV S/H commit.
- [x] Freeze later VCF-base plus sequential ENV/VCA/VCF voice region.
- [x] Freeze late ADC/performance processing, late LFO advance and final NOISE-level service.
- [x] Freeze 12-bit physical DAC boundary from `loadDac`; common 14-bit controls lose their lowest two bits before analogue S/H.
- [x] Freeze DAC-bus load and destination MUX/S&H commit as separate conceptual events.
- [x] Freeze note-event main-loop restart behavior via stack discard / jump to `$02EB`.
- [x] Distinguish restart-class note events from normal returning serial parameter and ADC interrupts.
- [x] Keep per-S/H settling, droop and source impedance as explicit analogue state.
- [x] Centralize unresolved numerical timing in `data/juno106_control_timing_calibration_v0.yaml`.
- [x] Define machine-readable macro schedule and deterministic regression vectors.
- [x] **CONTROL TIMING PHASE-0 ARCHITECTURE GATE CLOSED.**

## Calibration / cycle-harness backlog — `MEASURE_PENDING`

- [ ] exact normal-loop mean period and workload/branch dependence;
- [ ] exact microsecond offset of every DCO-CV, VCF-CV, VCA, PWM, SUB, NOISE, RES and global-VCA S/H commit;
- [ ] exact ADC/serial interrupt latency and resulting slot displacement;
- [ ] exact VCA/PWM low-level MUX interleave where public disassembly comments are ambiguous;
- [ ] DAC output settling into each MUX load;
- [ ] S/H capacitor settling, droop, source impedance and mux charge-injection error;
- [ ] physical-hardware cross-check of note-event restart timing relative to partially completed voice loops.

These quantities replace calibration placeholders without changing the scheduler graph.

## Phase-1 permission

Phase 1 may implement an idealized timing calibration initially if and only if it preserves:

1. event order;
2. six-voice sequencing;
3. 12-bit DAC quantization at S/H boundaries;
4. note-event restart semantics;
5. explicit replaceable timing/S&H calibration fields.

It may not collapse the block into one simultaneous control-rate update.
