# JUNO-106 digital VCF-control arithmetic

Status: Phase 0 behavioral/arithmetic freeze for the module-CPU VCF-CV path.

This document deliberately stops at the value written to each voice's VCF sample-and-hold. The analogue A1QH80017A / IR3109 response to that control value is specified separately.

Source basis:

- Roland JUNO-106 service documentation for VCF-control architecture and nominal ranges;
- public `ErroneousBosh/j106roms` module/voice CPU disassembly (`ic29.txt`) for literal arithmetic and control-flow order;
- MAME uPD7810/7811 core where instruction semantics need independent confirmation.

Human labels in the public disassembly remain provisional. Arithmetic/order below is based on literal instructions and cross-checkable constants.

## 1. Domain and clamp

The final per-voice VCF control is a 14-bit unsigned value:

```text
0x0000 .. 0x3fff
```

The front-panel Cutoff contribution is already expanded into approximately:

```text
0x0000 .. 0x3f80
```

before the composite VCF calculation begins.

Strict mode must preserve the integer domain and saturation/underflow behavior before converting to any analogue/log-frequency representation.

## 2. Calculation order

The module CPU builds VCF control in two stages.

### 2.1 Global base shared by all six voices

The routine begins with front-panel Cutoff, then applies signed LFO and signed Bender:

```text
global_vcf = cutoff
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

## 3. ENV-to-VCF arithmetic

Let:

```text
env       = current 14-bit envelope, 0..0x3fff
env_depth = VCF ENV parameter, 0..254
```

The ROM forms the scaled ENV contribution with two 8x8 multiplies equivalent to:

```text
env_vcf = floor(env * env_depth / 256)
```

subject to the exact partial-product/truncation order of the CPU.

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

## 4. Key Follow coordinate

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

## 5. Key Follow magnitude and direction

Let:

```text
key_depth = VCF Key Follow parameter, 0..254
```

The firmware computes distance from the Middle-C pivot in the `3/8 pitch` coordinate:

```text
delta = abs(pitch_3_8 - 0x1680)
```

and then:

```text
key_vcf = floor(delta * key_depth / 256)
```

using the same style of two-byte partial multiplication.

Direction is determined by pitch relative to Middle C:

```text
pitch > C4: vcf += key_vcf
pitch < C4: vcf -= key_vcf
pitch = C4: zero contribution
```

Because the source is the current per-voice 8.8 pitch state, Portamento can continuously move the Key Follow control in firmware-rate steps.

This is more faithful than implementing Key Follow as a post-hoc `Hz *= 2^(notes*k)` factor.

## 6. Final saturation

After ENV and Key Follow, the routine resolves the accumulated overflow/underflow state and clamps the per-voice result:

```text
if underflow:
    vcf = 0x0000
else if overflow / >0x3fff:
    vcf = 0x3fff
else:
    vcf = accumulated_value
```

The clamped result is then written through the shared DAC/multiplexer to that physical voice's VCF sample-and-hold.

Strict `MODEL_106` therefore needs a control-domain saturation step **before** the analogue filter law.

## 7. Bender magnitude

The already-frozen Bender path supplies VCF Bender magnitude as:

```text
vcf_bend = (bend_magnitude * vcf_bend_sensitivity) >> 4
```

with explicit external polarity.

At full 8-bit magnitude/sensitivity:

```text
255 * 255 >> 4 = 4064
```

which sits just below the 12-bit full-scale region and feeds the global VCF calculation before ENV/Key Follow.

## 8. LFO contribution

The LFO VCF path uses the same integer LFO magnitude/polarity generator as DCO modulation, but a separate VCF depth scaler.

Architectural freeze:

- LFO magnitude is generated in firmware/control rate;
- its requested VCF depth is multiplied into a control-domain quantity;
- sign is applied in the global VCF base before per-voice ENV and Key Follow;
- strict mode must not inject an independent audio-rate LFO directly into filter Hz.

A compact clean law for the VCF LFO-depth parameter remains a small follow-up item if it cannot be derived without retaining a ROM lookup.

## 9. Clean implementation form

```text
uint16 build_vcf_global(...)
{
    signed32 x = cutoff_14;
    x += signed_lfo_vcf;
    x += signed_bender_vcf;
    // preserve overflow/underflow intent
    return intermediate;
}

uint16 build_vcf_voice(global, env14, env_depth, env_positive,
                       pitch8_8, key_depth)
{
    env_amt = rom_mul16x8(env14, env_depth);
    x = env_positive ? global + env_amt : global - env_amt;

    p38 = (pitch8_8 >> 2) + (pitch8_8 >> 3);
    delta = abs(p38 - 0x1680);
    key_amt = rom_mul16x8(delta, key_depth);
    x += (p38 >= 0x1680) ? key_amt : -key_amt;

    return clamp14_with_rom_overflow_semantics(x);
}
```

The eventual implementation should expose the ROM-style multiply as a shared tested primitive so ENV, Key Follow and other firmware paths do not silently use different rounding.

## 10. Why this matters for the analogue model

The A1QH80017A should not be asked to reconstruct panel semantics.

Its strict interface should receive something close to the actual hardware boundary:

```text
14-bit sampled VCF control -> S/H reconstruction -> analogue VCF-CV law -> IR3109 stages
```

This separation lets us independently test:

- firmware arithmetic;
- control-frame/S&H behavior;
- exponential VCF control law;
- analogue filter response/nonlinearity.

It also prevents compensation errors where a wrong digital law is accidentally 'fixed' by tuning the analogue model.

## 11. Verification vectors

### ENV

- ENV = 0, mid, max;
- depth = 0, 1, 127, 254;
- positive and inverted polarity;
- combinations that exactly hit underflow/overflow boundaries;
- current-level ENV reattack while VCF ENV is active.

### Key Follow

- pitches C2, C3, C4, C5, C6;
- exact Middle C must yield zero Key Follow contribution;
- quarter/semitone fractions around C4 to verify 8.8 arithmetic;
- depth = 0, 127, 254;
- Portamento sweep through C4 to verify sign change without discontinuity beyond integer quantization.

### Combined

- Cutoff near 0 plus negative ENV/LFO/Bend;
- Cutoff near max plus positive ENV/LFO/Bend;
- positive ENV + negative LFO + positive Bend + Key Follow below C4;
- same patch on all six voices with different current Portamento pitches;
- final clamp exactly at 0 and `0x3fff`.

## 12. Remaining digital VCF-control work

Narrow items only:

- derive/verify compact VCF-LFO depth mapping;
- verify exact Cutoff panel scaling into `0x0000..0x3f80`;
- consolidate the common 16x8 partial-multiply primitive and prove its truncation against representative ROM paths;
- verify precise timing/order of per-voice VCF S/H writes inside one full control-service cycle.

The overall VCF-control architecture and arithmetic order are now frozen.