# JUNO-106 HPF and global output/VCA topology

Status: Phase 0 reference specification for strict `MODEL_106`.

This block begins **after the six per-voice 80017A VCA outputs have been summed** and ends at the input to the chorus/output section.

## 0. Why this block must remain global

The JUNO-106 front panel places HPF visually before VCF/VCA, but the hardware does not.

Physical signal order is:

```text
6 x voice VCF/VCA outputs
        |
        +--> mono voice sum
                |
                +--> global fixed-response HPF selector
                        |
                        +--> IC4A buffer/summing stage
                                |
                                +--> global uPC1252 VCA (VCA LEVEL)
                                        |
                                        +--> chorus / output chain
```

Therefore:

- do not place one HPF per voice;
- do not place VCA LEVEL inside each 80017A voice;
- do not calibrate the chorus from pre-global-VCA signal levels.

## 1. HPF selector and firmware encoding

**PRIMARY/FIRMWARE-CONFIRMED**

The module CPU receives the HPF state in switch data and explicitly transforms the two HPF bits before writing the external control latch:

```text
A = switch2
A ^= 0x18
A &= 0x18
A >>= 1
merge into IC40 latch bits
```

This inversion explains why the apparent Y0/Y1/etc. labeling of the 4052 mux is reversed relative to the front-panel position if the schematic is read literally.

Strict implementation should expose semantic states:

```text
HPF_0_BASS_BOOST
HPF_1_FLAT
HPF_2_HIGH_PASS
HPF_3_HIGH_PASS_HIGHER
```

and keep the original latch-bit mapping inside the hardware-compatibility/control layer rather than leaking inverted bit numbering into DSP code.

## 2. One global 4052-selected network

Circuit analysis of the Roland jack-board schematic confirms that the 106 uses one 4052-family analogue mux to route the mono voice sum through one of four fixed paths.

The switch is not a voltage-controlled cutoff; it is a discrete analogue-routing selector.

The four modes are therefore four **different transfer networks**, not one filter whose cutoff parameter changes continuously.

## 3. HPF position 1 — straight/flat path

Position `1` is the actual nominal straight-through response.

Strict model target:

```text
H1(s) ~= unity through the selector/buffer path
```

Real hardware may still contain:

- analogue-switch on resistance;
- coupling capacitors;
- op-amp bandwidth/headroom;
- global VCA loading after the buffer.

But there is no intentional high-pass or bass-boost response in this selector state.

This is a key distinction from treating panel position `0` as bypass.

## 4. HPF positions 2 and 3 — first-order high-pass paths

The two upper positions are passive RC high-pass branches selected by the mux.

Secondary engineering/cloning work gives useful calibration targets:

```text
HPF 2: ~6 dB/oct high-pass, fc ~225 Hz
HPF 3: ~6 dB/oct high-pass, fc ~700 Hz
```

These are **secondary calibration targets**, not yet a replacement for direct calculation from a clean Roland jack-board component read.

First strict reference form:

```text
H_hp(s) = s / (s + wc)
```

with independently configurable `wc2` and `wc3`.

Do not use a resonant/biquad HPF unless the original passive network or measurement demonstrates a second pole.

## 5. HPF position 0 — bass boost, not bypass

Position `0` is the most distinctive 106 state.

The Roland circuit routes both:

1. a straight/unfiltered contribution; and
2. a filtered bass-emphasis contribution

into the following summing/buffer stage.

Detailed circuit interpretation of the original schematic identifies two bass-emphasis stages in the filtered branch:

### Stage 1

Elements around:

```text
C8
C9
R22
```

form a one-pole / one-zero shelving-type section.

### Stage 2

Elements around:

```text
R18
R19
R20
C6
IC4B
```

form an active pole/zero section with similar low-frequency emphasis behavior.

The filtered branch is then combined with the straight branch through the network around `R24` / `R25` into IC4A.

Overall position-0 behavior is therefore better represented as a **two-pole/two-zero bass-emphasis branch summed with dry**, not as:

- cutoff = 0 Hz;
- a one-pole low-pass;
- or a generic single low-shelf EQ.

### Calibration target

Secondary clone engineering reports approximately:

```text
~+3 dB low-frequency shelf behavior centered around ~70 Hz
```

This is useful as an A/B target, but the reference topology should retain two poles/two zeros so later exact Roland component values can be dropped in without redesigning the model.

Provisional form:

```text
Hboost(s) = Gdry + Gf * ((s+z1)(s+z2))/((s+p1)(s+p2))
```

where `Gdry`, `Gf`, `p1,p2,z1,z2` are derived from the real circuit once the component map is unambiguous.

## 6. Why the JUNO-60 HPF/output cannot be silently reused

The 106 inserts IC4A as a buffer between the selected HPF network and the global uPC1252 VCA.

The JUNO-60 does not provide the same buffering arrangement. Its passive HPF network is therefore loaded more directly by the following VCA, and that loading can vary with VCA gain/control state, producing additional high-frequency rolloff.

Strict consequences:

- `MODEL_106` uses the buffered 106 topology;
- do **not** add a generic 'Juno warmth' high-frequency loss copied from 60;
- a 60-style load-dependent output response may later become an explicit DEEP/60 option.

This difference is likely one reason generic emulations that share one HPF/output model for 60 and 106 miss part of their tonal distinction.

## 7. Global VCA LEVEL firmware law

**PRIMARY/FIRMWARE-CONFIRMED**

VCA LEVEL is received as a 7-bit patch parameter and stored with the same 14-bit conversion used by SUB, NOISE, CUTOFF and RESONANCE:

```text
vca_level_code = raw7 << 7
```

so the internal target range is:

```text
0x0000 .. 0x3F80
```

The module main loop outputs this value through the shared DAC/S&H system before the pitch/DCO section.

Roland's D/A timing/control diagram categorizes the conditioned `VCA LEVEL` control in the nominal:

```text
0 .. +5 V
```

class.

Therefore the correct architecture is:

```text
raw slider/patch value
 -> exact integer code law
 -> DAC/S&H
 -> 0..~+5 V conditioned control
 -> jack-board scaling/network
 -> uPC1252 control port
 -> global audio gain
```

## 8. uPC1252 is not a linear-gain OTA knob

The uPC1252HA2 is a dbx-family voltage-controlled amplifier originally intended for companding/noise-reduction systems.

Independent dbx documentation/patent literature describes this family as a decilinear/log-domain VCA whose gain changes exponentially with differential control voltage, with a characteristic control law expressed in millivolts per dB (later related devices around the same family use approximately 6 mV/dB control scaling).

Important limitation:

Roland does **not** feed the raw 0..+5 V DAC control directly as `5 V / 6 mV per dB`.

The jack-board resistor/control network necessarily converts/scales the conditioned VCA LEVEL voltage into the uPC1252's differential control-current/voltage domain.

Therefore strict Phase-0 rule:

- preserve a separate `GlobalVca1252` block;
- do not assume linear amplitude vs `raw7`;
- do not assume 6 mV/dB directly from the module DAC;
- leave the Roland jack-board scale/offset as explicit constants to derive from schematic/measurement.

## 9. VCA LEVEL is intended to change patch loudness and can distort

The original owner's manual describes VCA LEVEL as a patch-level matching control and explicitly warns that setting it too high may produce distortion without indicating a fault.

This is highly relevant to the engine.

It means at least one of the following can become nonlinear/overloaded as level rises:

- IC4A/output buffer;
- uPC1252 audio path;
- post-VCA coupling/gain stage;
- chorus input stage;
- combinations of the above.

Strict model must therefore preserve headroom through this block rather than implementing VCA LEVEL as an arbitrary final floating-point multiplier with infinite clean headroom.

## 10. Reference block structure

```text
Hpf106
  mode

  process(x):
    switch mode:
      0: return bass_boost_topology(x)
      1: return flat_path(x)
      2: return hp1_225ish(x)
      3: return hp1_700ish(x)

Buffer106
  process(x)  // IC4A topology/headroom

GlobalVca1252
  control_code_14
  control_voltage_held
  gain_state

  process(x)
```

Keep HPF and global VCA separate even if an optimized FM-1 build later fuses arithmetic operations.

## 11. Hardware-voltage/headroom policy

Input to this block is the summed output of six calibrated voice VCAs.

Do not normalize by active voice count.

Six voices sounding together should naturally present a larger signal than one voice, subject to the real summing network and available headroom.

This is especially important for:

- resonant chords;
- SAW+PULSE+SUB combinations;
- high VCA LEVEL;
- chorus drive;
- Solo-Unison, where all six physical voices carry the same pitch.

The eventual model should be able to reproduce the manual's high-VCA-level distortion warning as an emergent headroom effect rather than a cosmetic waveshaper knob.

## 12. Provisional calibration targets

Until exact jack-board passives are fully transcribed, use these as explicit non-primary targets:

| Mode | Functional target | Confidence |
|---|---|---|
| HPF 0 | two-pole/two-zero dry+filtered bass boost, roughly +3 dB LF around ~70 Hz | topology strong; numeric target secondary |
| HPF 1 | nominal flat/straight path | strong |
| HPF 2 | first-order HP, ~225 Hz | secondary numeric |
| HPF 3 | first-order HP, ~700 Hz | secondary numeric |

These values belong in configurable reference constants, not hidden magic numbers in the final FM-1 DSP.

## 13. Exact component extraction still required

For full numerical circuit freeze, obtain an unambiguous node/component map for the JACK BOARD around:

```text
HPF 0:
  C8, C9, R22
  R18, R19, R20, C6, IC4B
  R24, R25, IC4A

HPF 2/3:
  the two selected RC branches and switch/on-resistance context

Global VCA:
  IC5 uPC1252
  VCA LEVEL control scaling resistors
  audio input/output resistors and coupling capacitors
  any symmetry/offset/headroom components
```

OCR fragments are **not** sufficient evidence if spatial connectivity is ambiguous.

A clean schematic crop, board tracing or direct measurement should be archived before these values are upgraded to `PRIMARY/SCHEMATIC-CONFIRMED`.

## 14. Required measurements / render tests

### HPF transfer sweeps

Inject a low-level broadband/swept sine into the mono post-voice-sum node with VCA LEVEL held at a reference gain.

Measure each mode:

- 10 Hz .. 30 kHz magnitude;
- phase;
- passband gain;
- -3 dB / shelf points;
- effect of 4052 on resistance/source impedance.

### VCA LEVEL sweep

At HPF 1:

- fixed 1 kHz sine;
- sweep panel/patch VCA LEVEL 0..127;
- record output RMS/Vp-p;
- convert to dB gain;
- fit gain vs DAC/control voltage;
- repeat at multiple input amplitudes to expose headroom/nonlinearity.

### High-level distortion

At VCA LEVEL near maximum:

- one voice vs six voices;
- SAW-only vs full source mix;
- chorus off vs chorus on;
- measure THD/spectral onset and identify which node clips first.

### 60/106 A/B

Once 106 is frozen, compare an explicit 60 output-loading model only as an A/B experiment. No 60 load-dependent rolloff is permitted in strict 106 defaults.

## 15. Freeze state

### Frozen

- one global HPF after six-voice sum;
- discrete four-path selector, not variable cutoff;
- mode 1 = straight/flat;
- modes 2/3 = first-order high-pass branches;
- mode 0 = dry + two-stage bass-emphasis branch, not bypass;
- IC4A buffers/sums the 106 HPF output before global VCA;
- 106 therefore must not inherit 60's load-dependent HPF/VCA rolloff;
- VCA LEVEL uses `raw7 << 7` firmware law and nominal 0..+5 V conditioned control;
- global VCA is uPC1252/dbx-family and needs explicit nonlinear gain/headroom modeling;
- VCA LEVEL remains post-HPF, pre-chorus;
- high VCA LEVEL may distort by design/available headroom.

### Open

- exact Roland HPF component values/connectivity in a machine-readable/archived crop;
- exact transfer constants for HPF 0/2/3;
- 4052 on-resistance contribution;
- IC4A gain/headroom/slew contribution;
- Roland uPC1252 control scale/offset/gain law;
- uPC1252 audio-path distortion/headroom in this exact circuit;
- downstream chorus-input loading and where high-VCA-level clipping actually begins.
