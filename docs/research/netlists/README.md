# JUNO-106 chorus netlist workspace

Status: **Phase-0 machine-readable chorus transcription complete for the architecture gate.** A full transistor-level SPICE model is deliberately **not** claimed where the public scan cannot support every micro-junction without guessing.

The source of truth is the original Roland JUNO-106 Service Notes jack-board schematic. Panasonic MN3009/MN3101 documentation supplies device pin semantics. Reconstruction/clone schematics may be used only as labelled cross-checks.

## Current artifacts

- `juno106_chorus_primary_pinmap.csv` — pin-level map for IC8/IC9/IC10/IC11 plus verified IC6 functional summing nodes.
- `juno106_chorus_nodes.csv` — named physical signal/control nodes and confidence state.
- `juno106_chorus_output_mix_reference.cir` — **verified limited SPICE subcircuit** for the original IC6 dry/wet summers.
- `../data/juno106_chorus_component_evidence.csv` — per-component value/evidence map.
- `../data/juno106_chorus_calibration_v0.yaml` — centralized strict calibration and `MEASURE_PENDING` quantities.
- `../JUNO106_CHORUS_TRANSCRIPTION_COMPLETENESS.md` — exact boundary between verified transcription and scan/measurement-limited detail.

## Why there is no guessed full `.cir`

A plausible runnable circuit is more dangerous than an explicit incomplete micro-net: guessed nodes quickly become de-facto constants.

The evidence-preserving sequence is:

```text
Roland schematic
 -> exact device pin map
 -> original component/value evidence
 -> verified physical node graph
 -> verified subcircuits where connections are unambiguous
 -> named scan-limited detail where they are not
 -> physical measurement / continuity closure
 -> fitted reference DSP
```

A future full transistor-level `.cir` may be added after continuity/clean-scan verification. It is **not required to redesign Chorus106** because the block boundaries are already frozen.

## Naming contract

Physical names are preferred over DSP interpretations.

```text
CH_IN
CH_DRY
BBD_A_IN
BBD_A_OUT1
BBD_A_OUT2
BBD_A_OUT_SUM
TP1_WET_A
CLOCK_A_CP1
CLOCK_A_CP2
VGG_A
OSC_A_COMMON
CTRL_A_TP3

BBD_B_IN
BBD_B_OUT1
BBD_B_OUT2
BBD_B_OUT_SUM
TP2_WET_B
CLOCK_B_CP1
CLOCK_B_CP2
VGG_B
OSC_B_COMMON
CTRL_B_TP4

IC6_A_SUM
IC6_A_OUT
IC6_B_SUM
IC6_B_OUT
SIG_OUT_1
SIG_OUT_2
```

Do not name an analogue node `dry55`, `wet45`, `lowpass14k` or similar interpretation-derived labels.

## Evidence states

```text
PRIMARY_VISIBLE
PRIMARY_VALUE_VISIBLE
PRIMARY_CONNECTION_VERIFIED
PRIMARY_VISIBLE_NODE_DETAIL_PENDING
PRIMARY_VISIBLE_OUTPUT_DETAIL_PENDING
CROSS_CHECK_ONLY
MEASURE_PENDING
```

A component value being legible and its exact small-signal transfer being known are different facts.

## Verified BBD/clock core

The current pin map explicitly preserves:

```text
MN3101 CP1/CP2 -> MN3009 CP1/CP2
MN3101 VGG_OUT -> MN3009 VGG
MN3101 1.8k / 8.2k / 150p oscillator network
MN3009 OUT1/OUT2 -> equal 3.3k output combining resistors
```

for both channels.

This is enough to freeze the clock/delay architecture and BBD output-pair boundary without inventing filter coefficients.

## Verified IC6 summer

Original mirrored values:

```text
A: wet R71=47k, dry R70=100k, feedback R72=39k
B: wet R73=47k, dry R67=100k, feedback R74=39k
```

Ideal law:

```text
Vout = -0.39*Vdry - 0.8297872340425532*Vwet
```

The executable reference is `juno106_chorus_output_mix_reference.cir`.

## Corrected output-region checkpoint

Primary schematic re-read gives:

```text
R54 = 1.5 kOhm
R57 = 1.5 kOhm
```

The older `15 kOhm` transcription is superseded everywhere in current chorus authority documents.

## Scan-limited networks

The original wet path clearly establishes the multi-element analogue architecture and major mirrored component families, but some transistor/RC micro-junctions are not safe to promote into a full SPICE net solely from the public scan.

These remain named boundaries:

```text
C44/R122/VR1 -> BBD_A_IN
C47/R113/VR2 -> BBD_B_IN

BBD_A_OUT_SUM -> mirrored active-RC wet filter -> TP1
BBD_B_OUT_SUM -> mirrored active-RC wet filter -> TP2

IC6_A_OUT -> coupling/mute/output -> SIG_OUT_1
IC6_B_OUT -> coupling/mute/output -> SIG_OUT_2
```

Their exact transfer functions are physical calibration targets, not architectural unknowns.

## Full-transistor SPICE promotion gate

A future complete original-jack-board `.cir` may be labeled `VERIFIED_TRANSISTOR_TOPOLOGY` only when:

1. every small passive/transistor junction around the two wet paths is traced from a clean primary drawing or board continuity;
2. intentional A/B asymmetries are distinguished from scan ambiguity;
3. post-IC6 mute/output loading is traced completely;
4. DC operating points are checked against original rails;
5. the model is compared with real-unit transfer/headroom data.

Until then, no guessed topology is allowed.

## Phase-0 conclusion

For the chorus architecture gate we now have:

```text
[x] machine-readable BBD/driver pin map
[x] named original node graph
[x] original component evidence map
[x] exact clock/delay semantics
[x] exact dry/wet summer subcircuit
[x] explicit scan-limited boundaries
[x] centralized measurement/calibration backlog
```

This satisfies the Phase-0 transcription requirement without creating a false sense of precision.
