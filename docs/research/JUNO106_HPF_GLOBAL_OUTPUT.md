# JUNO-106 HPF and global output/VCA — authoritative index

Status: **`GLOBAL_OUTPUT_PHASE0_ARCHITECTURE_FROZEN`**. This file is the compact authority/index; detailed implementation contract lives in `JUNO106_GLOBAL_OUTPUT_REFERENCE_MODEL.md`.

The block begins after the six per-voice A1QH80017A VCA outputs are summed and ends at the already-frozen `Chorus106` input.

## 1. Authority map

Use together:

1. `JUNO106_GLOBAL_OUTPUT_REFERENCE_MODEL.md` — current implementation/closure contract.
2. `data/juno106_global_output_calibration_v0.yaml` — only home for provisional/measured strict calibration values.
3. `data/juno106_global_output_evidence.csv` — evidence state per fact/value.
4. `netlists/juno106_hpf23_component_reference.cir` — verified component-ideal HPF2/3 reference.
5. `PHASE0_MEASUREMENT_PLAN.md` — physical acquisition plan for remaining calibration.

Older secondary interpretations do not override the original Roland schematic.

## 2. Frozen global order

```text
six voice VCA outputs
 -> mono sum
 -> TC4052 four-route HPF selector
 -> IC4A buffer / HPF0 summer
 -> uPC1252H2 global VCA
 -> post-VCA current-to-voltage/output stage
 -> Chorus106
```

Rules:

- HPF is global, not per voice;
- VCA LEVEL is global and distinct from six voice BA662 VCAs;
- no active-voice normalization;
- 106 IC4A isolation must not be replaced by JUNO-60 loading behavior;
- high VCA LEVEL may legitimately enter global-path nonlinearity.

## 3. HPF semantic states

```text
0 = active bass emphasis
1 = straight / flat
2 = first-order HPF
3 = higher first-order HPF
```

The physical TC4052 bit mapping is hidden inside the hardware/control layer. Strict DSP uses semantic modes.

## 4. HPF 1

Nominal straight branch:

```text
H1(s) ~= 1
```

This means no intentional HPF shaping. It does **not** bypass IC4A, global VCA or the downstream output/chorus path.

## 5. HPF 2

Original component pair:

```text
R = 47 kOhm
C = 15 nF
```

Ideal first-order transfer:

```text
H2(s) = sRC / (1+sRC)
fc = 225.7516923289 Hz
```

Freeze:

```text
HPF2_R_OHM = 47000
HPF2_C_F   = 15e-9
HPF2_FC_IDEAL_HZ = 225.7516923289
```

## 6. HPF 3

Original component pair:

```text
R = 47 kOhm
C = 4.7 nF
```

Ideal first-order transfer:

```text
H3(s) = sRC / (1+sRC)
fc = 720.4841244540 Hz
```

Freeze:

```text
HPF3_R_OHM = 47000
HPF3_C_F   = 4.7e-9
HPF3_FC_IDEAL_HZ = 720.4841244540
```

Both HPF2 and HPF3 are first-order. A `Q` value is **not applicable**; earlier checklist wording asking for cutoff/Q is superseded.

## 7. TC4052 on-resistance

The selector has finite Ron. Datasheet-class 15-V anchors are approximately:

```text
Ron typ ~= 80 ohm
Ron max ~= 160 ohm
```

These are component-reference values, not exact installed-unit constants.

Compared with 47 kOhm, their scale is small. If simply added to branch R, the ideal corner sanity values become:

```text
HPF2: 225.368 Hz typ / 224.986 Hz max
HPF3: 719.260 Hz typ / 718.040 Hz max
```

Strict reference retains `selectorRonOhm` as calibration data instead of deleting it or hard-coding a datasheet typical.

## 8. HPF 0 — architecture frozen

HPF 0 is not `fc=0`, bypass or a generic low-shelf.

The original 106 topology plus independent circuit cross-check establish:

```text
selected input
 |\
 | +-> direct contribution -----------------------+
 |                                                 |
 +-> shaped contribution                           |
      -> Stage 1 passive pole/zero: C8/C9/R22      |
      -> Stage 2 active pole/zero:                  |
         R18/R19/R20/C6/IC4B                        |
                                                   v
                                              R24/R25 / IC4A
```

Primary-readable anchor:

```text
C8 = 0.01 uF
```

Therefore strict architecture freezes HPF0 as:

```text
DIRECT + TWO_STAGE_TWO_POLE_TWO_ZERO_BASS_EMPHASIS
```

Exact original transfer coefficients/shelf gain remain `MEASURE_OR_PRIMARY_NETLIST_PENDING`. They can be replaced later without changing the graph.

## 9. IC4A and 106-vs-60 distinction

The 106 buffers/sums the selected HPF network through IC4A before the uPC1252 stage. This isolation is a real architecture difference from the 60 output path.

Strict consequence:

```text
MODEL_106: HpfSelector106 -> IC4A -> GlobalVca1252
```

Do not add JUNO-60 load-dependent dulling to `MODEL_106`.

## 10. VCA LEVEL digital boundary

Recovered control representation:

```text
vca_level_code = raw7 << 7
range = 0x0000 .. 0x3f80
```

Roland's DAC/control documentation places this signal in the nominal conditioned `0..+5 V` family before the jack-board network.

Physical chain:

```text
raw7
 -> code14
 -> DAC/S&H
 -> conditioned held voltage
 -> Roland external scale/offset/current network
 -> uPC1252 control input
 -> gain cell
```

Never implement strict mode as `gain=raw7/127`.

## 11. uPC1252 core

uPC1252H2 is a current-mode dbx-family VCA. Device documentation provides an approximately dB-linear control reference around its control input, with the useful component-level anchors approximately:

```text
-0.300 V -> +50 dB
 0.000 V ->   0 dB
+0.300 V -> -50 dB
```

This describes the **IC**, not the JUNO panel law.

The 106 has an external control network between held VCA LEVEL voltage and the chip. Original-unit technician measurements show a shifted/narrow chip-control region and enough variation that the correct strict split is:

```text
JunoVcaControlNetwork106 -> Upc1252Core
```

The panel/code-to-chip-control and complete code-to-gain curves remain centralized `MEASURE_PENDING` data.

## 12. Post-VCA stage

Because uPC1252 is current-mode, the following active network remains an explicit current-to-voltage/output block before chorus.

Do not silently collapse:

```text
uPC1252 + post-I/V + chorus loading
```

into one normalized multiplier.

## 13. Headroom/nonlinearity

Roland's owner's documentation explicitly notes that excessive VCA LEVEL may cause distortion and that this is normal.

Strict model therefore preserves:

- summed six-voice drive;
- Solo-Unison common-path drive;
- VCA LEVEL-dependent headroom;
- downstream chorus loading;
- a replaceable calibrated nonlinear transfer.

No hidden pre-VCA limiter or automatic unity compensation.

## 14. Central calibration

All remaining values live in:

`data/juno106_global_output_calibration_v0.yaml`

Still `MEASURE_PENDING`:

- exact HPF0 complex transfer;
- actual selector Ron at the operating point;
- IC4A gain/headroom/slew;
- code14 -> held control voltage;
- held control -> uPC1252 control-pin voltage;
- complete VCA LEVEL -> gain/dB law;
- post-VCA I/V gain;
- clipping/headroom/THD surface;
- loading into chorus.

## 15. Phase-0 state

Architecture gate closed:

```text
[x] global four-route selector
[x] HPF0/1/2/3 semantics
[x] HPF2 exact component-ideal first-order law
[x] HPF3 exact component-ideal first-order law
[x] HPF0 direct + two-stage/two-pole-two-zero topology
[x] IC4A 106-specific buffer/summer boundary
[x] exact raw7<<7 VCA LEVEL representation
[x] Roland external VCA-control network separated from uPC1252 core
[x] current-mode VCA + post-I/V architecture retained
[x] high-level distortion/headroom requirement retained
[x] unresolved numerical calibration centralized with tests
```

**Phase-0 global-output state: `GLOBAL_OUTPUT_PHASE0_ARCHITECTURE_FROZEN`.**
