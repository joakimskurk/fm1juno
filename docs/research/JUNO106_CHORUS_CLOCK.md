# JUNO-106 chorus clock and MN3009 delay model

Status: Phase 0 companion to `JUNO106_CHORUS.md`.

Purpose: freeze the **clock-domain semantics and component-level working model** without pretending that the original Mode I/II numeric clock trajectories have already been measured.

## 0. Evidence classes

- **PRIMARY-COMPONENT** — Panasonic/Xvive MN3101/MN3009 device documentation.
- **ORIGINAL-106 SCHEMATIC / STRONG CROSS-CHECK** — original JUNO-106 schematic plus independent repair/modification evidence.
- **106-RECONSTRUCTION** — modern hardware reconstruction explicitly based on the JUNO-106 topology.
- **OPEN** — direct original-unit measurement or cleaner primary schematic read still required.

The modern reconstruction is used to disambiguate topology/component relationships, not as a substitute for measuring an original unit.

## 1. Three clock frequencies must be named separately

The word `clock` is dangerously ambiguous in BBD documentation.

For the strict reference engine define:

```text
f_osc   = MN3101 internal oscillator frequency at OX1/OX2/OX3
f_cp    = frequency present on each MN3101 CP1 / CP2 output
f_xfer  = effective bucket-transfer event rate used by the BBD delay equation
```

### 1.1 MN3101 relation

**PRIMARY-COMPONENT**

The MN3101 documentation states:

```text
f_cp = f_osc / 2
```

CP1 and CP2:

- are opposite phase;
- have approximately 50% duty;
- each run at half the internal oscillator frequency.

Therefore any measured signal must carry a type tag. Never accept an unqualified `clock_hz` in strict code or test data.

Recommended type:

```text
struct BbdClock106 {
    double f_osc_hz;
    double f_cp_hz;
};
```

with one field derived from the other, not independently editable.

## 2. MN3009 delay equation

**PRIMARY-COMPONENT**

MN3009 has 256 BBD stages and is specified for roughly 10–200 kHz BBD clock operation, corresponding to approximately 12.8–0.64 ms of signal delay.

This fixes the delay law when `f_cp` means one of the two physical CP phases:

```text
t_delay = N / (2 * f_cp)
        = 256 / (2 * f_cp)
        = 128 / f_cp
```

Examples:

```text
f_cp =  10 kHz -> t_delay = 12.80 ms
f_cp =  25 kHz -> t_delay =  5.12 ms
f_cp =  50 kHz -> t_delay =  2.56 ms
f_cp = 100 kHz -> t_delay =  1.28 ms
f_cp = 200 kHz -> t_delay =  0.64 ms
```

Equivalent expression in terms of MN3101 internal oscillator frequency:

```text
t_delay = N / f_osc
```

because `f_osc = 2*f_cp`.

This equivalence is useful but strict code should still retain both names so measurements cannot silently change convention.

## 3. Why the two-phase factor matters

A common digital implementation mistake is to write either:

```text
t = N / f_cp
```

or

```text
t = N / (2*f_osc)
```

without defining what the measured frequency represents.

Either can create a factor-of-two error.

FM1JUNO policy:

```text
BbdClockConvention::MN3101_OSC
BbdClockConvention::MN3101_CP_PHASE
```

Conversion functions must be explicit and unit-tested.

## 4. Original JUNO-106 clock-setting resistors

**ORIGINAL-106 SCHEMATIC / STRONG CROSS-CHECK**

The original 106 uses one MN3101 per BBD channel. Independent service/modification literature identifies the clock-setting resistors:

```text
CH1: R124 = 8.2 kOhm
CH2: R137 = 8.2 kOhm
```

A historically documented modification replaces each 256-stage MN3009 with a pin-compatible 1024-stage MN3007 and changes those resistors to about one quarter their value (~2.05 kOhm ideal; 2.2 kOhm often used) to raise the clock rate approximately 4x and preserve approximately the original delay.

This is strong physical confirmation that R124/R137 belong directly to the MN3101 frequency-setting network.

It also gives a useful invariance test:

```text
N_new / N_old = 4
f_new / f_old ~= 4
=> t_new ~= t_old
```

Strict 106 of course retains MN3009 / 8.2 kOhm.

## 5. Working full MN3101 RC network

### 5.1 Cross-source triangulation

A text transcription/clone of the original 106 schematic and a later 106-focused hardware reconstruction converge on the following network per BBD clock driver:

```text
CH1 original designators (working mapping)
R123 ~= 1.8 kOhm
R124 ~= 8.2 kOhm
C53  ~= 150 pF

CH2 original designators (working mapping)
R136 ~= 1.8 kOhm
R137 ~= 8.2 kOhm
C57  ~= 150 pF
```

A modern One-O-Six reconstruction maps its equivalent MN3101 networks as:

```text
clock A: 1.8 kOhm / 8.2 kOhm / 150 pF
clock B: 1.8 kOhm / 8.2 kOhm / 150 pF
```

The symmetry and exact value agreement make this a high-confidence **working network**.

### 5.2 Confidence boundary

Do not yet tag all six original designator/value pairings as `PRIMARY_CONFIRMED` until a clean original schematic crop or direct board read is archived in the repo.

Use:

```text
ClockRc106Working {
  r1 = 1.8e3;
  r2 = 8.2e3;
  c  = 150e-12;
  evidence = SCHEMATIC_CROSS_CHECK;
}
```

The 8.2 kOhm values themselves are stronger than the remaining mapping because of the independent MN3007 conversion evidence.

## 6. Do not reduce MN3101 to a simple guessed RC formula

The MN3101 contains a two-stage inverter oscillator. Panasonic states that oscillator frequency is determined by the external C1/R2 time constant and provides empirical `f_CP vs R2` curves for several R1/C1 combinations.

The datasheet does **not** provide a single universal precision equation that we should assume is exact for the JUNO network.

Therefore strict reference work should not simply assert:

```text
f = 1 / (K*R*C)
```

with a guessed constant `K` and then call the result authentic.

Instead use two levels:

### Diagnostic analytic approximation

```text
f_cp_approx = mn3101_empirical_approx(r1, r2, c, vdd)
```

Useful for sanity checks and parameter sweeps.

### Strict calibration law

```text
f_cp = fitted_MN3101_clock(lfo_control_voltage,
                           original_component_set,
                           measured_clock_capture)
```

The strict function is ultimately fitted against physical 106 driver-clock captures.

## 7. LFO does not directly equal delay modulation

The 106 chorus has an analogue LFO/control network that modulates the MN3101 oscillator network. Consequently:

```text
triangle LFO voltage
    -> transistor/control network
    -> nonlinear change in MN3101 oscillator operating point
    -> f_osc(t)
    -> f_cp(t)
    -> reciprocal BBD delay t_delay(t)
```

Even if the control LFO voltage were a mathematically perfect triangle, **delay time is not expected to be a perfect triangle** because:

1. MN3101 clock frequency vs control network is not guaranteed perfectly linear;
2. BBD delay is reciprocal in clock frequency;
3. the transistor interface can introduce asymmetry/curvature;
4. component tolerances and the two physical channels can differ slightly.

This justifies SoundForce's observation that reproducing the captured driver signal preserves audible imperfections beyond an ideal triangle.

## 8. LFO amplitude anchor

**106-RECONSTRUCTION / service-procedure cross-check**

A 106-focused hardware reconstruction calibrates the normal and inverted chorus modulation signals to approximately:

```text
20 Vp-p
```

with equal amplitude and 180-degree inversion, explicitly referencing the Roland 106 service procedure.

This is a valuable analogue-domain anchor for the control waveform.

Strict model implication:

```text
lfo_A(t) ~= +10 V peak around its operating center
lfo_B(t) ~= inverse of A, matched in amplitude
```

Do not translate `20 Vp-p` directly to delay depth. The MN3101/transistor network sits between this control voltage and `f_cp`.

## 9. I/II rate setting belongs before the MN3101 clock model

The original 106 has one chorus LFO with two preset rates. A 106-derived adjustable hardware implementation uses a triangle LFO spanning roughly 0–1.8 Hz, with physical knob marks for original I and II positions.

This confirms that the original modes are comfortably inside a slow-LFO regime, but does **not** establish the exact original rates.

Model architecture:

```text
ChorusMode106::I  -> measured_lfo_rate_I
ChorusMode106::II -> measured_lfo_rate_II

lfo_voltage = TriangleLike106(rate, phase)
f_cp_A      = ClockDriver106(lfo_voltage)
f_cp_B      = ClockDriver106(-lfo_voltage + channel_trim)
```

Do not use JUNO-60's 0.5/0.8 Hz as strict constants merely because they are plausible.

## 10. Clock-driver-to-delay reference API

Provisional reference API:

```text
enum class ClockDomain106 {
    Oscillator,
    CpPhase
};

struct Mn3101State106 {
    double fOscHz;
    double fCpHz;
};

struct ClockDriver106 {
    Mn3101State106 processControlVoltage(double lfoVolts);
};

double mn3009DelaySeconds(double fCpHz) {
    return 128.0 / fCpHz;
}
```

Runtime implementation on FM-1 will not necessarily use doubles; these names define reference semantics only.

## 11. Measurement protocol to close numeric clock law

Probe **both CP outputs or a clearly identified driver-clock node** on a healthy original 106.

For I and II:

1. capture >= 4 complete LFO periods;
2. identify whether measured signal is OX oscillator or CP phase output;
3. convert every edge period to instantaneous `f_cp(t)`;
4. record min/max/mean/median frequency;
5. fit rising and falling trajectories separately;
6. compare channel A and B after accounting for 180-degree modulation;
7. calculate `t_delay(t) = 128/f_cp(t)`;
8. validate calculated delay with simultaneous audio impulse/phase measurement.

Store raw captures outside source code; store compact fitted coefficients/test vectors in the repository.

Required metadata:

```text
instrument serial / board revision
MN3009 manufacturer/date if visible
supply rails
probe point
scope sample rate
chorus mode
temperature / warmup
bias calibration state
```

## 12. Automated invariants

Before any chorus DSP is considered valid:

### Clock-domain identity

```text
assert_near(fOsc, 2*fCp)
```

### MN3009 range

For normal operation, measured/fitted `f_cp` should be checked against the device's nominal 10–200 kHz range. Out-of-range values must be flagged rather than silently clamped during research.

### Delay identity

```text
assert_near(delay_seconds * fCp, 128)
```

### Stage-scaling sanity check

Diagnostic only:

```text
MN3009: N=256
MN3007: N=1024

if f3007 ~= 4*f3009:
    delay3007 ~= delay3009
```

This reproduces the documented resistor-mod rationale.

## 13. FM-1 simplification path

Once the physical clock trajectory has been measured, the FM-1 does **not** need to emulate the transistor/MN3101 oscillator circuit sample-by-sample.

Preferred deployment representation:

1. fit `f_cp` as a low-order monotonic function of normalized LFO control for each channel;
2. optionally add a small periodic residual LUT for measured non-ideal shape;
3. convert to delay using exact `128/f_cp` relation;
4. drive a fractional-delay/BBD-equivalent path;
5. retain clock-dependent bandwidth/noise/headroom models separately.

This preserves the physical causal chain while avoiding unnecessary high-rate circuit simulation.

## 14. Freeze state

### Frozen

- explicit `f_osc` vs `f_cp` distinction;
- MN3101 CP1/CP2 are opposite-phase and half oscillator frequency;
- MN3009 is 256 stages;
- strict delay law `t = 256/(2*f_cp) = 128/f_cp`;
- original clock-setting R124/R137 = 8.2 kOhm strongly established;
- working symmetric external oscillator network ~= 1.8 kOhm / 8.2 kOhm / 150 pF per channel;
- clock-domain modulation precedes reciprocal delay conversion;
- 20 Vp-p opposite control-LFO amplitude is a useful 106 reference anchor;
- no Juno-60 numerical rate substitution in strict 106.

### Open

- archive clean original values/designators for R123/R124/C53 and R136/R137/C57;
- exact original I and II LFO frequencies;
- exact `f_cp(t)` min/max/mean in I and II;
- exact MN3101 clock transfer under original transistor control network;
- A/B channel mismatch;
- direct validation of `128/f_cp` against simultaneous original-106 audio delay measurement;
- clock-dependent BBD bandwidth and insertion loss across the measured sweep.
