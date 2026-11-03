# JUNO-106 pitch pipeline, 8253 count path and DCO-CV law

Status: Phase 0 behavioural freeze for the digital pitch/control path. The analogue MC5534/range-resistor scale remains a separate unresolved circuit-calibration task.

Source basis:

- Roland JUNO-106 service documentation;
- public module/voice CPU disassembly `ErroneousBosh/j106roms/ic29.txt`;
- Intel 8253 Mode-3 behavior;
- arithmetic cross-checks against Roland's published Tune/DCO-LFO/Bender ranges.

No full original ROM tables are reproduced here. The goal is to document observable arithmetic and independently derivable laws.

## 1. One pitch coordinate feeds both timer frequency and DCO ramp compensation

The module CPU represents musical pitch in 8.8 fixed-point semitone units:

```text
uint16 pitch_8_8
high byte = integer semitone
low byte  = 1/256 semitone
```

The same final modulated pitch coordinate is used to derive:

1. the integer 8253 timer divisor that determines DCO timing;
2. the DCO-CV DAC value that controls MC5534 ramp charge current/amplitude compensation.

This coupling is important: pitch modulation and ramp-amplitude compensation track each other through the same firmware-domain pitch.

## 2. Exact ordering of the pitch build

The ROM separates a global pitch offset from each voice's Portamento state.

### 2.1 Global offset

The calculation begins from a fixed 8.8 base constant:

```text
base = 0x1818
```

The firmware then applies, in order:

1. Tune magnitude with Tune sign;
2. DCO LFO pitch amount with current LFO polarity;
3. Bender pitch amount with Bender polarity.

The result is stored as a reusable global offset for all six voices.

Conceptually:

```text
global_pitch_offset = 0x1818
                    + signed_tune
                    + signed_lfo_pitch
                    + signed_bender
```

### 2.2 Per-voice Portamento

Each voice keeps its own current 8.8 Portamento pitch state.

Before final DCO conversion, Portamento moves that state toward the requested integer-note target by a constant integer step and clamps on overshoot:

```text
voice_pitch = step_toward(voice_pitch, note << 8, porta_step)
```

Portamento therefore occurs **before** Tune/LFO/Bender are applied to the final pitch.

### 2.3 Final per-voice pitch

For every physical voice:

```text
final_pitch = voice_porta_pitch + global_pitch_offset
```

The high byte then chooses the integer-note entries; the low byte becomes the interpolation fraction used by both the timer-divisor and DCO-CV paths.

This ordering should be frozen in strict `MODEL_106`.

## 3. Tune law

The Tune ADC value is centered around `0x80`. Firmware converts it into:

- a sign bit;
- an unsigned magnitude up to approximately `0x7F`.

That magnitude is added/subtracted directly in the 8.8 semitone domain.

Therefore the maximum displacement is approximately:

```text
127 / 256 semitone ~= 0.4961 semitone ~= 49.6 cents
```

This independently matches Roland's published Tune specification of approximately +/-50 cents.

Clean implementation:

```text
signed_tune_8_8 = clamp(raw_tune - 128, -127, +127)
```

The exact ADC endpoint asymmetry can remain a test detail; the musical law itself is now strongly constrained.

## 4. LFO-to-pitch law

DCO-LFO depth is not applied as an audio-rate frequency multiplier.

The firmware path is:

1. panel DCO-LFO value selects an 8-bit depth scaler;
2. LFO Delay/onset envelope scales that depth;
3. modulation-wheel contribution is added and saturated;
4. this combined depth multiplies the current integer LFO magnitude;
5. the product is shifted down into an 8.8-domain pitch amount;
6. the LFO polarity flag decides add versus subtract in the global pitch build.

The exact ROM depth curve should be represented by a clean derived/calibrated law rather than copied wholesale.

Validation anchor: Roland specifies DCO LFO modulation up to approximately +/-400 cents.

## 5. Bender path

Physical/MIDI bend handling computes separate DCO and VCF bend quantities.

For DCO pitch:

- raw bend magnitude is multiplied by DCO Bend Sensitivity;
- firmware normalizes the result into semitone plus fractional-pitch representation;
- a polarity flag determines add/subtract in the global 8.8 pitch build.

Validation anchor: Roland specifies DCO Bender range up to approximately +/-1200 cents.

Remaining work is to express the complete raw-control-to-8.8 formula in a compact clean-room equation and verify intermediate scaling at representative sensitivity values. The ordering and final pitch domain are already frozen.

## 6. Portamento slider/index law

The Portamento ADC path is more specific than a generic 0..1 time mapping.

Observed behavior:

1. firmware reads the 8-bit Portamento control;
2. zero follows the OFF/no-step path;
3. non-zero values are shifted right one bit;
4. the resulting 0..127 index selects an 8-bit Portamento step coefficient;
5. that coefficient is the amount added/subtracted from each voice's 8.8 pitch on a control-loop pass.

Thus:

```text
porta_index = raw_porta >> 1
porta_step  = clean_derived_step_table[porta_index]
```

The original ROM table is not required in source control. We can derive a compatible compact representation and validate representative indices against extracted behavior.

## 7. Timer-divisor conversion

The final pitch high byte selects a 16-bit note-divisor entry. The following semitone's entry is also read.

For fractional byte `f` in `0..255`:

```text
N0 = divisor[n]
N1 = divisor[n + 1]
d  = N0 - N1
N  = N0 - floor_like_rom(d * f / 256)
```

The ROM performs this with 8x8 partial products. Strict code should reproduce the observed integer truncation/order rather than replacing it with floating-point interpolation if that creates different counts.

Important: this is interpolation in **timer-count space**, not a direct ideal-frequency exponential calculation.

The result is written to the selected 8253 counter LSB first, then MSB.

## 8. 8253 Mode-3 update semantics

Strict timer state must distinguish an ordinary count update from an oscillator reset.

### 8.1 Ordinary count update

For Tune, LFO, Bend and Portamento movement on a running timer:

- firmware writes the new LSB/MSB only;
- Mode-3 control word is not rewritten;
- the currently executing half-cycle is preserved;
- the newly written count becomes active at the next Mode-3 output transition / half-cycle boundary.

Implementation consequence:

```text
Timer106::queue_count(N)
```

must not immediately replace the active half-cycle's duration.

### 8.2 Reset-and-load

When `resetVoiceBits` requests oscillator reset:

- firmware writes `36`, `76` or `B6` to the selected counter control register;
- then writes the new count LSB/MSB.

This is a distinct synchronization event and should be implemented separately:

```text
Timer106::reset_and_load_count(N)
```

## 9. DCO-CV digital law

The ROM contains a per-semitone DCO-CV table used alongside the timer-divisor table. Analysis of its values shows a simple underlying law to within table rounding:

```text
cv_code(n) ~= round(32 * 2^(n / 12))
```

with saturation near the 12-bit DAC maximum.

Evidence for this interpretation:

- the code approximately doubles every octave;
- intermediate semitones follow the twelfth-root-of-two progression;
- the service documentation explains that DCO CV controls ramp charge current so oscillator amplitude remains approximately constant as DCO frequency rises.

The firmware linearly interpolates adjacent DCO-CV codes using the same final fractional pitch byte, then left-aligns/scales the result for the DAC.

Strict clean-room implementation should therefore use the independently derivable exponential law plus ROM-equivalent quantization/interpolation rather than embed the complete original table.

## 10. Why DCO CV grows exponentially with pitch

For a ramp capacitor charged by approximately constant current during one oscillator period:

```text
DeltaV ~= I * T / C
```

and because:

```text
T = 1 / f
```

maintaining nearly constant ramp amplitude requires approximately:

```text
I proportional to f
```

A semitone frequency law is exponential, so a DCO-CV code that doubles every octave is exactly the expected digital compensation shape.

The range-selected analogue resistor network then maps this common DAC law into the appropriate current scale for 4', 8' and 16'.

## 11. What remains analogue/open

The **digital** DCO-CV law is sufficiently constrained to freeze.

Still open:

- exact R85/R86/R87 range-resistor values and effective current ratios;
- C54 value/effective charging capacitance if schematic extraction remains ambiguous;
- transistor/current-source nonlinearity inside/around MC5534;
- finite discharge/reset transient;
- DAC/S&H output impedance and settling;
- actual ramp amplitude versus note/range on hardware;
- whether component tolerances create meaningful per-voice DCO amplitude variance.

Do not guess these constants. They belong in the next circuit-extraction/measurement slice.

## 12. Clean API shape

```text
Pitch106
  global_offset_8_8(tune, lfo, bend)
  portamento_tick(voice, target_note, step)
  final_pitch_8_8(voice, global_offset)
  timer_count_from_pitch(pitch_8_8)
  dco_cv_code_from_pitch(pitch_8_8)

Timer106
  queue_count(N)
  reset_and_load_count(N)
  advance_master_clocks(...)

DcoCv106
  quantized_code(pitch_8_8)
  apply_range_network(range, code)
```

The API intentionally keeps digital DCO-CV generation separate from analogue range/current modeling.

## 13. Verification vectors

### Pitch arithmetic

- Tune center and both endpoints;
- LFO zero, positive peak and negative peak;
- Bender center and +/- maximum at several sensitivity settings;
- combinations of Tune + LFO + Bend near carry/borrow boundaries;
- Portamento up/down across semitone and octave boundaries.

### Timer conversion

- integer semitones across all ranges;
- fractions `0x00`, `0x40`, `0x80`, `0xC0`, `0xFF`;
- compare ROM-equivalent integer interpolation against ideal `clock/f` only as diagnostics;
- count update during first and second Mode-3 half-cycle;
- reset-and-load versus ordinary queue-count phase response.

### DCO CV

- verify octave doubling before saturation;
- verify all semitone values stay within expected +/-1-LSB rounding of `32*2^(n/12)` where unsaturated;
- verify fractional interpolation uses the same pitch fraction as divisor interpolation;
- sweep range 4'/8'/16' once analogue resistor ratios are extracted;
- verify ramp amplitude remains near Roland's ~12 Vpp target.

## 14. Remaining digital uncertainty

- exact bender normalization equation and intermediate truncation;
- exact firmware-loop cadence under different workloads;
- note-range clamp behavior at every boundary;
- whether any control update can arrive between LSB and MSB timer writes in real hardware (interrupts are disabled around the pair in the observed routine, so strict implementation should treat the pair atomically);
- Solo-Unison timing when all six physical counters receive a new note.

These items are narrow validation tasks rather than architectural unknowns.