# JUNO-106 Bender normalization

Status: Phase 0 arithmetic freeze for the module-CPU Bender path.

Source basis: literal arithmetic in the public module/voice ROM disassembly, cross-checked against Roland's published DCO Bender range.

## 1. Inputs

The module CPU maintains an unsigned bend magnitude `b` and separate polarity. DCO and VCF have independent Bend Sensitivity controls.

For the arithmetic below:

```text
b     = bend magnitude, 0..255
sdco  = DCO Bend Sensitivity, 0..255
svcf  = VCF Bend Sensitivity, 0..255
```

Polarity is applied separately after the magnitude is formed.

## 2. DCO Bender arithmetic

The ROM multiplies magnitude and DCO sensitivity with the CPU's 8x8 multiply:

```text
p = b * sdco          // 0..65025
```

It then performs the following integer operations, with truncation at every shift:

```text
q = (p >> 1) + (p >> 2) + (p >> 8)
bend_pitch_8_8 = q >> 4
```

The result is subsequently consumed as a signed amount in the global 8.8 semitone pitch build.

This exact sequence should be retained rather than replaced by a floating scale factor because the shift truncation is deterministic and inexpensive on FM-1.

## 3. Independent range verification

At maximum magnitude and maximum DCO sensitivity:

```text
p = 255 * 255 = 65025
q = (65025 >> 1) + (65025 >> 2) + (65025 >> 8)
  = 32512 + 16256 + 254
  = 49022

bend_pitch_8_8 = 49022 >> 4
                 = 3063
```

Converting 8.8 semitone units:

```text
3063 / 256 = 11.96484375 semitones
```

That is ~1196.5 cents, essentially the Roland specification of approximately +/-1200 cents once integer endpoint quantization is considered.

This match is a strong cross-check of the recovered byte alignment and normalization path.

## 4. VCF Bender arithmetic

The VCF path is simpler:

```text
p = b * svcf
vcf_bend = p >> 4
```

At maximum:

```text
65025 >> 4 = 4064
```

which lands just below full 12-bit scale, consistent with the surrounding VCF-CV arithmetic.

The final conversion of this 12-bit control amount into octaves/cutoff remains part of the broader VCF-CV model, but Bender magnitude generation itself is now frozen.

## 5. Polarity

Physical/MIDI Bender handling stores polarity separately from magnitude. The global DCO pitch builder later adds or subtracts `bend_pitch_8_8` according to the polarity state.

Likewise the VCF builder adds or subtracts the VCF Bender amount before per-voice ENV/key-follow processing.

Do not encode polarity by two's-complementing the intermediate magnitude unless tests prove bit-exact equivalence across every underflow/overflow path; preserving the ROM's explicit magnitude + polarity model is clearer.

## 6. Implementation form

```text
uint16 dco_bend_8_8(uint8 b, uint8 sensitivity)
{
    uint16 p = b * sensitivity;
    uint16 q = (p >> 1) + (p >> 2) + (p >> 8);
    return q >> 4;
}

uint16 vcf_bend_12(uint8 b, uint8 sensitivity)
{
    return (b * sensitivity) >> 4;
}
```

The production implementation should use explicit-width unsigned arithmetic and tests for all endpoint/carry cases.

## 7. Verification vectors

For both DCO and VCF paths:

- `b = 0`, sensitivity 0/max;
- `b = 1`, sensitivity 1/max;
- center/representative values 32, 64, 127, 128, 192, 254, 255;
- all sensitivity endpoints;
- positive and negative polarity;
- physical Bender and MIDI-derived magnitude paths producing the same `(b, sensitivity)` pair;
- combinations with Tune and LFO near 8.8 carry/borrow boundaries.

For DCO specifically:

- assert maximum = 3063 8.8 units;
- assert `3063 / 256 = 11.96484375` semitones;
- compare measured hardware maximum against the nominal ~12-semitone service specification.

## 8. Remaining Bender uncertainty

The normalization from `(b, sensitivity)` to DCO/VCF modulation is resolved.

Still to verify separately:

- exact physical lever ADC center/dead-zone/hysteresis behavior;
- exact MIDI pitch-bend preprocessing into the 0..255 magnitude domain;
- any difference in update cadence between Bender ADC reads and the main multiplexed CV service loop.

These do not require changes to the normalized DCO/VCF arithmetic above.