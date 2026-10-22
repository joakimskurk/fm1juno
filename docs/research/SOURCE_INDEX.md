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
- factory test/voice-assignment behavior.

Primary PDF mirror used during current research:
`https://www.kiwitechnics.com/downloads/Kiwi-106/Roland%20Juno-106%20Service%20Manual.pdf`

Cross-check mirrors:

- Synfo Roland service-manual archive;
- Synthfool Roland archive;
- BAMA manual archive.

### JUNO-106 Owner's Manual

Use for:

- user-facing parameter semantics;
- authentic front-panel modes;
- HPF position descriptions;
- normal chorus operation;
- controller behavior;
- authoritative normal Assign Mode contract.

Especially important for allocator work:

- Poly 1 = six-voice normal polyphony with natural releases;
- Poly 2 = six-voice mode intended for Portamento, with different release behavior;
- if six keys are simultaneously held, additional keys do **not** sound;
- Poly 1 + Poly 2 = Solo-Unison.

This user-mode statement overrides any temptation to interpret separate service-test rotary/non-rotary tests as normal seventh-note voice stealing.

Roland-hosted PDF:
`https://cdn.roland.com/assets/media/pdf/JUNO-106_OM.pdf`

### Roland current JUNO-106 technical-specification archive

Use as a convenient cross-check for nominal ranges and feature list, not as a replacement for the original service/owner documentation.

## Component references

### Intel 8253 / 8253-5 documentation

Use for interpreting the module ROM's literal `36/76/B6` control words and exact Mode-3 behavior:

- output frequency/count relationship;
- odd/even high/low timing;
- ordinary new-count loading while a counter is already running;
- the fact that a pending new count takes effect at a Mode-3 output transition/end of the current half-cycle rather than replacing that half-cycle immediately;
- distinction between an ordinary count write and reprogramming the counter control word.

Use the original Intel/compatible datasheet family where possible rather than tutorial summaries.

## Hardware / teardown evidence

### Obsolete Technology — 80017A teardown

Use for physical confirmation that the hybrid contains one IR3109-family filter IC, two BA662 VCA devices, passive components and printed resistors.

### Analogue Renaissance — 80017A work / Juno test procedures

Use for teardown/repair-derived implementation clues and practical test-point knowledge. Treat exact DSP constants as secondary until independently verified.

## Circuit-analysis sources

### Sequence 15 — Juno-60 vs Juno-106

Use for hypotheses and circuit interpretation around global HPF/output-loading differences. Verify calculations against schematics before freezing constants.

## Firmware reverse engineering

### ErroneousBosh/j106roms

Public, manually decoded partial disassembly of the JUNO-106 assigner ROM (`ic1.txt`) and module/voice ROM (`ic29.txt`).

Use literal instructions, addresses and data relationships as strong reverse-engineering evidence for:

- 8253 Mode-3 control words `36/76/B6`;
- normal count reload versus timer reset/control-word path;
- 8.8 fixed-point pitch coordinate;
- exact ordering of Tune/LFO/Bender global pitch versus per-voice Portamento;
- note-divisor interpolation;
- DCO-CV note law and fractional interpolation;
- Portamento integer stepping/index architecture;
- LFO integer accumulator, polarity and delay architecture;
- 14-bit envelope arithmetic;
- Attack and Decay/Release table architecture;
- `$083D` envelope fixed-point multiply routine;
- `noteTbl` / `voiceTbl` normal allocator behavior;
- Poly 1 release-order queue and same-note reuse;
- Poly 2 fixed first-free physical allocation;
- `voiceRun`, sustain and module Voice On interaction;
- conditions that set `resetVoiceBits`;
- current-level envelope re-attack.

Caution: repository author explicitly calls the disassembly partial, inaccurate and incomplete. Human labels/comments are hypotheses unless corroborated. Literal instruction flow and independently cross-checked data relationships are treated separately.

Do not copy original ROM machine code or wholesale tables into FM1JUNO. Reimplement observable behavior cleanly and use compact independently derivable equations/calibration data where possible.

Repository:
`https://github.com/ErroneousBosh/j106roms`

### MAME uPD7810/uPD7811 CPU core

Use as an independent ISA reference for interpreting instruction semantics in the public disassembly, especially where skip behavior changes loop cardinality or arithmetic meaning.

Examples already used:

- 8x8 `MUL` register/result semantics for the envelope decay helper;
- `DCR B` decrement/skip-on-borrow semantics, confirming six-pass allocator loops initialized with `B=5` are genuinely six-entry loops rather than apparent off-by-one errors.

This is a CPU-semantics reference, not an audio or JUNO behavior reference.

## Software / implementation references

### MAME `src/mame/roland/juno106.cpp`

Use for CPU/timer address-map cross-checks, ROM identity/checksums and future control-firmware harness work. Current driver is a skeleton and not an audio reference.

### Ultramaster KR-106

Modern desktop emulation project with hardware measurements, circuit simulation and firmware-analysis claims. It was useful for locating `j106roms` provenance and for comparison of candidate filter/chorus/ADSR approaches.

Do not treat its DSP architecture or approximations as primary evidence. GPL-3.0 code is not copied into FM1JUNO unless licensing is deliberately made compatible.

Repository:
`https://github.com/kayrockscreenprinting/ultramaster_kr106`

### PatttF/fm-106

Current FM-1 implementation derived from KR-106. Use as:

- proof that six-voice Juno-family synthesis is practical on FM-1;
- platform/integration/performance reference;
- pointer to existing fixed-point optimizations and host-test workflows.

Do not use its free-running PolyBLEP oscillator, filter, chorus or reconstructed tables as fidelity authority where they differ from primary circuitry/ROM behavior.

GPL-3.0-only code is not copied into FM1JUNO unless project licensing is deliberately made compatible.

Repository:
`https://github.com/PatttF/fm-106`

## JUNO-60 sources

Use only for shared topology, measurement methods and future DEEP-mode alternatives. Never substitute 60 constants for unresolved 106 constants.