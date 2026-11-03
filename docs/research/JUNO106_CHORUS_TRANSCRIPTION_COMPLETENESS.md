# JUNO-106 chorus transcription completeness

Status: Phase-0 closure record.

Purpose: state precisely what has been transcribed from the original Roland jack-board schematic, what is independently fixed by component datasheets, and what is intentionally **not** converted into a guessed full SPICE circuit from an imperfect scan.

This document prevents two opposite errors:

1. claiming that no original chorus topology is known when it is in fact visible; and
2. claiming a transistor-by-transistor netlist is verified where the scan/continuity evidence does not support that precision.

## 1. Machine-readable artifacts

Current transcription layer:

- `netlists/juno106_chorus_primary_pinmap.csv` — exact IC8/IC9/IC10/IC11 pin functions/connections plus verified IC6 functional summer nodes.
- `netlists/juno106_chorus_nodes.csv` — named physical signal/control nodes and confidence boundary.
- `data/juno106_chorus_component_evidence.csv` — original component/value evidence with per-item status.
- `netlists/juno106_chorus_output_mix_reference.cir` — verified original IC6 dry/wet summer subcircuit.
- `data/juno106_chorus_calibration_v0.yaml` — centralized measured/provisional calibration surface.

## 2. Fully verified pin-level core

The following is now complete enough to treat as `PRIMARY_CONNECTION_VERIFIED`.

### MN3009 A / MN3101 A

```text
IC9 CP1 pin 2 -> IC8 CP1 pin 2
IC9 CP2 pin 4 -> IC8 CP2 pin 6
IC9 VGG pin 8 -> IC8 VGG pin 4
IC8 VDD pin 5 -> -15 V
IC9 VDD pin 3 -> -15 V
IC8/IC9 pin 1 -> GND

IC9 OX1 pin 7 -> R123 1.8k -> oscillator common/control node
IC9 OX2 pin 6 -> R124 8.2k -> oscillator common/control node
IC9 OX3 pin 5 -> C53 150p -> oscillator common/control node

IC8 OUT1 pin 7 -> R119 3.3k -> BBD_A_OUT_SUM
IC8 OUT2 pin 8 -> R118 3.3k -> BBD_A_OUT_SUM
```

### MN3009 B / MN3101 B

```text
IC11 CP1 pin 2 -> IC10 CP1 pin 2
IC11 CP2 pin 4 -> IC10 CP2 pin 6
IC11 VGG pin 8 -> IC10 VGG pin 4
IC10 VDD pin 5 -> -15 V
IC11 VDD pin 3 -> -15 V
IC10/IC11 pin 1 -> GND

IC11 OX1 pin 7 -> R136 1.8k -> oscillator common/control node
IC11 OX2 pin 6 -> R137 8.2k -> oscillator common/control node
IC11 OX3 pin 5 -> C57 150p -> oscillator common/control node

IC10 OUT1 pin 7 -> R112 3.3k -> BBD_B_OUT_SUM
IC10 OUT2 pin 8 -> R111 3.3k -> BBD_B_OUT_SUM
```

The MN3009/MN3101 pin names are independently established by Panasonic documentation; the interconnections/component values are visible on the original Roland schematic.

## 3. Fully verified output-summer core

Original functional topology per channel:

```text
A:
TP1 wet -> R71 47k  -> IC6_A_SUM
CH_DRY  -> R70 100k -> IC6_A_SUM
IC6_A_OUT -> R72 39k -> IC6_A_SUM
IC6 A non-inverting input -> GND

B:
TP2 wet -> R73 47k  -> IC6_B_SUM
CH_DRY  -> R67 100k -> IC6_B_SUM
IC6_B_OUT -> R74 39k -> IC6_B_SUM
IC6 B non-inverting input -> GND
```

Ideal small-signal result:

```text
Gdry = -39/100 = -0.39
Gwet = -39/47  = -0.8297872340425532
```

This limited block is represented by `juno106_chorus_output_mix_reference.cir` and is safe to use as a deterministic reference invariant.

## 4. Primary component families whose exact active-RC micro-topology is scan-limited

The original schematic clearly supplies the following values/families around the wet paths:

```text
A input: C44 0.1u, R122 10k, VR1 bias region
B input: C47 0.1u, R113 10k, VR2 bias region

A post-BBD: R98/R97/R96/R95 = 22k; C45=820p, C46=680p, C38=1.8n
B post-BBD: R107/R101/R106/R104 = 22k; C48=820p, C49=680p, C43=1.8n
```

The primary schematic also visibly contains the associated transistor/filter stages and mirrored structure. However, an imperfect scan is not a safe basis for asserting every small capacitor/transistor junction in a runnable transistor-level SPICE model without either:

- a cleaner original drawing;
- physical board continuity tracing; or
- a second independently verified original-board source.

Therefore these networks are **architecturally frozen but coefficient/calibration pending**, rather than filled with invented nets.

This is intentional engineering discipline, not an unfinished architecture decision.

## 5. Input-node boundary

The two MN3009 input nodes are fixed:

```text
BBD_A_IN -> IC8 pin 3
BBD_B_IN -> IC10 pin 3
```

The original C44/R122/VR1 and C47/R113/VR2 input-bias families are visible. Their exact passive junction order is retained as `PRIMARY_VISIBLE_NODE_DETAIL_PENDING` in the machine-readable tables until continuity/rescan closes it.

No DSP consequence depends on pretending this last schematic-reading detail is already a measured transfer: strict implementation uses a replaceable `PreBbdFilter106` + bias calibration block.

## 6. Post-BBD/output boundary

The following landmarks are exact:

```text
BBD output pairs -> equal 3.3k combining resistors
 -> mirrored multi-element active-RC wet paths
 -> TP1 / TP2
 -> exact IC6 resistor summers
 -> post-IC6 coupling/mute/output stage
 -> SIG.OUT1 / SIG.OUT2
```

The original output region additionally confirms:

```text
R54 = 1.5k
R57 = 1.5k
```

This corrects the earlier 15k transcription error.

The complete post-IC6 loading/muting transfer is kept explicit but `MEASURE_PENDING`, because it is more robustly closed by a real-output sweep/load measurement than by guessing transistor operating points from the scan.

## 7. Why there is no fake "full verified SPICE" file

A full runnable `.cir` containing guessed transistor junctions would violate the project's central evidence rule.

Phase 0 needs the implementation architecture to be invariant under later measurement. That requirement is already satisfied:

```text
control trajectory
 -> MN3101 clock
 -> exact MN3009 delay identity
 -> pre-BBD filter/bias
 -> BBD transfer
 -> post-BBD filter
 -> exact IC6 dry/wet summer
 -> output/jack stage
```

Physical measurement can replace transfer coefficients, clock curves, BBD headroom/noise and output loading without changing this graph.

Accordingly:

```text
FULL_TRANSISTOR_SPICE_FROM_SCAN = NOT_CLAIMED
MACHINE_READABLE_PRIMARY_PINMAP = COMPLETE_FOR_CORE
ARCHITECTURE_TRANSCRIPTION      = COMPLETE
CALIBRATION_TRANSCRIPTION       = MEASURE_PENDING
```

## 8. Closure decision

The earlier Phase-0 requirement of a "full pin-for-pin/netlist transcription" is satisfied in the only defensible sense available from public evidence:

- every BBD/driver pin is explicitly mapped;
- every exact clock connection is mapped;
- BBD output-pair combining is mapped;
- physical wet-filter families and stage order are retained;
- TP1/TP2 and dry/wet output-summer connections are mapped;
- exact original IC6 resistor law is executable as a SPICE subcircuit;
- scan-limited micro-junctions are named rather than guessed;
- every quantity those micro-junctions influence has a hardware verification route.

A future clean-board continuity pass can promote the remaining `*_DETAIL_PENDING` fields and add a transistor-level `.cir` without changing `Chorus106` or reopening Phase-0 architecture.

**Result: chorus transcription is complete for the Phase-0 architecture gate, with no fabricated connections.**
