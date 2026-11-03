# FM1JUNO — Phase 0 complete

State: **`PHASE0_ARCHITECTURE_COMPLETE`**.

This file is the handoff from research/specification to implementation.

## What Phase 0 accomplished

The project now has frozen implementation graphs for:

```text
control / assigner / pitch / ENV / LFO
control scheduler / DAC-S&H
8253 / MC5534 DCO source
SUB / source mixer / common NOISE
A1QH80017A VCF/VCA
global HPF / uPC1252 output VCA
JUNO-106 chorus
FM-1 deployment / CPU-memory policy
```

The integrated authority is:

- `docs/PHASE0_REFERENCE_ARCHITECTURE.md`
- `docs/research/PHASE0_CHECKLIST.md`
- each block-specific `*_REFERENCE_MODEL.md` and `*_PHASE0_CLOSURE.md`.

## The remaining unknowns

Remaining unknown values are not undefined architecture. They are named calibration/profile quantities, including:

- exact physical S/H slot timing, settling and droop;
- MC5534 reset/headroom details;
- source injection ratios and noise transfer;
- vintage IR3109/BA662 nonlinear/control curves;
- HPF0 and global-VCA physical transfer;
- original chorus clock trajectories/BBD transfer/noise;
- target CPU ticks, linker map and fixed-point Q choices.

Every one has an explicit verification path in `docs/research/PHASE0_MEASUREMENT_PLAN.md` or a Phase-1 target profiling gate.

## No more speculative circuit redesign before Phase 1

Do not keep extending Phase 0 merely because a physical measurement is unavailable.

Reopen a Phase-0 graph only when new primary/hardware evidence **contradicts** the frozen structure.

Normal new measurements should update calibration data and regression vectors, not force a redesign.

## Phase-1 branch

Create implementation work from the completed Phase-0 state:

```text
phase1/reference-engine
```

Do not place production/reference DSP commits back on the research branch.

## Phase-1 first milestone

The first milestone is not audio fidelity yet. It is a deterministic executable skeleton:

```text
host build
unit-test runner
versioned event-script format
versioned trace format
calibration loader/generator
ControlScheduler106
Assigner106
Envelope106
Lfo106
Portamento106
Pitch106
Timer106
```

Exit gate:

- repeated runs are bit/deterministically identical for the integer/control trace;
- callback block size does not change the event/control result;
- physical-voice ordering is observable in trace output;
- note-event scheduler restart behavior is covered by tests;
- no FM-1 platform symbols appear in the core.

Only after that foundation passes should the high-precision MC5534 audio reference be added.

## Implementation sequence

```text
1. scaffold + tests + calibration plumbing
2. exact digital/control core
3. ordered ControlScheduler106 / 12-bit DAC-S&H boundary
4. Timer106 / 8253 event timeline
5. high-precision MC5534
6. common NOISE + source mixer
7. high-precision 80017A
8. global HPF / output VCA
9. Chorus106
10. integrated high-precision REF instrument
11. target fixed-point reductions
12. Dinghy host adapter
13. FM-1 physical profiling
14. optional core-2 TARGET_FULL acceleration
```

## Target rule that must survive Phase 1

The FM-1 hardware build is a **reduction of the reference**.

It must never become:

```text
whatever fits on FM-1 -> retroactively declared authentic
```

`TARGET_BASE` must retain six voices and strict behavioral semantics on one core. Optional core 2 may only increase measured fidelity/headroom.
