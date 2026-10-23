# JUNO-106 DCO ramp numerical triangulation

Status: Phase 0 numerical cross-check of the MC5534 amplitude-compensation model.

Purpose: determine whether the working `399k / 200k / 100k` RANGE resistor set and `~1 nF` ramp capacitor are merely plausible borrowed values or whether they are independently consistent with the JUNO-106's own firmware tables and Roland's primary electrical specifications.

## 1. Independent evidence inputs

### A. Roland service documentation — primary

The module CPU represents voice control targets as 12-bit data and converts them through the shared DAC/S&H system.

The service timing diagram labels the conditioned **DCO CV / SUB LEVEL** domain as nominally:

```text
0 V .. -10 V
```

Roland also states:

- DCO CV charges the ramp capacitor through the RANGE-selected resistor;
- R85 is selected for 16';
- positive DCO edges discharge/reset the capacitor;
- DCO CV is designed to keep saw/pulse amplitude nearly constant;
- target saw/pulse amplitude is approximately 12 Vp-p.

### B. Module ROM — firmware evidence

The recovered DCO-CV table contains octave anchor values that double exactly on C notes:

```text
C0-like table anchor: 0x020 = 32
+12 semitones:        0x040 = 64
+24 semitones:        0x080 = 128
+36 semitones:        0x100 = 256
```

This is consistent with the independently derived compact law approximately:

```text
DCO_CV_code(n) ~= round(32 * 2^(n/12))
```

The recovered 8253 divisor table contains matching C-note octave anchors approximately:

```text
0xF06E = 61550
0x780F = 30735
0x3BFC = 15356
0x1DFB =  7675
```

At the 16' timer clock of 1 MHz, these produce approximately:

```text
16.247 Hz
32.536 Hz
65.121 Hz
130.293 Hz
```

The slight deviations from mathematically ideal equal temperament are the actual integer timer/table behavior and are exactly what the reference model should preserve.

### C. RANGE resistor values — schematic/cross-check evidence

Multiple independent circuit readings identify the three selected values as:

```text
399 kOhm
200 kOhm
100 kOhm
```

Roland explicitly identifies R85 as the 16' path. The required current scaling fixes the mapping:

```text
16' / R85 ~= 399 kOhm
 8' / R86 ~= 200 kOhm
 4' / R87 ~= 100 kOhm
```

## 2. Nominal DAC voltage mapping

Using Roland's nominal 12-bit, 0..-10 V DCO-CV range, a first-order factory-reference mapping is:

```text
Vdco_nominal(code) = -10 V * code / 4095
```

This is a nominal conditioned-output law, not yet a statement that every real unit reaches exactly -10.000 V at code 4095 under every load.

The service procedure separately trims DCO-CV zero to 0 V at TP3 using VR33, so the factory-reference model should not invent a permanent DC offset.

## 3. Solve for the ramp capacitor instead of assuming it

For a linear inverting ramp:

```text
DeltaV = abs(Vdco) / (R * C * f)
```

Solve for C at Roland's approximate 12 V target:

```text
C = abs(Vdco) / (R * f * 12 V)
```

Use the lowest C anchor in 16' mode:

```text
code       = 32
Vdco       = 10 * 32 / 4095
           = 0.078144 V nominal magnitude

divisor    = 0xF06E = 61550
f          = 1,000,000 / 61550
           = 16.246954 Hz

R16        = 399,000 Ohm

C_required = 0.078144 / (399000 * 16.246954 * 12)
           ~= 1.0045 nF
```

Repeat at octave anchors:

| Pitch-table offset | DCO-CV code | 16' divisor | Frequency | C required for 12 V |
|---:|---:|---:|---:|---:|
| 0  | 32  | 61550 | 16.247 Hz  | ~1.0045 nF |
| 12 | 64  | 30735 | 32.536 Hz  | ~1.0032 nF |
| 24 | 128 | 15356 | 65.121 Hz  | ~1.0025 nF |
| 36 | 256 | 7675  | 130.293 Hz | ~1.0021 nF |

The result converges tightly around **1.0 nF**.

This is an important confidence upgrade: `C ~= 1 nF` is independently implied by the JUNO-106's own ROM-derived code/frequency relationships, Roland's nominal DCO-CV voltage range, the RANGE resistor network and Roland's ~12 Vp-p amplitude target.

It is no longer supported only by a secondary reconstruction.

## 4. Predict ramp amplitude using C = 1 nF

Use:

```text
C = 1.0 nF
R16 = 399 kOhm
```

The same four C-note anchors predict:

| Offset | Predicted ideal ramp excursion |
|---:|---:|
| 0  | ~12.055 V |
| 12 | ~12.039 V |
| 24 | ~12.030 V |
| 36 | ~12.025 V |

That is strikingly consistent with Roland's statement that the amplitude is kept at approximately 12 Vp-p.

The small residual slope is exactly the sort of structured imperfection expected from integer divisors, quantized CV codes and the non-ideal `399k` ratio.

## 5. Cross-range check

At the same table note (`code=32`, `divisor=61550`):

### 16'

```text
clock = 1 MHz
R = 399 kOhm
f ~= 16.247 Hz
DeltaV ~= 12.055 V
```

### 8'

```text
clock = 2 MHz
R = 200 kOhm
f ~= 32.494 Hz
DeltaV ~= 12.024 V
```

### 4'

```text
clock = 4 MHz
R = 100 kOhm
f ~= 64.988 Hz
DeltaV ~= 12.024 V
```

So the proposed resistor mapping reproduces essentially the same ramp excursion across all three RANGE positions without any arbitrary output normalization.

This is precisely the behavior Roland describes.

## 6. Confidence classification update

### Strongly triangulated

The following combination is now strong enough to use as the default Phase-1 reference model:

```text
R16 ~= 399 kOhm
R8  ~= 200 kOhm
R4  ~= 100 kOhm
Cint ~= 1.0 nF
DCO CV nominal conditioned range ~= 0 .. -10 V
```

`R85/R86/R87` numeric designator mapping should still be archived from a clean primary schematic/board read for documentation rigor, but the system-level numerical fit is too exact to treat these values as arbitrary guesses.

### Not yet frozen

This calculation does **not** establish:

- exact 4052 on-resistance;
- S/H output impedance or droop;
- MC5534 internal input/buffer resistance;
- exact op-amp/comparator saturation limits;
- reset-transistor timing/residual;
- real-unit full-scale DAC gain error;
- per-voice passive tolerances;
- loaded DCO-CV voltage at the MC5534 pin.

Those effects are where the remaining non-ideal amplitude curve should come from.

## 7. Reference-engine consequence

Do not implement a normalized saw oscillator.

Instead:

```text
held_dco_cv = nominal_DAC_voltage(code) + calibration/tolerance terms
slope       = -held_dco_cv / (R_range * Cint)
ramp        = integrate slope between timer-edge reset events
```

The predicted waveform amplitude should emerge from the model.

A global normalization may be applied only much later in the signal chain if required to map hardware-voltage units into safe DSP headroom; it must not erase note/range-dependent source behavior.

## 8. Verification target for eventual real-hardware capture

A very high-value measurement set would record raw wave-generator saw output for:

- several C notes per RANGE;
- equivalent pitches across adjacent RANGE settings;
- DCO CV at the corresponding S/H output and at the MC5534 input;
- reset minimum/residual and reset duration.

Compare:

```text
measured_slope
vs
-Vdco_loaded / (R_effective * C_effective)
```

The remaining difference then becomes a physically meaningful correction term rather than a generic 'analog character' control.
