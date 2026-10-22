# Source index

This file records the sources used for Phase 0 and what each source is allowed to establish.

## Primary Roland sources

### JUNO-106 Service Notes — First Edition, 31 July 1984

Use for:

- block diagram and board topology;
- master/slave CPU task split;
- module-board pin functions;
- 8 MHz DCO master oscillator and range divider;
- dual 8253 timer architecture;
- DCO divide-data description;
- shared DAC + S/H control architecture and timing diagram;
- MC5534 wave generator operation;
- ~12 Vpp wave amplitude-compensation intent;
- PWM CV/duty anchors;
- 80017A block structure;
- calibration procedures;
- jack-board HPF/output/chorus schematic;
- factory test/voice-assignment behaviour.

Primary PDF mirror used during current research:
`https://www.kiwitechnics.com/downloads/Kiwi-106/Roland%20Juno-106%20Service%20Manual.pdf`

Cross-check mirrors:

- Synfo Roland service-manual archive
- Synthfool Roland archive
- BAMA manual archive

### JUNO-106 Owner's Manual

Use for:

- user-facing parameter semantics;
- authentic front-panel modes;
- HPF position descriptions;
- Poly 1 / Poly 2 performance semantics;
- normal chorus operation;
- controller behaviour.

Roland-hosted PDF:
`https://cdn.roland.com/assets/media/pdf/JUNO-106_OM.pdf`

### Roland current JUNO-106 technical-specification archive

Use as a convenient cross-check for nominal ranges and feature list, not as a replacement for the original service manual.

## Hardware / teardown evidence

### Obsolete Technology — 80017A teardown

Use for physical confirmation that the hybrid contains one IR3109-family filter IC, two BA662 VCA devices, passive components and printed resistors.

### Analogue Renaissance — 80017A work / Juno test procedures

Use for teardown/repair-derived implementation clues and practical test-point knowledge. Treat exact DSP constants as secondary until independently verified.

## Circuit-analysis sources

### Sequence 15 — Juno-60 vs Juno-106

Use for hypotheses and circuit interpretation around the global HPF/output-loading differences. Verify calculations against schematics before freezing constants.

## Software / reverse-engineering references

### MAME `src/mame/roland/juno106.cpp`

Use for CPU/timer address-map cross-checks, ROM identity/checksums and future control-firmware harness work. Current driver is a skeleton and not an audio reference.

### PatttF/fm-106

Use as an FM-1 platform/performance reference and as a lead to firmware-derived control algorithms. GPL-3.0-only code is not copied into FM1JUNO unless project licensing is deliberately made compatible.

## JUNO-60 sources

Use only for shared topology, measurement methods and future DEEP-mode alternatives. Never substitute 60 constants for unresolved 106 constants.
