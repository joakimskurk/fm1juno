# JUNO-106 control scheduler / DAC-S&H reference model

Status: **`CONTROL_TIMING_PHASE0_ARCHITECTURE_FROZEN`**.

This document freezes the observable module-CPU scheduling architecture that sits between the recovered digital control laws and the analogue/S&H blocks. Exact microsecond slot offsets, analogue settling and workload-dependent jitter remain centralized calibration data.

## 1. Core rule: the JUNO-106 is not one 236 Hz atomic control update

Roland documents an approximately 4.2 ms multiplexed control-service cycle. The module ROM shows that the work inside a cycle is ordered and state-dependent.

Strict `MODEL_106` therefore uses an **ordered event scheduler**:

```text
CPU/control state
 -> ordered calculations
 -> ordered DAC/MUX writes
 -> physical S/H updates
 -> held analogue controls between updates
```

Forbidden strict shortcut:

```text
once_per_frame {
  calculate_everything();
  publish_all_controls_simultaneously();
}
```

The average frame rate can be near 236 Hz while two physical controls inside the same frame still update at different times and from different causal state.

## 2. Physical DAC boundary is 12-bit

The ROM routine at `$082F` inhibits the analogue MUXes, selects the requested MUX channel, then writes the DAC value through ports B/C.

The disassembly explicitly shows that the lowest two bits are not used by the DAC path.

For the common 14-bit control domain:

```text
code14 = 0x0000 .. 0x3fff
physical_dac12 = code14 >> 2
```

This quantization belongs **before** the S/H/analogue reconstruction.

Reference API:

```text
Dac106::write(code14):
    dac12 = code14 >> 2
    dac_bus = dac12
```

Do not carry all 14 bits invisibly through the analogue path and call the output a faithful 106 S/H model.

Some firmware values arrive in differently aligned forms before `loadDac`; the reference implementation should preserve the ROM's actual alignment at each call site, then apply the physical 12-bit boundary.

## 3. `loadDac` is a two-step physical operation

The ROM routine does not itself open the destination S/H MUX. It:

1. inhibits all relevant MUXes;
2. presets the MUX/channel address while inhibited;
3. places the DAC value on the physical bus;
4. returns;
5. the caller then uninhibits the intended physical MUX group.

Reference event model:

```text
DacBusLoad(channel, dac12)
MuxCommit(group, channel)
```

For first implementation these may occur at the same scheduler timestamp if sub-instruction timing is not modeled, but they must remain conceptually separate so later settling/aperture measurements can be inserted without changing architecture.

## 4. Frozen main-loop causal order

The ROM main loop starts at approximately `$02EC` and eventually jumps back there from the NOISE-level service near `$07B5`.

The robust Phase-0 order is:

```text
FRAME/LOOP START
  1. commit IC40 latch state: HPF / chorus / saw-related latch outputs
  2. update effective voiceRun state from gate/sustain state
  3. service VCF RESONANCE held CV
  4. compute automatic LFO-Delay/onset state and current LFO-derived modulation scalers
  5. service global VCA LEVEL held CV
  6. build current global pitch basis and update per-voice Portamento state
  7. service SUB LEVEL held CV
  8. for physical voices 1..6:
       a. build voice pitch from current per-voice + global pitch state
       b. compute integer 8253 divisor and DCO-CV value
       c. perform ordinary count load or reset/control-word + count load
       d. service that voice's DCO-CV S/H
  9. compute global VCF base = Cutoff +/- LFO +/- Bender
 10. per-voice ENV/VCA/VCF service region:
       a. advance that voice's integer envelope state
       b. service PWM at its ROM-defined point in this region
       c. service per-voice VCA held controls in ROM order
       d. add ENV and Key Follow to VCF base, clamp, service per-voice VCF S/H
 11. process current ADC/bender/portamento/performance-input data
 12. complete late VCA/control service required by the ROM pipeline
 13. advance the LFO accumulator/state
 14. service NOISE LEVEL held CV
 15. jump to loop start
```

This order is frozen. Exact fractional slot offsets inside it are calibration.

## 5. Important one-frame causality

Because the LFO accumulator is advanced late in the loop, while pitch/VCF modulation quantities are consumed earlier, strict scheduling must preserve the already-recovered causality:

```text
early-frame DCO/VCF work
  uses stored/current pre-advance LFO state

late-frame LFO step
  becomes available to a later loop pass
```

Likewise, panel/performance ADC processing occurs after the main DCO/VCF service region and naturally feeds later work rather than retroactively changing S/H values already committed earlier in the same pass.

## 6. Per-voice DCO service is sequential

The DCO loop starts with physical voice 1 and walks six voices.

For each voice the ROM:

- derives the timer divisor;
- checks the per-voice reset flag;
- when needed, writes the relevant 8253 Mode-3 control word;
- writes divisor low byte then high byte with interrupts disabled around the critical counter write;
- then computes/loads the DCO-CV value and commits that voice's S/H;
- advances to the next physical voice.

Therefore six voices are not pitch-updated atomically.

This is especially important for Solo-Unison, where the assigner also transmits six note events sequentially.

## 7. Per-voice ENV/VCA/VCF work is also sequential

The later loop region iterates across the six physical voices rather than advancing all envelopes first and publishing all VCA/VCF values later.

Strict reference must permit each physical voice to have its own update timestamp within the control frame.

The exact low-level MUX channel numbering/interleave is retained in the ROM evidence, but Phase 0 freezes the musically relevant contract as physical-voice ordered ENV/VCA/VCF service. A cycle harness or real MUX capture can replace the provisional slot offsets later.

## 8. PWM is a global held-control service in the voice region

PWM/PWM-LFO is not recalculated as an audio-rate duty parameter. Its current firmware result is sent through the shared DAC/S&H system at a defined point in the per-voice service region.

The MC5534 model therefore consumes a held PWM CV that changes on scheduler events.

## 9. Voice note events can restart the main-loop pipeline

The serial ISR has two qualitatively different exit classes.

Ordinary parameter values return from the interrupt and allow the interrupted main-loop work to continue.

The Voice-On/note-event path reaches `$0149`, where the ROM:

```text
LXI SP,$FFFF
JMP $02EB
```

That deliberately discards the interrupted call stack and resumes immediately before the main-loop entry.

Voice-off/note handling also has paths into this restart point.

Strict model consequence:

```text
NoteEventRestart:
  mutate gate/note/attack/reset state
  abandon remaining current main-loop pass
  restart scheduler from FRAME_START
```

This is stronger than adding small random jitter to an otherwise periodic frame.

A parameter/ADC interrupt should instead be modeled as an interrupt/latency event that normally returns to the current pass.

## 10. Interrupt classes

### ADC interrupt

The ADC ISR swaps register sets, stores the newly converted ADC bank, toggles the ADC bank selection and returns.

Architecture:

```text
ADC interrupt -> bounded local interruption -> resume current pass
```

### Serial parameter update

Normal patch/control bytes update state and return through RETI.

Architecture:

```text
parameter serial interrupt -> mutate control RAM -> resume current pass
```

### Voice note event

Note/gate/retrigger processing may intentionally clobber the stack and restart the main loop.

Architecture:

```text
voice event -> mutate voice state -> abandon remainder -> restart from loop start
```

These three cases must not be collapsed into one generic `onMidi()` callback.

## 11. Frame period

Primary Roland documentation supports approximately:

```text
~4.2 ms per complete normal service cycle
```

A commonly reported engineering target is approximately:

```text
4.2335 ms ~= 236.21 Hz
```

The latter is useful as an initial desktop calibration target but is not frozen as an exact universal hardware constant.

Reference state:

```text
ControlTimingCalibration106.frame_period_mean_us
ControlTimingCalibration106.frame_period_jitter_us
```

Frame duration may vary with branch paths and asynchronous interrupts.

## 12. Sample-and-hold model boundary

Each physical held control is represented by:

```text
SampleHold106 {
    held_value;
    last_update_time;
    settle_tau;
    droop_rate;
    source_impedance;
    aperture_or_mux_charge_error;
}
```

First Phase-1 reference may use ideal hold after 12-bit quantization:

```text
settle_tau = 0
droop_rate = 0
```

provided these are explicit calibration placeholders rather than silently removed from the architecture.

Later physical measurements can add settling/droop without changing control logic.

## 13. Deterministic scheduler API

```text
enum ControlEventType106 {
    FrameStart,
    LatchCommit,
    DacBusLoad,
    SampleHoldCommit,
    TimerCountLoad,
    TimerResetAndLoad,
    EnvelopeAdvance,
    LfoAdvance,
    AdcInterrupt,
    SerialParameterInterrupt,
    VoiceEventRestart
};

struct ControlScheduler106 {
    void runUntil(double t);
    void dispatchVoiceEvent(...);
    void dispatchParameterByte(...);
    void dispatchAdcResult(...);
};
```

Event timestamps must be deterministic for a given input/event stream and calibration dataset.

Do not generate synthetic random timing jitter in strict regression tests. Hardware-measured jitter distributions can be added later as an optional calibrated realism layer.

## 14. Required regression vectors

### 14.1 Normal idle/held frame

Record event-order trace for:

```text
idle
one held note
six held notes
```

Assert that causal event ordering is identical even when branch execution time differs.

### 14.2 Voice-event restart

Inject Voice On at several points in a pass:

```text
before DCO voice loop
mid DCO voice loop
mid ENV/VCF loop
after LFO advance
```

Assert:

- voice state changes immediately according to ISR semantics;
- remaining scheduled events from the abandoned pass do not commit;
- scheduler restarts at the loop-start boundary;
- timer reset request remains separate from envelope attack re-arm.

### 14.3 Parameter interrupt

Inject a parameter byte mid-pass and assert that execution resumes rather than restarting the frame.

### 14.4 DAC quantization

For representative 14-bit values verify:

```text
0x0000 -> 0x000
0x0003 -> 0x000
0x0004 -> 0x001
0x3fff -> 0xfff
```

and verify the analog S/H receives the quantized value, not the original lower two bits.

### 14.5 Ordered six-voice service

Trace DCO-CV and VCF S/H events for six voices and verify monotonically ordered physical-voice service rather than one broadcast timestamp.

### 14.6 LFO causality

Verify that the LFO state advanced near the end of pass N does not retroactively affect early DCO/VCF events already committed in pass N.

## 15. What is frozen vs measured later

Frozen:

- approximately 4.2 ms normal service-cycle family;
- ordered, non-atomic main-loop architecture;
- exact relative macro-order of major control regions;
- sequential physical-voice DCO service;
- sequential physical-voice ENV/VCA/VCF service;
- 8253 count writes happen before that voice's DCO-CV S/H commit in the DCO loop;
- physical DAC is 12-bit at the analogue boundary;
- MUX inhibited during DAC bus update, caller then commits destination S/H;
- LFO advance occurs late, after current pass's main DCO/VCF use;
- ADC/parameter interrupts normally resume current pass;
- note-event handling can abandon the pass and restart at loop start.

`MEASURE_PENDING` / cycle-harness calibration:

- exact mean frame period and branch-dependent period surfaces;
- exact microsecond offset for every S/H commit;
- exact interrupt latency/jitter distributions;
- exact low-level VCA/PWM MUX interleave/channel mapping where disassembly comments are ambiguous;
- DAC settling time;
- per-S/H hold-cap settling, droop, source impedance and charge injection.

**Phase-0 control-timing state: `CONTROL_TIMING_PHASE0_ARCHITECTURE_FROZEN`.**
