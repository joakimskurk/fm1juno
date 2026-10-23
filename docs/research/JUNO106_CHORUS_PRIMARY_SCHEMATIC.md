# JUNO-106 chorus — primary jack-board schematic transcription

Status: **Phase-0 primary-source transcription complete for the chorus architecture gate.** This document records facts read from Roland's original JUNO-106 Service Notes, device-pin semantics from Panasonic datasheets, and clearly labelled secondary cross-checks. It does not invent scan-ambiguous transistor micro-junctions.

Primary source:

- Roland, **JUNO-106 Service Notes**, first edition, July 31 1984, jack-board schematic p. 15 / jack-board PCB p. 14.
- Readable scan: https://www.vintagesynthparts.com/wp-content/uploads/2017/03/JUNO-106_SERVICE_NOTES.pdf
- Independent original-schematic crop: https://www.florian-anwander.de/roland_string_choruses/JUNO-106_schem.jpg

Device references:

- Panasonic MN3009 datasheet — pin functions/stage count/electrical limits.
- Panasonic MN3101 datasheet — pin functions, CP1/CP2 relationship and oscillator-network semantics.

Secondary/reconstruction sources are used only where explicitly marked.

## 1. Phase-0 state

Earlier research correctly refused to invent a 106 chorus from clone BOMs or a generic delay+LPF recipe.

The current evidence state is now:

```text
original Roland global topology                 = mapped
MN3009/MN3101 device pin semantics              = mapped
original BBD/driver pin-to-pin connections      = mapped
original timing-network values                  = mapped
original BBD output-pair combining              = mapped
original wet-path component families            = mapped
TP1/TP2 calibration boundaries                  = mapped
original IC6 dry/wet summing topology            = mapped + executable
post-IC6/output-stage physical boundary          = mapped
scan-ambiguous active-RC micro-junctions         = named, not guessed
physical clock/filter/headroom/noise calibration = MEASURE_PENDING
```

Machine-readable details live in:

- `netlists/juno106_chorus_primary_pinmap.csv`
- `netlists/juno106_chorus_nodes.csv`
- `data/juno106_chorus_component_evidence.csv`
- `JUNO106_CHORUS_TRANSCRIPTION_COMPLETENESS.md`

## 2. Primary IC / calibration map

Directly visible on Roland's jack-board schematic:

```text
IC8   MN3009   BBD channel A
IC9   MN3101   BBD driver/clock channel A
IC10  MN3009   BBD channel B
IC11  MN3101   BBD driver/clock channel B
IC6   TA75558S dual op-amp in the stereo output section
VR1   BBD bias channel A
VR2   BBD bias channel B
TP1   channel-A chorus wet/calibration point
TP2   channel-B chorus wet/calibration point
TP3   chorus-control/LFO test point
TP4   complementary chorus-control/LFO test point
```

Roland's adjustment procedure independently confirms TP1/TP2 + VR1/VR2 as the two BBD bias-calibration paths.

Strict consequence: these are two physical analogue wet paths with independent bias and separate clock state. A single variable delay duplicated or panned to stereo is not topology-faithful.

## 3. Exact BBD/driver pin-level core

Panasonic device pinouts plus Roland's schematic establish:

### Channel A

```text
IC9 MN3101 pin 2 CP1 -> IC8 MN3009 pin 2 CP1
IC9 MN3101 pin 4 CP2 -> IC8 MN3009 pin 6 CP2
IC9 MN3101 pin 8 VGG -> IC8 MN3009 pin 4 VGG
IC8 pin 5 / IC9 pin 3 -> -15 V
IC8 pin 1 / IC9 pin 1 -> GND

IC9 pin 7 OX1 -> R123 1.8k -> oscillator/control common node
IC9 pin 6 OX2 -> R124 8.2k -> oscillator/control common node
IC9 pin 5 OX3 -> C53 150p -> oscillator/control common node

IC8 pin 7 OUT1 -> R119 3.3k -> BBD_A_OUT_SUM
IC8 pin 8 OUT2 -> R118 3.3k -> BBD_A_OUT_SUM
```

### Channel B

```text
IC11 MN3101 pin 2 CP1 -> IC10 MN3009 pin 2 CP1
IC11 MN3101 pin 4 CP2 -> IC10 MN3009 pin 6 CP2
IC11 MN3101 pin 8 VGG -> IC10 MN3009 pin 4 VGG
IC10 pin 5 / IC11 pin 3 -> -15 V
IC10 pin 1 / IC11 pin 1 -> GND

IC11 pin 7 OX1 -> R136 1.8k -> oscillator/control common node
IC11 pin 6 OX2 -> R137 8.2k -> oscillator/control common node
IC11 pin 5 OX3 -> C57 150p -> oscillator/control common node

IC10 pin 7 OUT1 -> R112 3.3k -> BBD_B_OUT_SUM
IC10 pin 8 OUT2 -> R111 3.3k -> BBD_B_OUT_SUM
```

This core is `PRIMARY_CONNECTION_VERIFIED` in the pinmap.

## 4. Original timing-network evidence

Original mirrored families:

```text
A: R123 = 1.8 kOhm, R124 = 8.2 kOhm, C53 = 150 pF
B: R136 = 1.8 kOhm, R137 = 8.2 kOhm, C57 = 150 pF
```

R124/R137 are independently cross-checked by historical MN3009->MN3007 conversion practice: increasing BBD stages by 4x required roughly 4x clock frequency, implemented by reducing these frequency-setting resistors from 8.2 kOhm to about one quarter.

Original-board repair evidence additionally supports the tight matching role of C53/C57.

The clock document owns the exact `f_osc`, `f_cp` and `t_delay=128/f_cp` semantics.

## 5. Primary wet-path component families

Original readable values include:

```text
input/coupling:
 A C44 = 0.1 uF, R122 = 10 kOhm, VR1 bias region
 B C47 = 0.1 uF, R113 = 10 kOhm, VR2 bias region

BBD output combine:
 A R118/R119 = 3.3 kOhm / 3.3 kOhm
 B R111/R112 = 3.3 kOhm / 3.3 kOhm

post-BBD resistor family:
 A R98/R97/R96/R95 = 22 kOhm
 B R107/R101/R106/R104 = 22 kOhm

primary-readable post-BBD capacitors:
 A C45 = 820 pF, C46 = 680 pF, C38 = 1.8 nF
 B C48 = 820 pF, C49 = 680 pF, C43 = 1.8 nF
```

The original drawing also visibly contains the associated mirrored transistor/RC filter stages. A 106-focused reconstruction independently cross-checks the broader characteristic capacitor family, including a 270-pF element.

Where the public original scan does not support an unambiguous small transistor/capacitor junction, the machine-readable transcription explicitly says `*_DETAIL_PENDING` rather than creating a fake net.

This is enough to freeze the **multi-element filter architecture**, but not enough to claim final digital IIR coefficients without measurement/continuity work.

## 6. Exact original IC6 dry/wet summer

This part of the schematic is unambiguous and numerically closed.

### Channel A

```text
TP1 wet -> R71 = 47 kOhm  -> IC6 A inverting sum
CH_DRY  -> R70 = 100 kOhm -> IC6 A inverting sum
feedback R72 = 39 kOhm
non-inverting input -> ground
```

### Channel B

```text
TP2 wet -> R73 = 47 kOhm  -> IC6 B inverting sum
CH_DRY  -> R67 = 100 kOhm -> IC6 B inverting sum
feedback R74 = 39 kOhm
non-inverting input -> ground
```

Ideal small-signal law per channel:

```text
y = -(39/100)*dry -(39/47)*wet
```

Therefore:

```text
Gdry = -0.39
Gwet = -0.8297872340425532
```

This block is represented directly in:

```text
netlists/juno106_chorus_output_mix_reference.cir
```

The often-quoted approximately 55/45 completed dry/wet behavior is therefore not the resistor ratio. Wet-path insertion loss occurs before TP1/TP2 and must be calibrated separately.

## 7. Corrected output-region values

A second read of the original schematic corrected an earlier research transcription error:

```text
R54 = 1.5 kOhm
R57 = 1.5 kOhm
```

The former `15 kOhm` reading is superseded.

The post-IC6 coupling/mute/output network remains a distinct physical reference block. Its complete loaded transfer/headroom is a measurement/continuity target rather than a guessed transistor model.

## 8. What is intentionally not claimed

We do **not** claim that the public scan alone justifies a fully verified transistor-by-transistor SPICE model of every wet-filter and mute/output micro-junction.

Still requiring a cleaner primary drawing, physical continuity, and/or real-unit transfer measurement:

- exact passive junction ordering at the BBD input bias networks beyond the fixed IC input nodes;
- every small transistor/RC junction in the wet reconstruction filters;
- exact static/loaded transfer of the post-IC6 mute/output network;
- clock-dependent MN3009 insertion loss/bandwidth;
- BBD nonlinear/headroom curve at the factory bias point;
- BBD/device/clock noise spectrum and correlation;
- mono-normal jack loading/gain.

These are **calibration/transcription-detail backlog**, not unresolved architecture.

## 9. Netlist policy

The repository now deliberately contains two layers rather than one misleading monolithic circuit:

```text
machine-readable exact pin/node/component transcription
+ verified subcircuits where topology is unambiguous
+ named DETAIL_PENDING boundaries where the scan cannot prove more
```

This is preferable to a runnable full `.cir` with invented connections.

Promotion to a future `VERIFIED_TRANSISTOR_TOPOLOGY` file requires:

1. clean original or board-continuity trace of every small wet-path junction;
2. complete output/mute loading trace;
3. DC operating-point sanity;
4. comparison to real-unit transfer/headroom data.

See `JUNO106_CHORUS_TRANSCRIPTION_COMPLETENESS.md` for the explicit boundary.

## 10. Relationship to One-O-Six / other reconstruction evidence

A 106-focused reconstruction remains useful because it:

- provides a readable mirror of the physical design concept;
- cross-checks the multi-element filter families;
- documents an approximately 55% dry / 45% wet completed-output target;
- offers a practical adjustable clock/LFO implementation and calibration method;
- reports a possible one-channel control-path capacitor asymmetry.

But Roland's original schematic remains authority for original designators/topology. Clone additions, anti-thump modifications, MN3007 conversion parts and modern conveniences must not leak into strict `MODEL_106`.

## 11. Phase-0 gate result

```text
[x] original 106 jack-board schematic located/readable
[x] original dual-BBD / dual-driver / bias topology identified
[x] all MN3009/MN3101 pins explicitly represented in machine-readable pinmap
[x] original timing networks mapped
[x] BBD output-pair combine mapped
[x] original wet-path component families mapped without clone renaming
[x] TP1/TP2 calibration/output boundary mapped
[x] original IC6 dry/wet summing topology and gains derived
[x] verified IC6 output-summer SPICE subcircuit created
[x] output-region R54/R57 correction applied
[x] scan-limited micro-junctions explicitly named rather than guessed
[x] all remaining physical transfer constants assigned measurement tests
```

**Primary schematic transcription is complete for `CHORUS_PHASE0_FROZEN`.**

Real-unit captures later replace `MEASURE_PENDING` calibration data; they do not require changing the frozen chorus signal graph unless they contradict primary evidence.
