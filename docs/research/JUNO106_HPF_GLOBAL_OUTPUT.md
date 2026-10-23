# JUNO-106 HPF and global output/VCA topology

Status: Phase 0 reference specification for strict `MODEL_106`.

This block begins **after the six per-voice 80017A VCA outputs have been summed** and ends at the input to the chorus/output section.

## 0. Evidence classes

- **PRIMARY-106** — Roland owner/service documentation and original schematic.
- **SCHEMATIC-DERIVED** — direct mathematical consequence of original component values/topology.
- **SECONDARY-CIRCUIT-CHECK** — independent schematic analysis used to disambiguate or sanity-check.
- **OPEN** — retain a parameterized model and explicit verification test.

## 1. Physical order and why this block stays global

The front-panel graphics do not represent the physical signal order.

Reference path:

```text
6 x voice 80017A VCA outputs
        -> mono voice sum
        -> global HPF selector / boost network
        -> IC4A buffer
        -> global uPC1252H2 VCA (VCA LEVEL)
        -> chorus / output chain
```

Consequences:

- no HPF per voice;
- no patch VCA LEVEL inside each 80017A;
- no normalization by active voice count;
- six-voice chords and Solo-Unison naturally drive this common path harder.

## 2. HPF selector semantics

**PRIMARY-106**

Roland defines the front-panel states as:

```text
0 -> low-frequency boost
1 -> unprocessed / flat
2 -> high-pass
3 -> higher high-pass
```

The module firmware inverts/remaps the two physical control bits before driving the selector latch, explaining why literal 4052 Y-labels appear reversed against the front-panel numbering.

Strict code should use semantic states:

```text
HPF_0_BASS_BOOST
HPF_1_FLAT
HPF_2_225
HPF_3_720
```

and hide physical mux bit polarity inside the hardware/control layer.

This is a four-route analogue selector, not one continuously variable high-pass filter.

## 3. Position 1 — straight / flat branch

**PRIMARY-106 / SCHEMATIC-DERIVED**

Position 1 selects the nominal straight-through branch.

```text
H1(s) ~= 1
```

`flat` means no intentional HPF/boost branch. The real path still includes the analogue switch, IC4A buffer, global VCA and later output stages.

Do not implement HPF 1 as a bit-perfect bypass of the entire global output block.

## 4. Position 2 — first-order 225.75 Hz HPF

**SCHEMATIC-DERIVED**

The original branch uses:

```text
R = 47 kOhm
C = 15 nF
```

For the ideal first-order RC high-pass:

```text
fc = 1/(2*pi*R*C)
   = 225.7516923289 Hz
```

Freeze:

```text
HPF2_R_OHM       = 47000
HPF2_C_F         = 15e-9
HPF2_FC_IDEAL_HZ = 225.7516923289
```

Transfer before small source/switch-loading corrections:

```text
H2(s) = s*R*C / (1 + s*R*C)
```

Slope below the corner is 6 dB/octave.

The commonly quoted `225 Hz` figure is therefore not an arbitrary emulator tuning value; it falls directly out of the original RC pair.

## 5. Position 3 — first-order 720.48 Hz HPF

**SCHEMATIC-DERIVED**

The original branch uses:

```text
R = 47 kOhm
C = 4.7 nF
```

Ideal corner:

```text
fc = 1/(2*pi*R*C)
   = 720.4841244540 Hz
```

Freeze:

```text
HPF3_R_OHM       = 47000
HPF3_C_F         = 4.7e-9
HPF3_FC_IDEAL_HZ = 720.4841244540
```

Transfer:

```text
H3(s) = s*R*C / (1 + s*R*C)
```

Again the slope is 6 dB/octave.

This is a better strict reference than rounding the high setting to `700 Hz` before the circuit model is even built.

## 6. 4052 on-resistance and the simple HPF branches

The selector is a TC4052-family analogue mux with finite on-resistance.

For positions 2/3, switch resistance is small relative to 47 kOhm, so it only perturbs the ideal corners slightly.

Strict reference architecture should nevertheless retain:

```text
R_effective = 47k + sourceR + switchRon + loadingCorrection
```

rather than hard-coding a digital filter frequency detached from the actual circuit.

The final measured/reference corner may therefore differ slightly from 225.7517 / 720.4841 Hz while those values remain the **component-ideal anchors**.

## 7. Position 0 — active bass boost, not `fc = 0`

**PRIMARY-106 topology; numerical curve still OPEN**

Position 0 routes the signal through a distinct active network around IC4B and then into the IC4A summing/buffer stage.

Original-schematic analysis consistently identifies C8 = 0.01 uF as part of the low-frequency emphasis network.

The circuit combines:

1. a straight contribution; and
2. a frequency-shaped bass-emphasis contribution.

It is therefore incorrect to model position 0 as:

- bypass;
- HPF with cutoff 0 Hz;
- a simple gain boost;
- or a generic one-pole EQ chosen by ear.

Independent circuit analyses converge on a characteristic transition around roughly 65–70 Hz, but disagree enough on the exact shelf height/passband offset that Phase 0 deliberately does **not** freeze a dB value yet.

Published secondary interpretations range from a few dB around the transition to roughly +6 dB low-band gain with a small broadband offset once switch resistance is included.

Freeze only:

```text
HPF0.type = ACTIVE_DRY_PLUS_FILTERED_BASS_SHELF
HPF0.transition_region_hz ~= 65..70
HPF0.gain_curve = MEASURE_OR_DERIVE_FROM_FULL_NETLIST
```

Do not choose +3 dB, +6 dB or +10 dB as strict truth without closing the original node map.

## 8. Critical 106 vs 60 difference: IC4A isolation

The 106 inserts IC4A as a buffer between the selected HPF network and the global uPC1252H2 VCA.

The 60 does not have the same isolation. Its passive network is loaded more directly by the following global VCA, which can introduce gain-dependent high-frequency loss.

Strict consequences:

- `MODEL_106` must use the buffered topology;
- do not add generic Juno-60 output dulling to the 106;
- VCA LEVEL should not move the 106 HPF response through VCA input loading in the same way;
- a 60-style loading model belongs only in explicit `MODEL_60` / DEEP hybrid work.

This is a useful example of using 60 experience to explain the 106 without merging them.

## 9. Global VCA LEVEL control path

**PRIMARY/FIRMWARE-CONFIRMED**

VCA LEVEL arrives as a 7-bit patch parameter and uses the 14-bit storage conversion:

```text
vca_level_code = raw7 << 7
```

Range:

```text
0x0000 .. 0x3F80
```

Roland's D/A diagram places VCA LEVEL in the nominal conditioned:

```text
0 .. +5 V
```

control family.

The physical chain is therefore:

```text
raw patch value
 -> exact integer code
 -> shared DAC/S&H
 -> conditioned ~0..+5 V control
 -> jack-board control network
 -> uPC1252 gain-control pins
 -> global audio gain
```

Do not collapse this to `gain = raw/127`.

## 10. What the uPC1252H2 tells us

Component/service documentation identifies uPC1252H2 as a dbx-family VCA with:

- current-mode audio input/output;
- internal current amplifier;
- gain cell;
- bias-current circuitry;
- multiple gain-control pins.

So VCA LEVEL is not merely a linear post-DSP multiplier.

Important caution: dbx-family VCAs use log/decilinear control behavior, but the Roland `0..+5 V` module-board control is **not** fed directly as a simple `mV/dB` voltage to the chip. The jack-board network performs scale/offset/current conversion.

Strict Phase-0 rule:

```text
GlobalVca1252 {
    code14
    heldControlVoltage
    externalControlNetwork
    gainState
    nonlinearState
}
```

Do not guess either a linear taper or an arbitrary exponential taper and promote it to authenticity.

## 11. High VCA LEVEL distortion is part of the instrument

The original owner's manual explicitly says that setting VCA LEVEL too high may cause distortion and that this is normal rather than a fault.

Therefore available headroom in this common path is musically relevant.

Possible contributors include:

- IC4A / summing-buffer headroom;
- uPC1252 current/gain-cell headroom;
- post-VCA conversion/coupling stage;
- chorus input stage;
- combined loading of the above.

Strict engine policy:

- preserve six-voice summed voltage;
- apply HPF/shelf before the global VCA;
- allow high VCA LEVEL to reach the modeled nonlinear region;
- no invisible per-voice normalization;
- no hidden pre-model limiter.

A platform-protection limiter, if required on FM-1, belongs **after** the calibrated reference path and must be bypassable in reference renders.

## 12. Numerical uPC1252 gain law remains open

Required strict transfer:

```text
gain = upc1252Gain(controlVoltage,
                   externalNetwork,
                   bias,
                   signalLevel)
```

Open numerical items:

- exact 106 IIN/IOUT conversion resistors;
- exact gain-control pin resistor network;
- control offset/scale from module control voltage;
- gain dB vs VCA LEVEL code;
- unity/reference setting;
- maximum clean gain;
- positive/negative clipping knees;
- THD vs level and gain;
- control feedthrough/offset if audible.

These should be solved from the original jack-board netlist and/or measurement, not by taste.

## 13. Desktop reference implementation

Keep the blocks separate and observable:

```text
voiceSum
 -> HpfSelector106
 -> BufferIC4A106
 -> GlobalVca1252
 -> Chorus106
```

Recommended diagnostics:

```text
HPF_PHYSICAL
HPF_IDEAL_BRANCH

VCA_UNITY
VCA_STATIC_MEASURED
VCA_FULL_NONLINEAR
```

For HPF 2/3, derive digital coefficients from the physical RC values at the renderer sample rate rather than storing rounded frequencies as magic numbers.

For HPF 0, wait for the full active-node model before freezing coefficients.

## 14. Verification plan

### HPF sweep

On a calibrated original 106, chorus off:

1. reference all four modes to HPF 1;
2. capture complex transfer 10 Hz..30 kHz;
3. verify first-order phase/magnitude of positions 2/3;
4. fit actual corners including switch/source/loading effects;
5. derive the complete mode-0 shelf curve and passband offset;
6. repeat at low/mid/high VCA LEVEL to verify IC4A isolation.

Expected anchors:

```text
HPF1: flat reference
HPF2: ideal component corner 225.7516923 Hz
HPF3: ideal component corner 720.4841245 Hz
HPF0: active bass shelf, transition roughly 65..70 Hz, exact gain TBD
```

### VCA LEVEL sweep

At HPF 1:

- fixed 1 kHz source;
- VCA LEVEL raw 0..127;
- record output gain in dB;
- measure corresponding control voltage if safely possible;
- repeat at multiple input levels;
- determine gain taper independently from overload behavior.

### Headroom

Compare:

- one voice vs six voices;
- SAW-only vs maximum practical source mix;
- low vs high VCA LEVEL;
- chorus OFF vs ON.

Locate the first clipping/nonlinear node rather than fitting all distortion to one arbitrary waveshaper.

## 15. FM-1 deployment implication

This is a global block, so high fidelity is cheap compared with six nonlinear filters.

Likely deployment:

```text
voiceSum
 -> discrete HPF route
 -> cheap IC4A buffer/headroom model
 -> LUT/polynomial uPC1252 gain + nonlinearity
 -> Chorus106
```

Positions 2/3 each need only one filter state. Position 0 should remain a small circuit-derived low-order network. Global VCA nonlinearity is computed once, not six times.

## 16. Freeze state

### Frozen

- one global HPF after the six-voice sum;
- four discrete analogue routes;
- position 1 = straight/flat;
- position 2 = 47 kOhm / 15 nF first-order HPF, ideal `225.7516923289 Hz`;
- position 3 = 47 kOhm / 4.7 nF first-order HPF, ideal `720.4841244540 Hz`;
- position 0 = distinct dry+filtered active bass shelf around a ~65..70 Hz transition, exact gain still open;
- finite 4052 on-resistance remains part of the physical reference;
- IC4A isolates the 106 HPF from uPC1252 loading, unlike the 60;
- VCA LEVEL uses `raw7<<7` and the nominal 0..+5 V conditioned-control family;
- global VCA is uPC1252H2 after HPF and before chorus;
- high VCA LEVEL can intentionally expose global-path distortion;
- no active-voice normalization in strict mode.

### Open

- exact HPF0 node-by-node transfer and shelf gain;
- actual TC4052 on-resistance at the operating point;
- IC4A exact gain/headroom/slew contribution;
- complete uPC1252 external network;
- static VCA LEVEL -> gain/dB law;
- global VCA/output headroom and nonlinear curve;
- exact point at which the owner's-manual high-level distortion begins;
- downstream loading into chorus/output stage.
