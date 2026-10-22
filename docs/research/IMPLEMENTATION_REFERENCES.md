# Implementation references — not fidelity authorities

## Purpose

This file tracks existing software that can reduce platform risk or expose useful reverse-engineering leads. None of these projects overrides the Roland service documentation or direct measurements.

## PatttF/fm-106

Public GPL-3.0-only FM-1 firmware, created in October 2026, described as a port of KR-106 to the M-VAVE FM-1.

Useful to FM1JUNO for:

- proof that a six-voice Juno-family synth is practical on FM-1;
- FM-1 dual-core/audio/UI integration patterns;
- fixed-point implementation ideas;
- host-test and screen-capture workflow;
- leads into prior Juno-106 firmware reverse engineering.

Important differences from FM1JUNO target:

- oscillator is a free-running 32-bit phase accumulator with PolyBLEP saw/pulse; FM1JUNO intends to preserve the 8 MHz -> range divider -> 8253 integer timer -> MC5534 charge/reset structure;
- current FM-106 source resets oscillator phase on note retrigger, which must not be accepted as authentic until verified against 106 hardware/firmware;
- chorus is implemented as fractional delay with filtered taps and calibrated constants rather than a full clocked BBD state model;
- filter is a practical nonlinear TPT model, not necessarily a component-complete 80017A simulation;
- source contains useful clean-room claims for D7811G control algorithms, but provenance must be checked before treating tables/constants as confirmed evidence.

Licensing:

- GPL-3.0-only. Do not copy code into FM1JUNO until FM1JUNO licensing is explicitly chosen and compatibility is accepted.
- Numerical findings independently verified from public hardware/firmware evidence may be re-derived and documented separately.

## MAME juno106 skeleton

MAME currently includes a non-working/no-sound Juno-106 skeleton driver with two uPD7810 CPUs and two PIT8253 devices. Its module memory map confirms the hardware address decoding arrangement for the two 8253 timers and records known ROM checksums.

Useful to FM1JUNO for:

- ROM/version identification;
- validating address decode and CPU/timer topology;
- potential future execution of original control firmware in a test harness if legally and practically appropriate.

Not useful as an audio reference yet because the driver is explicitly a skeleton and has no sound implementation.

## KR-106 family

Existing software-emulation work can provide hypotheses for envelope/filter/HPF/chorus constants and test vectors. Every such constant should be tagged B/C/H until traced back to hardware measurements, ROM disassembly, circuit calculation, or primary documentation.

## Reuse policy

1. Platform plumbing may be adapted only with license compatibility documented.
2. DSP equations are not accepted merely because another emulator labels them authentic.
3. Firmware-derived algorithms should be reproduced from independently documented disassembly/analysis where possible.
4. We prefer small, auditable original DSP blocks in FM1JUNO over importing a large synth engine and patching its sound afterward.
