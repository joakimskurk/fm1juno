# JUNO-106 MC5534 ramp, reset and amplitude-compensation model

Status: Phase 0 reference specification. JUNO-106 is authoritative; JUNO-60 is used only as a topology/measurement aid where the 106 documentation is incomplete.

## 0. Evidence policy

This document intentionally separates four levels of confidence.

- **PRIMARY-CONFIRMED** — stated directly by Roland service documentation or literal module firmware behavior.
- **SCHEMATIC/CROSS-CHECK** — visible in the 106 schematic and independently corroborated, but not always available as machine-readable service text.
- **DERIVED** — follows mathematically from confirmed topology/values.
- **OPEN/HYPOTHESIS** — plausible model detail that still needs a direct 106 measurement or cleaner primary-component read.

Do not silently promote a JUNO-60 passive value to strict 106 truth.

## 1. What the MC5534 block actually is

**PRIMARY-CONFIRMED**

The JUNO-106 does not generate an audio saw by incrementing a digital phase accumulator and converting samples to analogue.

For each physical voice, the system contains:

1. a digitally timed DCO rectangular signal from an 8253 counter;
2. a sampled analogue DCO-CV amplitude-control signal from the slave CPU/DAC/S&H system;
3. a range-selected resistor path controlled by the 4'/8'/16' RANGE code;
4. the MC5534-family wave-generator block;
5. a ramp capacitor (`C54` in the CH1 service description);
6. an edge-driven discharge/reset transistor;
7. a comparator path that derives PWM from the physical ramp;
8. a divide-by-two path that derives SUB from the DCO timing signal.

Roland explicitly describes the DCO CV as containing key, LFO, Bender, Portamento and Tune information, but not RANGE. RANGE is applied afterwards by the 4052 switch selecting one of R85/R86/R87.

The purpose of DCO CV + range scaling is to keep saw/pulse amplitude approximately constant, with Roland giving approximately **12 Vp-p** as the design target.

## 2. Range resistor network

### 2.1 Functional mapping

**PRIMARY-CONFIRMED**

Roland states that for CH1 the DCO CV charges `C54` through `R85` when RANGE = 16'. R85/R86/R87 are selected according to RANGE.

**SCHEMATIC/CROSS-CHECK**

Independent readings/analyses of the 106 schematic consistently identify the resistor set as approximately:

```text
100 kOhm
200 kOhm
399 kOhm
```

The service text plus first-principles slope requirement makes the mapping unambiguous:

```text
16' -> R85 ~= 399 kOhm
 8' -> R86 ~= 200 kOhm
 4' -> R87 ~= 100 kOhm
```

Reason: a 16' period is twice as long as 8', so for the same DCO CV and target ramp excursion it needs roughly half the charging current, hence roughly twice the resistance. 4' requires the opposite.

Therefore the network is very close to a 4:2:1 resistance ratio, or 1:2:4 conductance ratio.

### 2.2 Important terminology

Do **not** describe R85/R86/R87 as a conventional simultaneous 3-bit audio DAC in the clean model.

The main DCO CV is already produced by the shared DAC/S&H system. The 4052 selects **one** of the three range resistors. Functionally this is octave/range-dependent slope scaling of the continuous sampled DCO CV.

## 3. Ramp capacitor

**SCHEMATIC/CROSS-CHECK, pending final primary read**

High-quality circuit analyses use approximately:

```text
Cint ~= 1 nF
```

for the 106 ramp integrator. This is consistent with the visible 106 topology and with the required operating range.

Until a cleaner primary component-value read or direct board measurement is stored in the evidence log, keep `Cint_106` configurable rather than compile-time sacred.

Recommended reference default for experiments:

```text
Cint_106 = 1.0 nF
```

with the evidence tag `SCHEMATIC/CROSS-CHECK`, not `PRIMARY-CONFIRMED`.

## 4. Ideal ramp law

For the inverting integrator approximation, ignoring reset time and op-amp limits:

```text
dV_ramp/dt = -V_dco_cv / (R_range * Cint)
```

For one DCO period `T`:

```text
DeltaV_ideal = -V_dco_cv * T / (R_range * Cint)
```

or using `T = 1/f`:

```text
abs(DeltaV_ideal) = abs(V_dco_cv) / (R_range * Cint * f)
```

To maintain approximately constant `DeltaV`:

```text
abs(V_dco_cv) proportional to f * R_range
```

This directly explains both pieces of the 106 design:

- DCO-CV magnitude rises approximately exponentially with note frequency;
- range resistance halves for each octave increase in RANGE.

The digital ROM DCO-CV note table and the analogue range resistor network are therefore two halves of one amplitude-compensation system.

## 5. Why the 399k/200k/100k sequence is so convincing

Using the provisional `Cint = 1 nF`, the range RC constants are approximately:

```text
16': 399 us
 8': 200 us
 4': 100 us
```

Each range doubles oscillator frequency while approximately halving R, so `R*T^-1` remains in the same operating family.

For example, if an 8' condition uses `R=200k`, then the corresponding 16' condition uses about half the frequency and about twice the resistance. Their `R*f` products are essentially equal:

```text
399k * (f/2) ~= 199.5k * f
200k * f     = 200.0k * f
100k * (2f)  = 200.0k * f
```

The slight `399k` rather than ideal `400k` difference is small enough to be plausible resistor-series/trim behavior while intentionally not creating perfect mathematical equality.

Strict mode should preserve the real resistor ratio rather than replacing it with an exact power-of-two abstraction once the primary read is fully confirmed.

## 6. DCO-CV source and zero-offset calibration

**PRIMARY-CONFIRMED**

Roland provides a DCO-CV OFFSET calibration:

- press MIDI CH in test procedure so the D/A converter requests 0 V;
- measure TP3;
- trim VR33 to exactly 0 V (1 mV-resolution meter specified).

This matters to the DSP model because the hardware explicitly calibrates the zero point of the shared DCO-CV path. A constant unknown offset should not be baked into the nominal factory-new model.

Recommended architecture:

```text
DcoCv106
  ideal_dac_code
  calibrated_zero_offset
  optional_s_and_h_droop
  optional_voice_path_offset/tolerance
```

Strict factory-reference defaults should use zero calibrated offset, while DEEP/AGE may later expose realistic residual calibration error.

## 7. Ramp reset

**PRIMARY-CONFIRMED**

Roland states that the ramp capacitor is discharged by transistor `E` on the **positive-going edges** of the DCO signal.

This gives the source oscillator two very different timing actions:

1. 8253 edge timing determines when reset is initiated;
2. analogue transistor/capacitor behavior determines how the ramp actually returns toward its reset level.

This is why `Timer106` and `Wave5534` should remain separate engine blocks.

### 7.1 Finite reset is required in the reference architecture

A mathematically instantaneous saw reset is allowed as a diagnostic baseline, but not as the final physical reference assumption.

The implementation must have room for:

```text
reset_on_time
reset_resistance_or_tau
reset_residual_voltage
transistor_threshold
transistor_saturation/headroom
```

### 7.2 Values not yet frozen

Secondary reconstruction work commonly uses approximately:

```text
clock differentiator: 10 kOhm * 270 pF
reset discharge:       2.2 kOhm * 1 nF
```

and estimates a reset-transistor on-time of roughly 5 us, leaving a small finite residual before normal charging resumes.

However the same source explicitly notes that several non-integrator passive values were borrowed/inferred from JUNO-60-family documentation.

Therefore these numbers are **not** strict-106 constants yet.

Use them only in a `RESET_HYPOTHESIS_A` desktop experiment until direct 106 schematic/measurement evidence closes the item.

## 8. Candidate physical ramp state machine

```text
Wave5534Ramp106
  v_cap
  dco_cv_held
  range_R
  reset_active
  reset_elapsed

on_positive_dco_edge():
  reset_active = true
  reset_elapsed = 0

advance(dt):
  if reset_active:
      v_cap = physical_discharge(v_cap, dco_cv_held, dt)
      reset_elapsed += dt
      if reset_pulse_finished(reset_elapsed):
          reset_active = false
  else:
      v_cap += (-dco_cv_held / (range_R * Cint)) * dt

saw_output = buffer_and_limit(v_cap)
```

Important: the DCO CV is a held analogue control value; it is not recalculated at every audio sample. It changes when the original DAC/S&H schedule services that voice.

## 9. Non-ideal amplitude is desirable

Roland says DCO CV keeps amplitude **nearly** constant, not perfectly constant. Independent engineering analysis suggests level error across the operating range can be on the order of roughly a volt.

Strict model implication:

- do not normalize every oscillator cycle to exactly 12 Vp-p;
- use the actual DCO-CV/range/ramp calculation;
- let resistor ratios, DAC quantization, reset dead-time and headroom naturally create small amplitude variation;
- only use the ~12 Vp-p figure as a calibration/validation region.

This is likely more authentic than adding arbitrary random gain drift after an ideal saw.

## 10. Pulse/PWM must share the ramp

**PRIMARY-CONFIRMED**

PWM is generated by comparing the physical saw/ramp with PWM CV.

Roland anchors:

```text
PWM CV ~ +6.0 V -> ~50% duty
PWM CV ~ +0.6 V -> ~95% duty
PWM OFF ~ -0.8 V -> comparator held high / pulse disabled
```

Factory test tolerances:

```text
50% setting: 48..52%
high PWM:    93..97%
```

Therefore strict pulse must not be a separate ideal digital square oscillator with a UI duty value.

Reference shape:

```text
pulse = comparator(saw_voltage, pwm_cv_held, comparator_nonidealities)
```

Once the saw ramp/reset is correct, pulse inherits its cycle timing, ramp curvature/reset artifacts and amplitude-control behavior automatically.

## 11. SUB remains a separate divide-by-two path

**PRIMARY-CONFIRMED**

SUB is generated as a divided-by-two rectangular derived from DCO timing. Its output level changes with the SUB LEVEL control path.

Do not derive SUB by simply frequency-dividing the rendered saw samples; maintain a timing-domain divide-by-two state so phase is tied to DCO edges.

## 12. Anti-aliasing strategy

The physical model and anti-aliasing layer are separate concerns.

Desktop reference experiments should compare:

1. high-rate/oversampled physical ramp used as truth rendering;
2. event-aware minBLEP/polyBLEP correction applied at the actual finite reset transition;
3. hybrid: analytical linear ramp between reset events + oversampled/reset kernel only during the short reset region.

The third option is promising for FM-1 because almost the entire cycle is a cheap linear integration; the computationally expensive part can be localized around DCO reset and comparator crossings.

Do not replace the physical source model with a generic free-running PolyBLEP oscillator merely because that is cheaper.

## 13. FM-1 implementation implication

A six-voice physical DCO need not be expensive if represented event-first.

Per audio sample, most voices require only:

- integrate one linear ramp using a held slope;
- compare against held PWM threshold if pulse is enabled;
- update cheap SUB output state only on DCO edge events.

Per DCO edge:

- trigger/reset finite-reset state;
- emit anti-aliasing correction;
- toggle/subdivide timing state as required.

At control slots:

- update timer pending count;
- update held DCO CV;
- update PWM CV;
- update SUB level.

This fits the already-frozen ordered-control architecture naturally.

## 14. Validation vectors

### 14.1 Amplitude vs note and range

Render/measure representative low, middle and high keys in each RANGE.

For each:

- log 8.8 pitch;
- timer count and effective frequency;
- DCO-CV code/held value;
- selected R;
- predicted ideal ramp excursion;
- rendered Vp-p;
- deviation from ~12 Vp-p target.

The error should be structured/quantized like the hardware control system, not randomized.

### 14.2 Range-boundary equivalence

Compare equivalent musical pitches produced with adjacent RANGE choices where keyboard/transposition permits.

Expected:

- same/near pitch timing;
- different timer/range paths;
- closely similar but not mathematically forced-identical waveform amplitude.

### 14.3 Reset residual

At low/mid/high frequency:

- measure voltage immediately before edge;
- minimum/reset level;
- time until normal linear ramp resumes;
- residual as fraction of pre-reset excursion.

### 14.4 PWM

At C4 and multiple ranges:

- calibrate 50% target against +6 V equivalent;
- verify 48..52%;
- verify high PWM target 93..97%;
- verify OFF suppresses the rectangular source.

### 14.5 Unison reset staggering

Use the real 31.25-kbit/s six-voice reset schedule from `JUNO106_SOLO_UNISON_TIMING.md` and confirm that finite analogue reset does not accidentally become one shared simultaneous phase reset in the DSP model.

## 15. What JUNO-60 contributes here

Allowed:

- confirming the same basic edge-reset integrator/comparator design family;
- suggesting what to probe/measure on real hardware;
- providing hypotheses for hidden/unclear reset-network passive values;
- future DEEP mode alternative behavior.

Not allowed in strict 106:

- importing the 60's variable master-clock pitch modulation;
- assuming the 60's complete passive network is numerically identical;
- forcing a 60-derived reset time constant without 106 corroboration.

## 16. Freeze state

### Frozen enough for Phase-1 source-model architecture

- edge-timed analogue ramp generator, not ideal digital saw;
- DCO CV is held analogue slope control;
- RANGE selects one of three slope resistors;
- 16' uses the highest resistance and 4' the lowest;
- working resistor set ~= 399k / 200k / 100k, pending final primary designator-value read;
- ramp target is approximately 12 Vp-p, not exact per-cycle normalization;
- positive DCO edge triggers discharge/reset;
- PWM is comparator-derived from this same ramp;
- SUB is timing-domain divide-by-two;
- reset must be architecturally finite/configurable.

### Still open before strict numerical freeze

- primary-confirm exact C54 capacitance;
- primary-confirm exact R85/R86/R87 printed values from a clean schematic/board read;
- exact MC5534 internal/external reset-network passive values;
- reset transistor conduction duration and residual voltage on real 106;
- MC5534 buffer/comparator saturation, slew and asymmetric recovery;
- exact mapping from 12-bit DCO-CV DAC/S&H code to voltage at each voice input under load;
- per-voice S/H droop/output impedance and 4052 on-resistance contribution;
- measured amplitude-error curve across all ranges/keyboard.
