# FM1JUNO Phase-1 implementation status

Updated: 2026-11-03
Active branch: `phase1/reference-engine`
Authority: `docs/PHASE1_START_CONTRACT.md` -> `docs/PHASE1_REFERENCE_ENGINE_PLAN.md` -> relevant Phase-0 block-specific models.

**State: `P1.0_TRACE_SCAFFOLD_RUNNING / P1.1_DIGITAL_IN_PROGRESS`.**
**Do not mark P1.1 as complete, and do not begin claiming analogue/audio 106 fidelity.**

## 1. Current checked-in code

Portable C core:

- `src/common/fixed_time.h`: absolute Q32.32 sample/subsample time;
- `src/common/event_queue.*`: fixed-capacity deterministic ordered event queue;
- `src/control106/assigner106.*`: Poly 1 / Poly 2 / Solo-Unison voice logic;
- `src/control106/envelope106.*`: 14-bit ADSR state and clean-room Attack/Decay/Release/Sustain coefficient laws;
- `src/control106/lfo106.*`: integer LFO and compact rate/depth laws;
- `src/control106/lfo_delay106.*`: two-stage LFO Delay and physical/MIDI performance paths;
- `src/control106/portamento106.*`: constant-step pitch glide and compact step law;
- `src/control106/control_math106.*`: Tune and DCO/VCF Bender arithmetic helpers;
- `src/control106/pitch106.*`: 8.8 pitch arithmetic, exact index/clamp/interpolation, derived DCO-CV curve and replaceable 8253 base-count candidate;
- `src/control106/vcf_control106.*`: firmware-domain VCF global base, per-voice ENV/Key Follow, final clamp and DAC quantization;
- `src/control106/dac106.*`: 14-bit-to-physical-12-bit boundary;
- `src/control106/control_scheduler106.*`: non-atomic ROM macro schedule plus voice-event restart;
- `src/control106/module_control106.*`: six-voice integration harness, event trace, timer/ENV, now ordered per-voice VCF holds;
- `src/timer106/timer8253_106.*`: 8253 Mode-3 event-time model.

Host/test entry points:

- CMake host build and GitHub Actions;
- `fm1juno_ref` reference scaffold;
- `fm1juno_render` CSV event-script/trace CLI;
- deterministic unit, integration, invariance, pitch and coefficient tests.

**Important limitation:** `fm1juno_render` currently prints trace/state results and explicitly says `audio_wav=not_available_until_reference_audio_blocks`. There is not yet a WAV-producing reference instrument, 80017A DSP or MC5534 audio renderer. A "render" executable name is not evidence of audio-fidelity completion.

## 2. New VCF control work

Files:

- `src/control106/vcf_control106.h`
- `src/control106/vcf_control106.c`
- `tests/unit/test_vcf_control.c`
- `tests/unit/test_module_control.c` (new scheduler-integration vectors)

Behavioral boundary:

1. `global_raw = cutoff14 +/- floor(lfoMag*depthAfterDelay/256)/2 +/- bend` in signed widened arithmetic;
2. global value is **not** prematurely clamped;
3. per physical voice, update ENV then add/subtract scaled ENV;
4. Key Follow uses `(pitch88>>2)+(pitch88>>3)` relative to `0x1680`, multiplied by `rawKeyFollow*2`;
5. clamp final per-voice value to `0..0x3fff`, then convert to 12-bit DAC code;
6. commit voices in ROM scheduler order, preserving a partially completed pass if a note event causes restart (remaining slots are abandoned).

This is a well-grounded macro-arithmetic model, **not** a claim that every obscure uPD7811 carry/borrow-flag corner has been compared against a cycle-accurate original-CPU execution.

The direct `module_control106_set_voice_pitch()` and `module_control106_set_vcf_params()` APIs remain legacy injection boundaries for standalone tests. The new timestamped raw-parameter mode (sections 6–7) now drives Portamento, global DCO pitch and VCF through the macro scheduler. Full original serial/ADC preprocessing and exact uPD7811 cycle verification remain open.

## 3. Timer divisor candidate: evidence and gate

**Do not silently promote `pitch106_timer_build_derived_candidate()` to ROM-exact strict mode.**

The original 104-value 8253 divisor series is not exactly described by a single equal-tempered inverse exponential. Instead of copying its ROM words, the candidate independently generates 104 integer counts **once at initialization**, then exact firmware-style fractional interpolation is applied during control updates.

For index `i` with `x=2^(-i/12)`, the compact current candidate is:

- `i=0..71: round(61355*x + 210*x*x)`
- `i=72..83: round(61355*x)`
- `i=84..103: round(61325*x)`

The code uses integer Q24 for the reciprocal semitone recurrence. There is **no original 104-entry note table in production code**.

An offline comparison against the 104 public disassembly values found:

- maximum approximate base-count pitch deviation: **0.4782 cents** across the examined sequence;
- highest indices `72..103` matched the checked base counts exactly in that comparison;
- the low-register count deviation can be larger than one LSB without producing a large cent deviation;
- no claim of bit-identical timer counts is made;
- runtime lookup/interpolation is cheap; generating the profile uses 64-bit multiplication **only during initialization**, not inside the 44.1-kHz render callback.

Regression tests in `test_pitch_conversion.c` use sparse independent observation anchors, an integer relative-error bound, clamp and fractional-interpolation vectors. They deliberately assert that the experimental profile differs from at least one original anchor.

**Open strict gate:** obtain an independently derived exactly matching compact base law, or explicitly approve and document a bounded pitch-error fidelity tier distinct from the bit-exact control tier. Do not mix these two labels.

The DCO-CV curve is also a clean-room exponential generator: its available sparse hardware/ROM-derived anchor checks are within one LSB. It is **not** certified bit-identical across every possible index or fraction.

## 4. Current quality gates

Confirmed:

- [x] Host CMake project and GitHub Actions workflow.
- [x] Integer ENV, LFO, Portamento and DCO/VCF Bend primitives.
- [x] Clean-room compact parameter coefficient laws.
- [x] Ordered module control scheduler with Voice On/Off restart.
- [x] Physical 12-bit DAC quantization boundary.
- [x] 8253 Mode-3 baseline event model.
- [x] Deterministic 16/32/64/128/256-frame **control trace** invariance tests.
- [x] VCF global/per-voice arithmetic tests, signed unclamped base, Key Follow and ENV corner vectors.
- [x] VCF S/H integration test for interleaved per-voice updates and interrupt/restart.
- [x] Derived divisor-candidate profile and sparse error-bound regression vectors.
- [x] Host-tests CI #126 completed successfully after the timestamped-parameter implementation and end-to-end CSV smoke (`808240d`).

Still open before **P1.1 exact digital/control exit**:

- [x] Versioned CSV script `PARAMETER` events with Q32.32 timestamps; validated 7-bit, 8-bit and flag domains, stable fixed-capacity event queue, original interrupt-vs-restart behavior at macro level.
- [ ] Validate original hardware/serial/MIDI preprocessing and exact ADC/physical switch endpoint behavior against an executable module-CPU reference.
- [x] Shared DCO service: an injected Portamento pitch and latched global pitch offset now feed both 8253 count and DCO-CV/S&H from **one** final pitch coordinate (candidate profile explicitly identified).
- [x] Raw-parameter driven Tune + integer DCO-LFO/depth/Delay + signed Bend and per-voice Portamento now update the shared timer/DCO-CV pitch path at the ROM macro service slots.
- [ ] Extend raw producer to complete Roland protocol/ADC scan semantics and verify macro and cycle-level timing against executable CPU traces.
- [ ] Certify a ROM-equivalent divisor base or explicitly label/configure the accepted non-exact tier.
- [ ] Complete VCF global/voice dataflow tests against executable CPU arithmetic for rare carry/borrow cases.
- [x] Independent Mode-3 vectors now exercise normal even/odd counts, N=0→65536, high/low pending writes, repeated pending overwrites, fractional-sample reset and range-specific reset timings.
- [ ] Original 8253 count=1/3 corner behavior, GATE/CLK alignment, live-range phase switching and actual MSB-write-to-first-edge timing still need independent hardware-level validation.
- [x] `LFO_DELAY_AND_MOD_SCALE`, `PITCH_AND_PORTAMENTO`, `VCF_GLOBAL_BASE`, `ENV_VCA_VCF_VOICE`, `ADC_PERFORMANCE` and `LFO_ADVANCE` now update persistent digital control state when raw-parameter mode is active.
- [x] Secondary output hold coverage: resonance, global VCA LEVEL, SUB, NOISE and delayed PWM now route through distinct physical mux channels and hold 14-bit/12-bit digital codes; voice VCA 0..4 uses previous-voice slot and voice 5 uses the separate post-ADC slot.
- [ ] Independently verify native packed switch bitfields and raw ADC preprocessing of PWM/gate modes, beyond the semantic test flags.
- [ ] Resolve/explicitly defer exact microsecond slot offsets to calibrated scheduler data. Do not fabricate them.

Before the broader **Phase 1A/1D audio exit**:

- [ ] WAV writer and real audio reference engine; current render CLI is trace-only.
- [ ] Generate/read central calibration manifest and print all active `MEASURE_PENDING` values.
- [ ] Event queue integration into final sample-accurate audio dispatcher and end-to-end callback-invariance renders.
- [ ] High-precision MC5534 reference, then shared noise/mixer, 80017A, global output and chorus according to plan.

## 5. Shared DCO pitch/CV integration now implemented

Files: `module_control106.h/.c` and `tests/unit/test_module_control.c`.

- The host harness injects a complete Portamento 8.8 voice pitch and a global Tune/LFO/Bender 8.8 offset.
- The global offset becomes *active* on the ROM `PITCH_AND_PORTAMENTO` scheduler slot.
- Each subsequent physical `DCO_VOICE` service computes one final pitch coordinate; **that same value** drives profile-derived timer count and DCO-CV.
- Timer count reset/queue trace precedes that voice's physical DCO-CV S/H trace.
- An ordinary pitch change queues a new count without discarding the current Mode-3 half-cycle, while its DCO-CV hold is updated at this voice's scheduled service point.
- The injected base-count profile is explicitly replaceable, and the current generated candidate is not strict count-bit-identical.
- Default/manual `DCO_COMMAND` behavior remains available for deterministic legacy tests.

The new integration regression verifies one-frame global-offset causality, initial reset, later ordinary count queueing, shared pitch and the timer-write-before-CV ordering.

**This does not mean the full original 106 pitch pipeline is done.** Real raw parameter reception, LFO/ENV servicing, per-voice Portamento state updates, and full 8253 clock-edge coordination still need implementation and independent vectors.

## 6. Timestamped raw parameter / control producer — implemented

New code:

- `src/control106/raw_params106.h/.c`: 21 validated raw panel/performance parameters, with explicit 7-bit, 8-bit and boolean domains.
- `src/control106/module_control106.h/.c`: timestamped parameter-receive API, raw-state mode and actual LFO/Delay/Bend/Portamento/VCF/ENV service-slot writes.
- `src/control106/event_replay106.h/.c`: version-1 CSV commands `PARAMETER` and explicit derived `PITCH_PROFILE`; stable fixed-capacity event-queue integration.
- `reference/render_main.cpp`: parser for both new event types.
- `tests/unit/test_raw_parameter_flow.c`: macro slot causality, LFO/Bend + ENV/VCF integration, note-event restart and 16/32/64/128/256-frame invariance.
- `tests/vectors/p1_raw_parameter_replay_v1.csv`: reproducible fractional-timestamp script and CLI smoke.
- `docs/PHASE1_RAW_PARAMETER_EVENTS.md`: parameter registry and authority for this interface.

The LFO state does not advance at sample rate; it advances only at its ROM macro slot. LFO Delay and performance control are separate, and the physical/MIDI performance effect becomes visible on the following LFO depth service. Portamento steps each active voice in 8.8 semitone space before the DCO loop. Raw parameter writes increment the parameter-interrupt counter without rewinding the control pass. A note event still restarts it and abandons unserviced slots.

Existing direct setters and a manual `DCO_COMMAND` remain usable for legacy harness tests when raw-parameter mode is not activated.

**Limit:** successful macro-level traces do not certify original hardware ADC representation, raw MIDI decoding or cycle-accurate scheduler microseconds.

## 7. Secondary digital control outputs — implemented and tested

New files: `src/control106/secondary_control106.h/.c`, `tests/unit/test_secondary_control.c`.

- Eight additional stable `RawParameterId106` IDs (21–28) for PWM depth/manual/pulse enable, VCF resonance, SUB, NOISE, global VCA LEVEL and VCA ENV/gate mode; earlier CSV IDs are unchanged.
- Resonance, SUB, shared NOISE and global VCA LEVEL are individually held as `raw7 << 7` on their original ROM-order mux channels.
- PWM's 14-bit integer law is computed **at LFO_ADVANCE**, then held to its IC23 channel-7 S/H only when the **next** main-loop pass processes voice-0 ENV. A panel change after the calculation cannot modify this pending PWM code retroactively.
- Physical voice-VCA 0..4 writes lag the corresponding envelope tick by one voice-service slot: service voice 1 writes VCA 0, etc. Voice 5 VCA hold is now represented by a dedicated **post-ADC** `CONTROL106_FINAL_VCA_VOICE6` scheduler macro.
- Voice VCA ENV and gate-mode code paths use the running-voice bitmap and the 14-bit envelope state. These are not the global uPC1252 gain cell.
- Each `SecondaryHold106` retains code14, physical DAC12, commit time, pass ID and validity. Tests cover mux bank/channel distinction, PWM end points, one-pass latency, note-interrupt abort and global hold timing.
- The timestamped callback-invariance regression now includes all eight new parameters and remains exact across 16/32/64/128/256 outer sizes.

**Verified:** GitHub Actions host-tests #148 passed for new secondary output tests; #150 passed after all new raw IDs were inserted into the timestamped callback-invariance script.

**Boundary:** the 0/1 PWM/manual/gate flags are **normalized semantic test inputs**. They are not proven to be the original 106 packed switch serial bytes. The physical VCA analog response and S/H settling are explicitly not modeled here. See `docs/PHASE1_RAW_PARAMETER_EVENTS.md`.

## 8. 8253 Mode-3 independent edge validation — regular-count subset tested

Implemented/validated:

- `tests/unit/test_timer_edge_vectors.c` covers explicit independent timing vectors rather than only reusing the timer's own helper functions.
- Q32.32 absolute edge timestamps are checked across normal odd/even counts, count=0→65536, queued updates in both halves, overwrite of pending writes, reset/discard and all three project RANGE clocks via explicit reset.
- The legacy module harness no longer starts unconfigured with count=1 (a zero-length low edge in the simplified model); it defaults to valid register count 0.
- Scripted `DCO_COMMAND` rejects special small counts 1 and 3 until their original 8253 behavior is independently proven, while accepting 0, 2 and the normal 4..65535 values.
- `src/timer106/timer8253_106.h` exports `timer8253_106_regular_mode3_count()` to state the supported subset explicitly.

**Verified:** GitHub Actions host-tests #158 passed for the independent timing-vector suite; #168 passed with the unsupported-count guard and safe default.

**Open:** physical LSB/MSB completion phase, live PF-range change/prescaler phase, GATE, exceptional 1/3 count behavior and independent exact original-chip comparison. The current half-cycle pending-update behavior is still a **reference model contract**, not full hardware timing certification.

Authoritative documentation: `docs/PHASE1_8253_EDGE_VALIDATION.md`.

## 9. Next implementation slice

1. Recover packed serial switch 1/2 byte semantics (PWM manual/LFO, pulse enable, VCA ENV/gate, HPF/chorus and octave range bits), and validate against the original module-ROM instructions and route maps.
2. Audit physical ADC/hysteresis and bender endpoint handling. Preserve normalization boundaries and avoid inventing native MIDI/ADC equivalence.
3. Check 8253 and 80017A coupling: full count precision, physical clock phase, and original range change while running. Keep the exceptional count and live-range cases explicitly open if hardware evidence is insufficient.
4. Close remaining exact digital P1.1 regression gates before any MC5534 waveform reference or FM-1 target implementation.

`fm1juno_render` is still trace-only; no original-unit analogue/audio fidelity is claimed.
