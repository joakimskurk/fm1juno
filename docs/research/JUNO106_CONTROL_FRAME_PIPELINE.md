# JUNO-106 ordered module-control pipeline

Status: Phase 0 timing/causality map for strict `MODEL_106`.

This document records the **ordering** of control work in the module CPU main loop. The JUNO-106 control system must not be treated as one atomic ~236 Hz parameter update: different state is read, calculated and written at different points inside the loop, while serial and ADC interrupts can occur asynchronously.

Source basis: literal control flow in the public `ErroneousBosh/j106roms` module-CPU disassembly (`ic29.txt`), cross-checked against Roland's service timing description.

## 1. Why this matters

A simple VA implementation would commonly do this once per control tick:

```text
read controls
update LFO
update envelopes
calculate pitch/filter
write all targets simultaneously
```

The 106 does not.

Important one-pass dependencies include:

- pitch and VCF modulation use the LFO accumulator value that existed **before** the LFO is advanced near the end of the current loop;
- Tune, Bender, Portamento setting and physical LFO-trigger/modulation ADC-derived state are refreshed **after** the current loop's DCO/VCF calculations, so they naturally feed a later pass;
- each envelope is advanced while its voice's VCA/VCF targets are being serviced, so the newly calculated envelope value can affect that voice in the current pass;
- PWM CV is output early in the six-voice service sequence, while the next PWM value is derived from the LFO near the end of the loop;
- six DCO timer writes and reset consumptions happen sequentially rather than at one shared timestamp.

The desktop reference engine should preserve these causal relationships even if we later prove that some sub-frame offsets can be collapsed on FM-1 without an audible penalty.

## 2. High-level main-loop order

The normal module loop begins around `$02EC` and returns from `$07B5`.

Working order:

1. **Latch global switch outputs**
   - chorus / HPF control latch written.
2. **Rebuild `voiceRun`**
   - sustain off: copy current gates;
   - sustain on: retain running bits and OR current gates.
3. **Output global VCF Resonance CV**.
4. **Update LFO Delay/onset envelope and calculate signed LFO target magnitudes**
   - uses the current stored LFO accumulator from the previous LFO-advance stage;
   - produces DCO-pitch and VCF-LFO modulation magnitudes for this pass.
5. **Output master VCA control**.
6. **Build global 8.8 pitch offset**
   - fixed base;
   - Tune;
   - signed LFO pitch amount;
   - signed Bender amount.
7. **Advance six Portamento pitch coordinates**
   - per physical voice;
   - current stored Portamento coefficient.
8. **Output Sub-Oscillator level CV**.
9. **Service six DCOs sequentially**
   - combine per-voice Portamento pitch + global pitch offset;
   - convert 8.8 pitch through divisor interpolation;
   - consume that voice's pending `resetVoiceBits` state if present;
   - either reset/control-word + load count, or ordinary LSB/MSB reload;
   - interpolate and output DCO amplitude-compensation CV for that voice.
10. **Build global base VCF control**
    - Cutoff;
    - signed LFO;
    - signed Bender.
11. **Service six voice control paths sequentially**
    - advance that physical voice's digital envelope;
    - output PWM CV at the designated early slot;
    - output VCA ENV/GATE CV according to the multiplex schedule;
    - add/subtract ENV to VCF;
    - calculate signed key follow from the physical voice's current 8.8 pitch;
    - output that voice's VCF CV.
12. **ADC/control-processing phase**
    - ADC bank alternates;
    - process Bender and Bender sensitivities;
    - process Portamento setting;
    - process physical LFO trigger/mod depth;
    - process Tune;
    - apply hysteresis/de-click state.
13. **Complete final deferred voice-VCA service slot**.
14. **Advance global LFO accumulator**
    - integer add/subtract;
    - endpoint clamp;
    - direction/polarity flag increment.
15. **Calculate/store next PWM signal from the newly advanced LFO value**.
16. **Output Noise level CV**.
17. Jump back to step 1.

## 3. LFO is effectively pipelined by one control pass

The DCO/VCF modulation calculation near the start of a pass reads the previously stored LFO accumulator.

Only near the end of that same pass does the firmware advance the LFO accumulator.

Therefore conceptually:

```text
pass N early:  targets_N = scale(lfo_state_N)
...
pass N late:   lfo_state_Nplus1 = advance(lfo_state_N)
pass N+1 early: targets_Nplus1 = scale(lfo_state_Nplus1)
```

Strict model consequence:

Do not advance a triangle LFO and immediately use that new value for the same nominal control tick if the goal is ROM-domain fidelity.

This also makes the integer endpoint clamp/rate quantization easier to reproduce exactly.

## 4. Tune/Bender/Portamento-control updates have natural pass latency

The current DCO pitch is built before the ADC/control-processing section.

Physical control state refreshed later in the pass therefore affects subsequent pitch calculations rather than retroactively changing counts that were already written.

Relevant examples:

- Tune ADC -> `tuningAmt` after current DCO service;
- physical Bender / sensitivity -> stored bend contribution after current VCF/DCO service;
- Portamento knob -> `portaCoeff` after current per-voice Portamento step;
- physical LFO trigger/mod control -> next-pass LFO depth contribution.

MIDI/serial control events can arrive asynchronously through the serial ISR and therefore have somewhat different phase relative to the main loop.

Reference tests must distinguish panel-ADC changes from serial/MIDI changes rather than assuming one universal parameter latency.

## 5. Envelope update is interleaved with voice output

The firmware does not first update all six envelopes and then write all six VCA/VCF CVs.

Instead the six-voice loop advances one voice's envelope and services that voice's output/control path before moving to the next physical voice.

Consequences:

- voice control S/H updates are separated in time inside the control frame;
- the six envelope transitions are not perfectly simultaneous at analogue S/H pins;
- a serial ISR occurring during the loop can change state before later voices are serviced;
- an authentic unison transient is therefore a six-voice distributed event, not a single control sample broadcast six times.

## 6. DCO reset requests are consumed in physical-voice order

Serial Voice On processing only sets a bit in `resetVoiceBits`.

The actual 8253 reset/control-word write occurs when the main DCO loop reaches that physical voice.

For each physical voice slot:

```text
if reset_pending[voice]:
    clear reset_pending[voice]
    write counter-specific Mode-3 control word
    write new count LSB
    write new count MSB
else:
    write new count LSB
    write new count MSB
```

A reset request arriving after that voice's slot for the current pass remains pending until the next pass.

This is especially relevant to Solo-Unison, whose six Voice On messages occupy ~4.16 ms on the serial link.

## 7. Ordinary count reload semantics

The original 8253 family documents Mode 3 as closely related to Mode 2, with the current square-wave half-cycle completing before a newly written count takes over. Later compatible 82C54 documentation states this explicitly: absent a new Gate trigger, a newly written count is loaded at the end of the current half-cycle.

Strict timer state therefore needs at least:

```text
active_count
pending_count
half_cycle_state
output_state
```

An ordinary DCO control update changes `pending_count`, not the already-running half-cycle.

A control-word reset is a distinct operation that re-establishes Mode 3/output state before the new count is loaded.

## 8. Proposed reference scheduler

Do **not** emulate the complete uPD7811 instruction stream inside the eventual FM-1 instrument.

Instead, the desktop fidelity engine should use an ordered control scheduler with explicit phase slots.

Provisional shape:

```text
ControlFrame106
  begin_frame()
  update_voice_run()
  service_resonance()
  compute_lfo_targets_from_previous_state()
  service_master_vca()
  build_global_pitch()
  step_portamento_all()
  service_sub_level()
  for voice 0..5:
      service_dco_slot(voice)
  build_vcf_base()
  for voice 0..5:
      update_env_and_service_voice_cv_slot(voice)
  process_adc_bank_and_performance_controls()
  finish_deferred_vca_slot()
  advance_lfo()
  build_next_pwm()
  service_noise()
  end_frame()
```

Initially the slot timing may be expressed as ordered events within one nominal frame rather than exact CPU cycles.

If measurement shows that sub-frame timing is audible, add calibrated fractional-frame timestamps. If it is not audible, the FM-1 build may preserve ordering/one-frame latency with fewer actual scheduled events.

## 9. Double-buffered state recommendation

Several controls benefit from explicit old/new state separation.

Example:

```text
lfo_current      // read by early-pass pitch/VCF calculation
lfo_next         // generated by late-pass LFO step

panel_pitch_ctl_current
panel_pitch_ctl_next

pwm_signal_current
pwm_signal_next
```

At the correct control boundary, swap/promote only the states that the original firmware would have made visible.

This is safer than scattering compensating one-frame delays through later DSP code.

## 10. Validation vectors

### LFO pipeline

- choose a coefficient whose integer LFO step is obvious;
- verify current-pass pitch uses pre-advance `lfoVal`;
- verify next pass sees the newly advanced value;
- repeat across an endpoint clamp and polarity change.

### Panel Tune

- inject a Tune ADC change just before vs just after the ADC section;
- verify no retroactive change to DCO counts already written in that pass;
- verify next eligible pass reflects it.

### Portamento

- change Portamento knob during a glide;
- verify the current pass uses the previous `portaCoeff` and later passes use the newly decoded coefficient.

### Note On/reset

- inject Voice On before physical voice N's DCO slot: reset consumed this pass;
- inject it after the slot: reset consumed next pass;
- repeat with ordinary same-note reattack and confirm no reset bit.

### Unison

- queue the real 13-byte unison Note On sequence at 31.25 kbit/s;
- sweep burst start phase across an entire module control loop;
- record which voice reset slots land in current vs next pass;
- compare transient/phase behavior with an artificially simultaneous six-voice reset.

### ENV/VCA/VCF

- trigger all six voices together;
- verify per-voice envelope/CV service occurs in physical order rather than as an atomic six-value vector.

## 11. FM-1 simplification gate

The desktop reference should preserve ordered causality first.

Only after A/B rendering/profiling should FM-1 collapse any of the following:

- sub-frame per-voice S/H timing;
- serial-burst-induced DCO reset staggering;
- panel-vs-MIDI one-pass latency distinctions;
- exact placement of PWM/noise DAC updates.

A simplification is acceptable only if it is:

1. measured against the ordered reference;
2. inaudible or below a defined error threshold in relevant patches;
3. documented as an FM-1 optimization rather than silently redefined as authentic hardware behavior.

## 12. Remaining work

- calibrate the exact nominal main-loop period and quantify interrupt-induced jitter;
- map each DAC/S&H write to an approximate fractional position inside the frame;
- determine whether the deferred voice-6 VCA slot is simply a scheduling artifact of the ADC section or creates measurable channel skew;
- quantify serial ISR cycle cost;
- measure whether real 106 unison transients reveal the predicted reset staggering;
- decide the minimum timing model required on FM-1 after reference A/B tests.
