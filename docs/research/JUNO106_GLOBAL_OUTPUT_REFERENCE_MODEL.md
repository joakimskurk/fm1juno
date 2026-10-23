# JUNO-106 global HPF / output-VCA — Phase 0 reference model

Status: **`GLOBAL_OUTPUT_PHASE0_ARCHITECTURE_FROZEN`**. Exact original-unit gain/headroom curves remain centralized `MEASURE_PENDING` calibration data.

This block begins after the six A1QH80017A voice-VCA outputs are summed and ends at the input to the already-frozen `Chorus106` block.

## 1. Frozen physical graph

```text
six physical voice-VCA outputs
 -> mono voice sum
 -> TC4052 discrete HPF-route selector
      HPF 0 -> direct path + two-stage bass-emphasis path -> IC4A sum/buffer
      HPF 1 -> straight route                          -> IC4A buffer
      HPF 2 -> 47k / 15n first-order HPF             -> IC4A buffer
      HPF 3 -> 47k / 4.7n first-order HPF            -> IC4A buffer
 -> uPC1252H2 global VCA
 -> current-to-voltage / post-VCA output stage
 -> Chorus106
```

Strict consequences:

- one global HPF, never one per voice;
- one global patch-level VCA after HPF, separate from the six BA662 voice VCAs;
- no active-voice-count normalization;
- HPF 0 is a dedicated active bass-emphasis route, not `fc=0` and not a generic EQ;
- the 106 IC4A buffer isolates the selected HPF network from the following uPC1252 stage;
- global-path overload at high VCA LEVEL is an authentic behavior to preserve, not auto-normalize away.

## 2. HPF 1 / 2 / 3

### HPF 1

Nominal straight branch:

```text
H1(s) ~= 1
```

This bypasses intentional HPF shaping only. It does not bypass the selector, IC4A, global VCA, post-VCA stage or chorus.

### HPF 2

Original branch:

```text
R = 47,000 ohm
C = 15 nF
```

Ideal first-order transfer:

```text
H2(s) = sRC / (1 + sRC)
fc = 1 / (2*pi*R*C)
   = 225.7516923289 Hz
```

### HPF 3

Original branch:

```text
R = 47,000 ohm
C = 4.7 nF
```

Ideal first-order transfer:

```text
H3(s) = sRC / (1 + sRC)
fc = 720.4841244540 Hz
```

These are **first-order filters**, so a second-order `Q` parameter is not applicable. The previous checklist wording `cutoff/Q` is superseded.

## 3. TC4052 selector resistance

The selector is a TC4052-family analogue multiplexer. Device documentation gives an on-resistance on the order of tens of ohms at high supply voltage; a useful datasheet-class 15-V anchor is approximately:

```text
Ron typ ~= 80 ohm
Ron max ~= 160 ohm
```

This is not frozen as the exact installed-unit resistance because it varies with signal level, supply and temperature.

For scale only, adding 80/160 ohm to the 47k branch resistance moves the ideal component corners to approximately:

```text
HPF2: 225.368 / 224.986 Hz
HPF3: 719.260 / 718.040 Hz
```

So the architecture is not sensitive to Ron, while the high-precision reference should still retain `selectorRonOhm` as a calibration term.

## 4. HPF 0 topology is now frozen

The original schematic plus independent circuit analysis support a stable architecture:

```text
selected input
 |\
 | +-> direct contribution --------------------+
 |                                              |
 +-> frequency-shaped branch                    |
       -> stage 1: passive pole/zero network     |
       -> stage 2: IC4B active pole/zero network |
                                                v
                                           IC4A sum/buffer
```

The shaped branch uses the original component families around:

```text
Stage 1: C8, C9, R22
Stage 2: R18, R19, R20, C6, IC4B
final/direct summing region: R24 / R25 -> IC4A
```

`C8 = 0.01 uF` is primary-readable from the original 106 schematic.

The important freeze is structural:

```text
HPF0 = direct contribution + two-pole/two-zero bass-emphasis contribution
```

It is **not** a Sallen-Key HPF, not a one-pole shelf chosen by ear, and not an arbitrary low-frequency gain knob.

The exact numerical transfer remains `MEASURE_OR_PRIMARY_NETLIST_PENDING` until every HPF0 passive value/node is cleanly transcribed or a calibrated original-unit sweep closes the curve. This does not reopen the signal graph.

## 5. Critical 106-vs-60 isolation

The 106 inserts IC4A between the HPF network and global VCA. The 60 does not present the same loading arrangement.

Strict `MODEL_106` therefore keeps:

```text
HpfSelector106 -> Buffer/SummerIC4A106 -> GlobalVca1252
```

Do not import the JUNO-60's load-dependent high-frequency behavior into the 106 merely because both machines have four HPF switch positions.

## 6. VCA LEVEL digital/control boundary

The recovered module-control representation is:

```text
raw7          = 0..127
vcaLevelCode  = raw7 << 7
              = 0x0000..0x3f80
```

Roland's DAC/control documentation places this in the nominal conditioned `0..+5 V` family before the jack-board control network.

Physical chain:

```text
raw patch value
 -> exact 14-bit-style code
 -> DAC / sample-and-hold
 -> conditioned control voltage
 -> Roland jack-board resistor/coupling network
 -> uPC1252 gain-control input
 -> VCA gain cell
```

No strict code may replace this with `gain = raw7/127`.

## 7. uPC1252H2 component law versus Roland control law

The uPC1252H2 is a dbx-family current-mode VCA. Component documentation describes an approximately dB-linear gain-control characteristic around its control input, with the useful reference relationship approximately:

```text
control ~= -0.300 V -> about +50 dB
control ~=  0.000 V -> about   0 dB
control ~= +0.300 V -> about -50 dB
```

This is a **component-level reference**, not the JUNO panel law.

The 106 places its own network between the held VCA LEVEL control and the uPC1252 control pin. Original-board service measurements reported by technicians show a shifted/narrow control-voltage region at the chip and also show unit/measurement variation. That is exactly why the strict architecture separates:

```text
VcaLevelControl106
  code14
  heldControlVoltage
  externalNetwork
  chipControlVoltage

Upc1252Core
  chipControlVoltage -> gainDb
```

The external Roland mapping remains `MEASURE_PENDING` / primary-netlist pending. Do not infer it from the bare IC's mV/dB law.

## 8. Post-VCA current-to-voltage stage

The uPC1252 audio path is current-mode. The following active stage must remain explicit as a current-to-voltage/output conversion stage before chorus.

Reference graph:

```text
IC4A voltage output
 -> uPC1252 input-current conversion
 -> uPC1252 gain cell/current output
 -> post-VCA I/V converter
 -> coupling/headroom
 -> Chorus106 input
```

Do not call the following op-amp a generic anti-alias low-pass unless its original network proves that transfer.

## 9. High-level distortion contract

Roland's owner documentation explicitly warns that excessive VCA LEVEL can cause distortion and treats that as normal operation.

Therefore strict rendering must allow:

- one vs six voices to drive the global block differently;
- Solo-Unison to increase common-path drive naturally;
- VCA LEVEL to move the path toward its nonlinear/headroom boundary;
- chorus input loading to remain downstream of that boundary.

Forbidden:

```text
per-voice auto normalization
pre-global-VCA hidden limiter
post-level automatic unity compensation
```

Platform protection, if required, lives after the reference signal path and is separately bypassable in validation renders.

## 10. Reference API

```text
enum class HpfMode106 {
    BassBoost0,
    Flat1,
    Hpf225_2,
    Hpf720_3
};

struct Hpf106 {
    HpfMode106 mode;
    double selectorRonOhm;
    Hpf0BassEmphasis106 bass0;
    FirstOrderHpf106 hpf2;
    FirstOrderHpf106 hpf3;
};

struct GlobalVcaControl106 {
    uint16_t code14;
    double heldVolts;
    double chipControlVolts;
};

struct GlobalVca1252 {
    GlobalVcaControl106 control;
    double process(double x);
};
```

Desktop diagnostics must allow:

```text
HPF_PHYSICAL
HPF_IDEAL_COMPONENT
VCA_UNITY_DIAGNOSTIC
VCA_STATIC_CALIBRATED
VCA_NONLINEAR_CALIBRATED
```

## 11. Centralized calibration boundary

All unresolved numerical data belongs in:

```text
data/juno106_global_output_calibration_v0.yaml
```

Required future original-unit captures:

- exact HPF0 complex transfer relative to HPF1;
- actual selector Ron contribution if significant;
- IC4A gain/headroom/slew;
- code14 -> held jack-board control voltage;
- held voltage -> uPC1252 control-pin voltage;
- control-pin voltage -> total output gain through the installed network;
- post-VCA I/V gain;
- overload/clipping/THD surface versus input and VCA LEVEL;
- final loading into chorus.

Every quantity has a test route in `PHASE0_MEASUREMENT_PLAN.md`.

## 12. Phase-0 closure state

Architecture/implementation decisions now closed:

```text
[x] one global four-route HPF selector
[x] semantic positions 0/1/2/3
[x] HPF1 straight branch
[x] HPF2 exact component-ideal first-order transfer
[x] HPF3 exact component-ideal first-order transfer
[x] HPF0 direct + two-stage/two-pole-two-zero shaped branch
[x] IC4A 106-specific isolation/summing boundary
[x] raw7<<7 VCA LEVEL digital representation
[x] separate Roland external-control network and uPC1252 core
[x] uPC1252 current-mode global VCA boundary
[x] explicit post-VCA I/V/output stage
[x] authentic high-level distortion/headroom requirement
[x] no active-voice normalization
[x] unresolved calibration centralized and measurable
```

Physical calibration still pending:

```text
[ ] HPF0 exact measured/netlist coefficients
[ ] installed selector Ron
[ ] IC4A analogue headroom/slew
[ ] 106 code -> uPC1252 control-voltage curve
[ ] full VCA LEVEL -> gain/dB curve
[ ] nonlinear/headroom/THD curve
[ ] post-VCA output gain/loading into chorus
```

Those values can be replaced without changing the frozen graph.

**Phase-0 global-output state: `GLOBAL_OUTPUT_PHASE0_ARCHITECTURE_FROZEN`.**
