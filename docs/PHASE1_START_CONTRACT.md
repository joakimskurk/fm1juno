# FM1JUNO — Phase 1 start contract

This file is the executable handoff from Phase 0 to the first implementation branch.

Phase 1 must start from the completed Phase-0 state and preserve the frozen graph.

## Milestone P1.0 — deterministic scaffold

Create:

```text
CMakeLists.txt
src/control106/
src/timer106/
src/common/
reference/
platform/host/
tests/unit/
tests/vectors/
tools/
calibration/
```

Minimum first-build deliverables:

```text
fm1juno_core        // portable C control/timer library
fm1juno_ref         // host C++ reference shell
fm1juno_tests       // deterministic unit tests
fm1juno_render      // event-script -> trace/WAV CLI shell
```

No FM-1 adapter is required in P1.0.

## Milestone P1.1 — exact digital/control core

Implement in this order:

1. fixed absolute/fractional time primitives;
2. deterministic event queue + trace ids;
3. Assigner106;
4. Envelope106;
5. Lfo106 + LFO Delay;
6. Portamento106;
7. Pitch106 / Bender laws;
8. ControlScheduler106;
9. physical 12-bit DAC/S&H boundary;
10. Timer106 / 8253 Mode 3.

## Scheduler authority

For control-frame macro order use:

- `research/JUNO106_CONTROL_SCHEDULER_REFERENCE_MODEL.md`
- `research/data/juno106_control_schedule.csv`

Do not use the older conceptual assumption that all control values update at one ~236-Hz timestamp.

## Calibration rule

Phase 1 reads generated calibration from centralized machine-readable sources. No unresolved physical constant is hard-coded directly inside DSP implementation files.

Required build/report behavior:

```text
print/list active MEASURE_PENDING parameters
```

Host tests may use provisional defaults only when their status remains visible.

## First regression vectors

Before any analogue DSP:

- Poly 1 dynamic allocation/release order;
- Poly 2 physical first-free allocation;
- seventh-key ignored;
- same-note re-use;
- ENV current-level re-attack;
- integer Attack/Decay/Release edge vectors;
- LFO endpoint/rate/delay vectors;
- Portamento constant-step/overshoot clamp;
- DCO/VCF Bender arithmetic vectors;
- 12-bit physical DAC quantization vectors;
- scheduler normal-frame order trace;
- note-event restart mid-frame;
- parameter interrupt resume mid-frame;
- six sequential DCO service trace;
- 8253 even/odd Mode-3 vectors;
- ordinary pending count vs reset-and-load.

## P1.1 exit gate

- exact integer equality for all digital/control tests;
- deterministic trace across repeated runs;
- identical digital/control results with outer render sizes 16/32/64/128/256;
- no FM-1/Dinghy headers in core;
- no analogue-audio approximation needed to pass the gate.

Only after P1.1 passes should `wave5534_ref` begin.
