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
- A1QH80017A VCF/VCA block structure;
- VCF-CV constituent parameters;
- factory VCF/VCA calibration procedures and warmed-up test targets;
- jack-board HPF/output/chorus schematic;
- factory test/voice-assignment behavior.

Especially important 80017A calibration anchors:

- VCF resonance: Bank 3, C4, 4.8 Vp-p sine at the per-voice VCF test point;
- VCA gain: Bank 3, C4, 6 Vp-p at the post-VCA voice output;
- VCF Frequency: C4 -> 248 Hz / B3 pitch;
- VCF Width: C6 -> 992 Hz / B5 pitch;
- Frequency and Width interact and are repeated until within +/-10 cents;
- warm-up of at least ten minutes before VCF frequency/width calibration.

Primary PDF mirror:
`https://www.kiwitechnics.com/downloads/Kiwi-106/Roland%20Juno-106%20Service%20Manual.pdf`

Cross-check mirrors/transcriptions:

- Synfo Roland service-manual archive;
- ManualsLib transcription/index;
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

Use as a convenient cross-check for nominal ranges and feature list, not as a replacement for original service/owner documentation.

## Original shared circuit visibility

### JUNO-6 / JUNO-60 Service Notes

Use only where physical teardown proves the same IC family/topology exists inside the 80017A.

The exposed JUNO-6/60 VCF/VCA circuit is valuable because it shows externally what the 106 hybrid hides:

- IR3109 four-stage low-pass core;
- BA662 resonance/Q-compensation path;
- BA662 final voice VCA;
- 68 kOhm / 240 pF filter-stage family in the exposed circuit.

This source does **not** by itself prove every SMD passive inside an 80017A has the same numeric value. In strict 106 work, shared topology is high confidence; exact hybrid passive values remain separately validated.

Example service-note mirror:
`https://manuals.fdiskc.com/flat/Roland%20Juno-60%20Service%20Notes%20%28%20HI-RES%20%29.pdf`

## Component references

### Intel 8253 / 8253-5 documentation

Use for interpreting the module ROM's literal `36/76/B6` control words and exact Mode-3 behavior:

- output frequency/count relationship;
- odd/even high/low timing;
- ordinary new-count loading while a counter is already running;
- a pending new count taking effect at a Mode-3 output transition/end of the current half-cycle;
- distinction between an ordinary count write and reprogramming the counter control word.

Use original Intel/compatible datasheet family where possible rather than tutorial summaries.

### Alfa RPAR AS3109 datasheet

Modern IR3109-compatible device used as an architectural/electrical reference, **not** proof of exact vintage IR3109 silicon constants.

Useful published constraints:

- four variable-transconductance stages under one exponential converter;
- frequency-control range around 15 octaves;
- typical control sensitivity around -19 mV/octave;
- typical single-stage 250 Hz cutoff at VC=0 with C=240 pF and R=68 kOhm;
- approximately 560-ohm stage input resistance;
- high-impedance MOS-type output buffers;
- stages 1/3 and 2/4 use different buffer-load structures and have different current capability;
- typical reference low-pass wiring uses 68 kOhm / 560 ohm / 240 pF networks.

Datasheet mirror used:
`https://www.ericasynths.lv/media/AS3109_uGjL63a.pdf`

## Hardware / teardown evidence

### Obsolete Technology — 80017A teardown

Physical original-hybrid teardown.

Use for strong confirmation that the A1QH80017A contains:

- one IR3109-family filter IC;
- two BA662 VCA/OTA devices;
- SMD capacitors;
- printed resistors/passives.

The teardown identifies one BA662 as resonance-control element and the second as the final voice VCA, and explicitly compares the architecture to the exposed JUNO-60 circuit.

Source:
`https://obsoletetechnology.wordpress.com/projects/80017a-vcfvca-teardown/`

### Sound Doctor opened-80017A measurements

Repair-derived direct measurements on original hybrids. Treat as secondary measurement evidence pending independent cross-check.

Reported values include:

- inter-stage/input resistors around 68 kOhm;
- shunts around 550–560 ohm;
- filter capacitors around 250 pF in measured hybrids;
- resonance-network resistors including input/feed-forward and output-feedback paths;
- final BA662 VCA input/shunt network values.

Important use: preserve the `240 pF exposed JUNO-60` vs `~250 pF measured 80017A` question as an explicit 106 validation task instead of silently collapsing the two.

Source:
`https://www.sounddoctorin.com/synthtec/roland/juno106.htm`

### Analogue Renaissance — D80017 / AR80017A engineering

Use for repair/clone-derived implementation clues, calibration practice and behavioral edge cases. Treat exact internal constants as secondary until independently verified.

Particularly useful observations:

- a correct replacement must undergo the Roland adjustment procedure;
- original-like nonlinear and resonance behavior is measurable, not merely cosmetic;
- extreme resonance/cutoff transient behavior can differ due to thermal distribution;
- supply interaction and filter/VCA noise tolerance can matter in edge cases.

Sources:

- `https://www.analoguerenaissance.com/D80017A/`
- `https://www.analoguerenaissance.com/AR80017A/`
- `https://www.analoguerenaissance.com/JUNOTEST/`

## Circuit-analysis sources

### Electric Druid — Roland filter designs with IR3109 / AS3109

Secondary but technically transparent circuit analysis based on original schematics and the modern AS3109 datasheet.

Especially useful for the exposed JUNO-6 topology:

- standard four-stage 68 kOhm / 240 pF IR3109 low-pass;
- BA662 resonance control;
- Roland Q/passband compensation: the resonance BA662 receives both stage-4 feedback and an input/feed-forward signal on opposite differential inputs;
- BA662 soft limiting prevents unbounded/hard-clipped self-oscillation;
- stage-buffer differences in the 3109 family matter when tapping/loading stages.

Source:
`https://electricdruid.net/roland-filter-designs-with-the-ir3109-or-as3109/`

### AMSynths — IR3109 / 80017A analysis

Use as secondary component/history/circuit interpretation. Useful for cross-checking that the 80017A packages SMD IR3109 + two SMD BA662F devices and retains the JUNO-family Q-compensated self-oscillating 24 dB low-pass topology.

Do not elevate modern clone component choices to primary 106 constants.

Source:
`https://amsynths.co.uk/2022/04/06/all-about-the-ir3109-chip/`

### Sequence 15 — JUNO-60 vs JUNO-106

Use for hypotheses and circuit interpretation around global HPF/output-loading differences. Verify calculations against schematics before freezing constants.

## Firmware reverse engineering

### ErroneousBosh/j106roms

Public, manually decoded partial disassembly of the JUNO-106 assigner ROM (`ic1.txt`) and module/voice ROM (`ic29.txt`).

Use literal instructions, addresses and data relationships as strong reverse-engineering evidence for:

- 8253 Mode-3 control words `36/76/B6`;
- normal count reload versus timer reset/control-word path;
- 8.8 fixed-point pitch coordinate;
- exact Tune/LFO/Bender global pitch ordering versus per-voice Portamento;
- exact DCO and VCF Bender normalization;
- note-divisor interpolation;
- DCO-CV note law and fractional interpolation;
- Portamento integer stepping/index architecture;
- LFO integer accumulator, polarity, rate-coefficient architecture and Delay behavior;
- automatic LFO-Delay trigger semantics from the `voiceRun` empty/non-empty transition;
- performance LFO-trigger/MIDI-mod contribution bypassing automatic DCO LFO Delay;
- 14-bit envelope arithmetic;
- Attack and Decay/Release table architecture;
- `$083D` envelope fixed-point multiply routine;
- `noteTbl` / `voiceTbl` normal allocator behavior;
- Poly 1 release-order queue and same-note reuse;
- Poly 2 fixed first-free physical allocation;
- normal no-seventh-note behavior;
- Solo-Unison sending Voice On/Off to all six physical voices;
- `voiceRun`, sustain and module Voice On interaction;
- conditions that set `resetVoiceBits`;
- current-level envelope re-attack;
- exact Cutoff `7-bit << 7` storage;
- VCF ENV/Key Follow/LFO/Bender integer arithmetic;
- Key Follow's `3/8 * pitch` coordinate and Middle-C (`0x1680`) pivot;
- shared `floor(value * 8-bit_multiplier / 256)` control primitive.

Caution: repository author explicitly calls the disassembly partial, inaccurate and incomplete. Human labels/comments are hypotheses unless corroborated. Literal instruction flow and independently cross-checked data relationships are treated separately.

Do not copy original ROM machine code or wholesale tables into FM1JUNO. Reimplement observable behavior cleanly and use compact independently derivable equations/calibration data where possible.

Repository:
`https://github.com/ErroneousBosh/j106roms`

### MAME uPD7810/uPD7811 CPU core

Use as an independent ISA reference for interpreting instruction semantics in the public disassembly, especially where skip behavior changes loop cardinality or arithmetic meaning.

Examples already used:

- 8x8 `MUL` register/result semantics for envelope/control multiply helpers;
- `DCR B` decrement/skip-on-borrow semantics, confirming six-pass allocator loops initialized with `B=5` are genuinely six-entry loops;
- compare/skip semantics in performance-modulation arbitration.

This is a CPU-semantics reference, not an audio or JUNO behavior reference.

## Software / implementation references

### MAME `src/mame/roland/juno106.cpp`

Use for CPU/timer address-map cross-checks, ROM identity/checksums and future control-firmware harness work. Current driver is a skeleton and not an audio reference.

### Ultramaster KR-106

Modern desktop emulation project with hardware measurements, circuit simulation and firmware-analysis claims. It was useful for locating `j106roms` provenance and comparing candidate filter/chorus/ADSR approaches.

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

Use only for shared topology, measurement methods and future explicit strict-60 / DEEP alternatives. Never substitute 60-specific timing, clock, envelope, HPF or chorus constants for unresolved 106 constants.