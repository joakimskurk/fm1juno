# JUNO-106 chorus netlist workspace

Status: Phase 0 transcription workspace. **Do not treat anything in this directory as a runnable/frozen circuit model until it is explicitly marked VERIFIED.**

The source of truth is the original Roland JUNO-106 Service Notes jack-board schematic. Reconstruction/clone schematics may be used to resolve legibility only when the Roland image is ambiguous, and any such use must remain labelled as a cross-check.

## Why there is no guessed `.cir` yet

A plausible SPICE file is more dangerous than an incomplete evidence map: once it runs, guessed nodes can easily become de-facto constants. The current sequence is therefore:

```text
Roland schematic
 -> node/component evidence table
 -> mirror-pair cross-check
 -> second-pass visual verification
 -> literal netlist transcription
 -> DC/AC sanity checks
 -> physical-unit measurement comparison
 -> fitted DSP
```

The first machine-readable topology artifact is:

- `juno106_chorus_nodes.csv`

Component/value evidence shared with the wider chorus research lives in:

- `../data/juno106_chorus_component_evidence.csv`

## Naming contract

Physical names are preferred over DSP names.

Examples:

```text
CH_IN                 post-global-VCA signal entering chorus section
BBD_A_IN              IC8 MN3009 audio input-side node
BBD_A_OUT             IC8 output-combine node
TP1_WET_A             Roland TP1 wet/calibration node
BBD_B_IN              IC10 MN3009 audio input-side node
BBD_B_OUT             IC10 output-combine node
TP2_WET_B             Roland TP2 wet/calibration node
CTRL_A_TP3            Roland TP3 chorus-control probe
CTRL_B_TP4            Roland TP4 complementary chorus-control probe
OUT_MIX_A              passive/active mix network feeding output A
OUT_MIX_B              passive/active mix network feeding output B
SIG_OUT_1              Roland jack-board SIG.OUT 1
SIG_OUT_2              Roland jack-board SIG.OUT 2
```

Do not name an analogue node `dry55`, `wet45`, `lowpass14k` or similar interpretation-derived labels.

## Evidence states

```text
PRIMARY_VISIBLE
PRIMARY_VALUE_VISIBLE
PRIMARY_CONNECTION_VERIFIED
PRIMARY_MIRROR_VERIFIED
CROSS_CHECK_ONLY
RESCAN_REQUIRED
MEASURE_REQUIRED
```

A value may be visible while its exact node attachment is not yet verified. Those are separate facts.

## Mirror-pair check

The original chorus is intentionally close to bilateral/mirrored. During transcription, channel-A and channel-B elements must be checked as pairs rather than independently guessed.

High-level pair anchors:

```text
IC8  <-> IC10   MN3009
IC9  <-> IC11   MN3101
VR1  <-> VR2    BBD bias
TP1  <-> TP2    wet/calibration output
R124 <-> R137   clock timing resistor, 8.2 kOhm
C53  <-> C57    clock timing capacitor, 150 pF
R71  <-> R73    output-region checkpoint, 47 kOhm
R70  <-> R67    output-region checkpoint, 100 kOhm
R72  <-> R74    output-region checkpoint, 39 kOhm
R54  <-> R57    output-region checkpoint, 15 kOhm
```

The last four resistor pairs are transcription checkpoints and remain `SCHEMATIC_READ_CHECK_REQUIRED` until their node connections are traced end-to-end.

## Verification gate for the first SPICE file

A `.cir` file may be promoted from scaffold to `VERIFIED_TOPOLOGY_V0` only when:

1. every component around IC8/IC10 and IC9/IC11 has a Roland designator;
2. all visible values have been read twice from the primary scan;
3. mirrored A/B branches have been compared for intentional vs accidental differences;
4. TP1..TP4 and all BBD/driver pins are present;
5. dry path into both output summers is explicitly traced;
6. IC6 feedback/input networks are explicitly traced;
7. mute/output transistors and output loading are retained or explicitly justified away;
8. power/bias rails needed for DC/headroom analysis are retained;
9. every unreadable item is a named placeholder, never a guessed value.

Only after that should AC transfer functions be calculated.