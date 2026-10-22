# JUNO-106 digital VCF-control arithmetic

Status: Phase 0 behavioral/arithmetic freeze for the module-CPU VCF-CV path.

This document deliberately stops at the value written to each voice's VCF sample-and-hold. The analogue A1QH80017A / IR3109 response to that control value is specified separately.

Source basis:

- Roland JUNO-106 service documentation for VCF-control architecture and nominal ranges;
- public `ErroneousBosh/j106roms` module/voice CPU disassembly (`ic29.txt`) for literal arithmetic and control-flow order;
- MAME uPD7810/7811 core where instruction semantics need independent confirmation.

Human labels in the public disassembly remain provisional. Arithmetic/order below is based on literal instructions and cross-checkable constants.

## 1. Domain and exact Cutoff scaling

The final per-voice VCF control is a 14-bit unsigned value:

```text
0x0000 .. 0x3fff
```

The incoming panel/SysEx Cutoff parameter is `0..127` and uses the ROM's common `store16Bit` path:

```text
cutoff_14 = cutoff_7bit << 7
```

Therefore:

```text
0   -> 0x0000
1   -> 0x0080
127 -> 0x3f80
```

This is exact firmware behavior, not an approximation.

Strict mode must preserve this integer domain and later saturation/underflow behavior before converting to any analogue/log-frequency representation.

## 2. Calculation order

The module CPU builds VCF control in two stages.

### 2.1 Global base shared by all six voices

The routine begins with front-panel Cutoff, then applies signed LFO and signed Bender:

```text
global_vcf = cutoff_14
           + signed_lfo_vcf
           + signed_bender_vcf
```

LFO polarity is taken from the same firmware LFO polarity state used elsewhere. Bender magnitude/polarity is generated separately.

Overflow/underflow state is tracked explicitly rather than relying on ordinary wrapping arithmetic.

### 2.2 Per-voice additions

For each physical voice, the shared base is then modified by:

1. that voice's current 14-bit ENV level scaled by VCF ENV depth and polarity;
2. that voice's current 8.8 pitch scaled by Key Follow around a Middle-C pivot;
3. final clamp to `0x0000..0x3fff`;
4. write to that voice's VCF DAC/S&H channel.

Therefore ENV and Key Follow are genuinely per-voice control terms, while panel Cutoff, global LFO and Bender are computed once and reused.

## 3. Shared 16x8 scaled-multiply primitive

Several VCF-control paths use the same uPD7811 arithmetic pattern.

Given a 16-bit value:

```text
value = VH:VL
```

and 8-bit multiplier `m`, the instruction sequence computes:

```text
mul16x8_hi(value, m)
    = (VH * m) + ((VL * m) >> 8)
    = floor(value * m / 256)
```

The two products are performed as separate 8x8 multiplies, so strict code should retain explicit integer truncation.

This helper is used below for ENV, Key Follow and VCF LFO scaling.

## 4. ENV-to-VCF arithmetic

Let:

```text
env       = current 14-bit envelope, 0..0x3fff
env_depth = VCF ENV parameter, 0..254
```

The panel parameter is doubled on receipt:

```text
env_depth = raw_env_depth_7bit << 1
```

The scaled ENV contribution is:

```text
env_vcf = mul16x8_hi(env, env_depth)
        = floor(env * env_depth / 256)
```

The VCF ENV polarity switch then selects:

```text
vcf += env_vcf
```

or:

```text
vcf -= env_vcf
```

with overflow/underflow tracked for the final clamp.

Important consequences:

- the ENV contribution is not converted to Hz before summing;
- inverted ENV uses subtraction in the same 14-bit control domain;
- strict implementation should preserve integer truncation before any analogue cutoff mapping.

## 5. Key Follow coordinate

Key Follow uses the physical voice's current **8.8 pitch state**, not merely raw MIDI note number.

The firmware first computes:

```text
pitch_3_8 = (pitch_8_8 >> 2) + (pitch_8_8 >> 3)
```

which is the integer/truncated implementation of:

```text
pitch_3_8 ~= pitch_8_8 * 3/8
```

The pivot constant is:

```text
0x1680
```

This equals:

```text
60 * 256 * 3/8 = 0x1680
```

so the Key Follow zero point is exactly Middle C / MIDI note 60 in this coordinate.

## 6. Key Follow magnitude and direction

The incoming Key Follow parameter is likewise doubled:

```text
key_depth = raw_key_follow_7bit << 1   // 0..254
```

The firmware computes distance from the Middle-C pivot:

```text
delta = abs(pitch_3_8 - 0x1680)
```

and then:

```text
key_vcf = mul16x8_hi(delta, key_depth)
        = floor(delta * key_depth / 256)
```

Direction is determined by pitch relative to Middle C:

```text
pitch > C4: vcf += key_vcf
pitch < C4: vcf -= key_vcf
pitch = C4: zero contribution
```

Because the source is the current per-voice 8.8 pitch state, Portamento moves Key Follow in the same firmware-rate pitch steps.

This is more faithful than implementing Key Follow as a post-hoc `Hz *= 2^(notes*k)` factor.

## 7. Exact VCF-LFO magnitude law

VCF-LFO depth is a separate panel parameter from DCO-LFO depth.

On receipt:

```text
vcf_lfo_depth = raw_vcf_lfo_7bit << 1   // 0..254
```

The automatic LFO Delay produces an onset multiplier:

```text
onset = 0..255
```

The panel VCF-LFO depth is first scaled by that onset:

```text
depth_after_delay = floor(vcf_lfo_depth * onset / 256)
```

Then the current firmware LFO magnitude `lfo_mag` (`0..0x1fff`) is scaled with the shared 16x8 helper and divided by two:

```text
vcf_lfo_signal = mul16x8_hi(lfo_mag, depth_after_delay) >> 1
```

Equivalently:

```text
vcf_lfo_signal = floor(lfo_mag * depth_after_delay / 256) >> 1
```

with sign applied later from the LFO polarity bit.

At maximum nominal panel depth and completed Delay:

```text
vcf_lfo_depth  = 254
onset           = 255
depth_after_delay = floor(254*255/256) = 253
lfo_mag max     = 8191
mul16x8_hi      = floor(8191*253/256) = 8095
vcf_lfo_signal  = 8095 >> 1 = 4047
```

So maximum VCF-LFO modulation naturally lands near a 12-bit control span without inserting an arbitrary floating-point "octave depth" constant.

This is an important structural cross-check against the service specification's large VCF-LFO range.

Unlike the DCO pitch performance-modulation path, this VCF-LFO calculation uses the panel depth/onset path directly; the separately recovered performance DCO-LFO contribution should not be assumed to modulate VCF unless a distinct ROM path proves it.

## 8. Bender magnitude

The frozen Bender path supplies VCF Bender magnitude as:

```text
vcf_bend = (bend_magnitude * vcf_bend_sensitivity) >> 4
```

with explicit external polarity.

At full 8-bit magnitude/sensitivity:

```text
255 * 255 >> 4 = 4064
```

which likewise sits just below the 12-bit full-scale region and feeds the global VCF calculation before ENV/Key Follow.

## 9. Global VCF composition

The firmware order is:

```text
x = cutoff_14
x = signed_add_or_sub(x, vcf_lfo_signal, lfo_polarity)
x = signed_add_or_sub(x, vcf_bend, bend_polarity)
```

This global intermediate is then reused for all six voices.

Overflow/underflow state from these operations is carried into the later per-voice clamp logic rather than silently wrapping.

## 10. Per-voice ENV + Key Follow composition

For each voice:

```text
x_voice = global_vcf

env_amt = mul16x8_hi(env14[voice], env_depth)
x_voice = env_positive ? x_voice + env_amt
                       : x_voice - env_amt

p38      = (pitch8_8[voice] >> 2) + (pitch8_8[voice] >> 3)
delta    = abs(p38 - 0x1680)
key_amt  = mul16x8_hi(delta, key_depth)
x_voice += (p38 >= 0x1680) ? key_amt : -key_amt
```

The result is then saturated to the 14-bit DAC-control domain.

## 11. Final saturation

After ENV and Key Follow, the routine resolves accumulated overflow/underflow and clamps:

```text
if underflow:
    vcf = 0x0000
else if overflow / >0x3fff:
    vcf = 0x3fff
else:
    vcf = accumulated_value
```

The clamped result is written through the shared DAC/multiplexer to that physical voice's VCF sample-and-hold.

Strict `MODEL_106` therefore needs control-domain saturation **before** the analogue filter law.

## 12. Clean implementation form

```text
uint16 mul16x8_hi(uint16 value, uint8 m)
{
    return ((value >> 8) * m) + (((value & 0xff) * m) >> 8);
}

uint16 cutoff_from_panel(uint8 p)
{
    return (uint16)p << 7;
}

uint16 vcf_lfo_from_panel(uint8 depth7, uint8 onset, uint16 lfoMag)
{
    uint8 d = depth7 << 1;
    uint8 dDelayed = (d * onset) >> 8;
    return mul16x8_hi(lfoMag, dDelayed) >> 1;
}
```

The actual global/per-voice sum functions should preserve the ROM's overflow/borrow intent explicitly rather than rely on unsigned C wrapping.

## 13. Why this matters for the analogue model

The A1QH80017A should not reconstruct panel semantics.

Its strict interface should receive something close to the actual hardware boundary:

```text
14-bit sampled VCF control
    -> DAC/S&H voltage
    -> per-voice Frequency/Width calibration
    -> IR3109 exponential converter
    -> four analogue filter stages
```

This separation lets us independently test:

- firmware arithmetic;
- control-frame/S&H behavior;
- exponential VCF control law;
- analogue filter response/nonlinearity.

It also prevents compensation errors where a wrong digital law is accidentally 'fixed' by tuning the analogue model.

## 14. Verification vectors

### Cutoff

- every 7-bit panel value against `p<<7`;
- endpoints `0 -> 0x0000`, `127 -> 0x3f80`.

### VCF LFO

- depth 0, 1, 63, 64, 126, 127;
- onset 0, 1, 127, 254, 255;
- LFO magnitude 0, 1, midpoint, 0x1fff;
- maximum path must yield 4047 before sign;
- positive and negative LFO polarity;
- verify integer truncation at both multiplication stages.

### ENV

- ENV = 0, mid, max;
- depth = 0, 1, 127, 254;
- positive and inverted polarity;
- combinations that exactly hit underflow/overflow boundaries;
- current-level ENV reattack while VCF ENV is active.

### Key Follow

- pitches C2, C3, C4, C5, C6;
- exact Middle C must yield zero Key Follow contribution;
- fractional 8.8 pitches around C4;
- depth = 0, 127, 254;
- Portamento sweep through C4 to verify sign change without discontinuity beyond integer quantization.

### Combined

- Cutoff near 0 plus negative ENV/LFO/Bend;
- Cutoff near max plus positive ENV/LFO/Bend;
- positive ENV + negative LFO + positive Bend + Key Follow below C4;
- same patch on all six voices with different Portamento pitch states;
- final clamp exactly at 0 and `0x3fff`.

## 15. Remaining digital VCF-control work

The arithmetic law is now essentially frozen. Remaining work is timing/edge detail:

- verify precise order and spacing of the six per-voice VCF S/H writes inside one complete control-service cycle;
- verify how DAC/S&H settling shapes each stepped code before the IR3109 CV input;
- prove any subtle carry/borrow corner cases against an executable CPU/ROM reference when available.

No generic floating VCF-modulation law is required for strict mode.