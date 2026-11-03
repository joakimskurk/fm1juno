# P1.1 — Timestamped raw parameter/control producer (implementation note)

Status: **implemented and CI-tested host control slice, not the complete exact P1.1 exit.**

Use `docs/PHASE1_IMPLEMENTATION_STATUS.md` for overall current status, and `docs/PHASE1_NATIVE_PANEL_SERIAL_ADC.md` for the native byte/ADC receiver and original IC29 command protocol. This document is the source of truth for the CSV/event interface introduced in this slice.

## Event script: version 1, backwards-compatible

CSV header:

`version,sample,frac_q32,type,voice,value,aux`

Timestamp is Q32.32 sample time: `(sample<<32)|frac_q32`. Input events must be ordered by timestamp; events with equal timestamps retain their source-file ordering.

New types:

- `PARAMETER`: `voice=255`, `value=<raw parameter value>`, `aux=<RawParameterId106>`. Invalid type/value/voice or an out-of-order timestamp rejects the script.
- `SWITCH1` / `SWITCH2`: `voice=255,value=0..255,aux=0` receives the literal original packed switch register.
- `ADC_BANK`: `voice=0|1,value=<four packed little-endian 8-bit channels>,aux=0` publishes an ADC interrupt snapshot; filtering waits for the late ADC slot.
- `SERIAL_BYTE`: `voice=255,value=0..255,aux=0` feeds one native IC29 UART command/data byte. Patch running status and voice-event restart are supported; test mode is intentionally ignored.
- `PITCH_PROFILE`: `voice=255,aux=0,value=1` opts into the currently **DERIVED_APPROXIMATE** 8253 divisor candidate; `value=0` disables profile use. No implicit ROM-exactness claim.
- Existing `VOICE_ON, VOICE_OFF, SUSTAIN, DCO_COMMAND, CONTROL_STEP` remain valid. An input note is routed to the physical voice specified by `voice` in the current module-replay harness (assigner tests run separately).

A fixed-capacity `Fm1junoEventQueue` now receives events for each outer block. It drains in timestamp/sequence order. If one block contains more than the capacity, it deterministically drains the earlier events and continues; this is safe because input is already time-sorted. The queue does not allocate heap memory. The **control-step timestamps remain explicit**: this is not a fabricated microsecond-accurate scheduler loop.

Reference vector: `tests/vectors/p1_raw_parameter_replay_v1.csv`.

## Parameter ID registry

| aux | parameter | allowed value |
|---:|---|---|
| 0 | `RAW106_TUNE_ADC8` | 0..255 |
| 1 | `RAW106_PORTAMENTO_ADC8` | 0..255 |
| 2 | `RAW106_LFO_RATE7` | 0..127 |
| 3 | `RAW106_LFO_DELAY7` | 0..127 |
| 4 | `RAW106_DCO_LFO_DEPTH7` | 0..127 |
| 5 | `RAW106_BEND_MAGNITUDE8` | 0..255 |
| 6 | `RAW106_DCO_BEND_SENSITIVITY8` | 0..255 |
| 7 | `RAW106_VCF_BEND_SENSITIVITY8` | 0..255 |
| 8 | `RAW106_BEND_NEGATIVE` | 0 or 1 |
| 9 | `RAW106_VCF_CUTOFF7` | 0..127 |
| 10 | `RAW106_VCF_LFO_DEPTH7` | 0..127 |
| 11 | `RAW106_VCF_ENV_DEPTH7` | 0..127 |
| 12 | `RAW106_VCF_ENV_INVERTED` | 0 or 1 |
| 13 | `RAW106_VCF_KEY_FOLLOW7` | 0..127 |
| 14 | `RAW106_ENV_ATTACK7` | 0..127 |
| 15 | `RAW106_ENV_DECAY7` | 0..127 |
| 16 | `RAW106_ENV_SUSTAIN7` | 0..127 |
| 17 | `RAW106_ENV_RELEASE7` | 0..127 |
| 18 | `RAW106_PERFORMANCE_TRIGGER` | 0 or 1 |
| 19 | `RAW106_PERFORMANCE_SENSITIVITY8` | 0..255 |
| 20 | `RAW106_MIDI_MOD_DEPTH8` | 0..255 |
| 21 | `RAW106_PWM_DEPTH7` | 0..127 |
| 22 | `RAW106_PWM_MANUAL` | 0 or 1 |
| 23 | `RAW106_PULSE_ENABLED` | 0 or 1 |
| 24 | `RAW106_VCF_RESONANCE7` | 0..127 |
| 25 | `RAW106_SUB_LEVEL7` | 0..127 |
| 26 | `RAW106_NOISE_LEVEL7` | 0..127 |
| 27 | `RAW106_GLOBAL_VCA_LEVEL7` | 0..127 |
| 28 | `RAW106_VCA_GATE_MODE` | 0 or 1 |

An update to a recognized raw parameter modifies the module's firmware-like RAM immediately and signals a **parameter interrupt**, but does **not** restart the main-loop pass. Derived outputs update at their own service slots; a VOICE_ON/OFF causes a main-loop restart.

## Service order / causality

1. `FRAME_START`: recompute voice-run and arm/restart LFO Delay after silence.
2. `LFO_DELAY_AND_MOD_SCALE`: advance two-stage Delay when voices are active, combine delayed panel DCO-LFO depth with previous performance depth; this **does not** advance the LFO waveform.
3. `PITCH_AND_PORTAMENTO`: build a signed 8.8 global offset from raw Tune, current integer LFO/depth, and signed DCO Bend; advance each voice's Portamento 8.8 pitch toward its latest note, using the compact 8-bit step law. One global offset is latched for this pass.
4. `DCO_VOICE[0..5]`: for each voice, derive one final pitch, queue/reset 8253 timer count, then commit the *same pitch*-derived DCO CV to its own 12-bit S/H. With profile disabled, legacy manual count commands remain the test harness.
5. `VCF_GLOBAL_BASE`: derive Cutoff, LFO, VCF Bend in widened signed arithmetic; no premature clamp.
6. `ENV_VCA_VCF_VOICE[0..5]`: use current raw ENV A/D/S/R coefficient laws to tick that voice and current ENV depth/key follow to calculate and hold the per-voice 12-bit VCF CV.
7. `ADC_PERFORMANCE`: advance physical trigger declick / MIDI modulation maximum path (available to the **following** LFO-depth service).
8. `FINAL_VCA_VOICE6`: output physical voice-six VCA sample-and-hold. This happens after ADC, unlike that voice's earlier ENV and VCF writes.
9. `LFO_ADVANCE`: update rate coefficient and integer LFO accumulator (its new magnitude/polarity is available to the **following** control pass); compute the new PWM register.
10. `NOISE_LEVEL`: commit shared noise-source VCA control code through the separate IC26 mux channel 7.

This avoids running a synthetic 44.1-kHz LFO or portamento; no output changes solely because a host audio buffer boundary happened.

## Regression vectors in the code

- `tests/unit/test_raw_parameter_flow.c`: invalid id/value rejection with no side effects; within-frame parameter latched on correct slot; 8.8 Portamento steps; DCO Bend and LFO integer scaling; per-voice DCO CV and VCF hold; note-event frame abort; Q32.32 fractional same-sample parameter ordering; callback-size invariance at 16/32/64/128/256.
- `tests/unit/test_event_replay.c`: original event-script behavior remains valid.
- `tests/unit/test_module_control.c`: prior exact/derived DCO and VCF slot tests remain valid.
- `fm1juno_render tests/vectors/p1_raw_parameter_replay_v1.csv --samples 128 --outer-frames 64`: host CLI smoke.

## Secondary DAC/S&H control outputs — new P1.1 slice

The exact common ROM parameter scaling for resonance, SUB, NOISE and the global VCA LEVEL is `raw7 << 7`. All four feed different physical S/H destinations and must **not** be modeled as a shared analog gain or single simultaneous write:

| ROM macro slot | control | mux bank | channel |
|---|---|---|---:|
| `VCF_RESONANCE` | resonance | IC26 VCA | 6 |
| `GLOBAL_VCA_LEVEL` | global uPC1252-level control | IC23 VCF | 6 |
| `SUB_LEVEL` | common SUB control | IC24 ramp | 6 |
| `ENV_VCA_VCF_VOICE[0]` | previous-pass PWM register | IC23 VCF | 7 |
| `ENV_VCA_VCF_VOICE[1..5]` | voice VCA for physical voice `index-1` | IC26 VCA | 0..4 |
| `FINAL_VCA_VOICE6` | voice VCA for physical voice 5 | IC26 VCA | 5 |
| `NOISE_LEVEL` | common NOISE control | IC26 VCA | 7 |

PWM follows the recovered ROM integer law:

```text
lfo_carrier = (lfo_polarity_negative ? 0x2000 + lfo_magnitude
                                   : 0x2000 - lfo_magnitude)
carrier = pwm_manual ? 0x3fff : lfo_carrier
pwm_code14 = pulse_enabled
  ? 0x3fff - floor(carrier * (pwm_depth7 << 1) / 256)
  : 0
```

This computation happens **after** the LFO advances and is held as an intermediate code. The PWM S/H receives that intermediate when the **next** pass services voice 0. Switching between manual and LFO modes during the intervening pass therefore does not retroactively recalculate an already prepared code.

Voice VCA uses current ENV 14-bit code in ENV mode or `0x3fff` / `0x0000` from `voiceRun` in gate mode. It is distinct from the later global uPC1252 VCA and is committed with the ROM's one-voice offset: ENV 1 writes voice VCA 0; ENV 5 writes voice VCA 4; the final voice VCA 5 only writes **after ADC**.

`SecondaryHold106` tracks the last 14-bit digital value, 12-bit DAC value, timestamp, pass ID and a validity bit, with explicit physical mux routing. This tracks digital holds only: it does not model analog settling, output headroom, or BA662/uPC1252 control voltage-to-gain curves.

**Raw switch normalization caveat:** `RAW106_PWM_MANUAL`, `RAW106_PULSE_ENABLED` and `RAW106_VCA_GATE_MODE` are semantic 0/1 flags in the test format, **not the original switch 1/2 serial bitfields**. Those packed switch bytes and precise panel ADC/hysteresis effects still need separate native-protocol verification.

Tests: `tests/unit/test_secondary_control.c` (mux destinations, PWM extrema, one-pass latch delay, voice VCA one-slot lag and note-event abort) and `test_raw_parameter_flow.c` (timestamped cross-callback secondary-control regression).

## Precision and remaining gates

This is a coherent macro-level arithmetic emulator. It **does not** establish cycle-accurate uPD7811 behavior or measured 106 controller response for every raw value. Specifically:

- original module-CPU ADC/button serialization, MIDI preprocessing and physical bender endpoint signs/offsets remain integration/validation work;
- raw parameter values use an explicit engineering test convention where multiple original hardware/serial representations may exist;
- raw `DCO_BEND_SENSITIVITY8` and `VCF_BEND_SENSITIVITY8` are post-preprocessing scalers; avoid calling them direct 7-bit JUNO panel bytes;
- the 8253 generated divisor candidate and derived DCO-CV curve are not ROM-table bit-identical;
- original microsecond offsets between macro slots remain `MEASURE_PENDING`;
- physical DAC/S&H settling remains an analogue hardware-fitting task;
- event queue is a host **control-replay** dispatcher, not a completed sample-accurate audio/event engine;
- `fm1juno_render` is still **trace-only** and does not create WAV files.

Native direct and serial inputs are now implemented, including one-code ADC hysteresis, endpoint correction, switch-1/switch-2 active-low bit meanings, and the delayed ADC service path. See `PHASE1_NATIVE_PANEL_SERIAL_ADC.md` for exact event IDs and ROM-address evidence. Remaining strict exit work is independent executable-CPU corner verification, physical ADC scan timing, live 8253 clock/range semantics, and complete control-frame traces.
