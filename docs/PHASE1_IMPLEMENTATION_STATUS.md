# FM1JUNO Phase-1 implementation status

Updated: 2026-11-05
Active branch: `phase1/reference-engine`
Authority: `docs/PHASE1_START_CONTRACT.md` -> `docs/PHASE1_REFERENCE_ENGINE_PLAN.md` -> relevant Phase-0 block-specific models.

**State: `P1.0_TRACE_SCAFFOLD_RUNNING / P1.1_DIGITAL_IN_PROGRESS`.**
**Do not mark P1.1 as complete, and do not begin claiming analogue/audio 106 fidelity.**

**Latest verified result (read §24 for full evidence):**
**832/832 selected MAME CPU process scenarios pass** across the
separately verified test gates, including **36/36 executions
of a genuinely compiled, test-modified MAME 0.285 binary**
with eight distinct nonzero ADC callback inputs (source-pinned
MAME commit `f379cd06`, compiled gate #3 SUCCESS).
The original ADC/ISR, six-voice ENV/VCF and selected ADC
conditioner checkpoints match compiled production control C.
**Actual Roland firmware scheduler, CPU variant, hardware
analogue/electrical/timing and audio still remain OPEN.**
Sections §1–23 document earlier incremental milestones;
**§24 supersedes older statements that nonzero analogue
input callbacks had not yet been tested.**

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
- `src/control106/native_panel106.*`: original IC29 packed switches, IC40 frame latch, ADC ISR bank snapshot, ±1 code hysteresis and bounded endpoint correction;
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
- [x] Recover original packed switch and raw ADC integer preprocessing from public IC29 disassembly and test macro-level signed/bank/endpoint behavior via native UART and ADC_BANK inputs.
- [ ] Independently certify native CPU flags, full ADC acquisition timing and physical endpoints against an executable uPD7811 or original-hardware trace.
- [x] Shared DCO service: an injected Portamento pitch and latched global pitch offset now feed both 8253 count and DCO-CV/S&H from **one** final pitch coordinate (candidate profile explicitly identified).
- [x] Raw-parameter driven Tune + integer DCO-LFO/depth/Delay + signed Bend and per-voice Portamento now update the shared timer/DCO-CV pitch path at the ROM macro service slots.
- [ ] Extend raw producer to complete Roland protocol/ADC scan semantics and verify macro and cycle-level timing against executable CPU traces.
- [ ] Certify a ROM-equivalent divisor base or explicitly label/configure the accepted non-exact tier.
- [x] Independent test-only uPD7811 ALU semantics oracle passes exhaustive 256×256 ADC raw/held grid, 2,016 structured VCF cases and 90,000 seeded VCF carry/borrow permutations.
- [x] Execute the original published IC29 opcode bytes in separate bounded uPD7811 instruction-window runners for ADC (0x0800..0x0824) and VCF final saturation (0x0609..0x0618); compare with compiled production C.
- [x] Execute representative ADC and VCF opcode windows under the **independently maintained MAME UPD7810 CPU device** with generated test bootstrap and compare actual CPU RAM/EA to compiled C; 28 ADC + 52 VCF final-clamp + 42 VCF per-voice ENV/Key Follow cases PASS. The last 42 cover 0x05C1..0x063B but not upstream global cutoff/LFO/bend or live original IRQ state.
- [ ] Run under an actual UPD7811-configured CPU and independently sourced full original Roland ROM with cycle/interrupt semantics; the MAME UPD7810 + synthetic bootstrap window probes do **not** close this strict gate.
- [x] Independent Mode-3 vectors now exercise normal even/odd counts, N=0→65536, high/low pending writes, repeated pending overwrites, fractional-sample reset and range-specific reset timings.
- [ ] Original 8253 count=1/3 corner behavior, GATE/CLK alignment, live-range phase switching and actual MSB-write-to-first-edge timing still need independent hardware-level validation.
- [x] `LFO_DELAY_AND_MOD_SCALE`, `PITCH_AND_PORTAMENTO`, `VCF_GLOBAL_BASE`, `ENV_VCA_VCF_VOICE`, `ADC_PERFORMANCE` and `LFO_ADVANCE` now update persistent digital control state when raw-parameter mode is active.
- [x] Secondary output hold coverage: resonance, global VCA LEVEL, SUB, NOISE and delayed PWM now route through distinct physical mux channels and hold 14-bit/12-bit digital codes; voice VCA 0..4 uses previous-voice slot and voice 5 uses the separate post-ADC slot.
- [x] Native IC29 switch1/switch2 byte paths now decode active-low pulse/PWM/VCF ENV/VCA gate and HPF/IC40 bit outputs; native ADC bank processing includes ±1-code hystereses and two-slope endpoint correction, with delayed scheduler semantics.
- [x] Native IC29 serial command receiver handles patch-data running status, 0x80..0x8D physical voice/sustain commands, negative/positive MIDI bend and doubled MIDI modulation; original test-mode command is deliberately suppressed.
- [ ] Verify all switch/ADC condition flags in an executable original uPD7811 model and validate scan clock, ADC ISR timing and live PF range-phase on original hardware.
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

- `src/control106/raw_params106.h/.c`: 29 validated raw panel/performance parameters, with explicit 7-bit, 8-bit and boolean domains.
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

## 9. Original IC29 switch, ADC and serial receiver — implemented and tested

New module: `src/control106/native_panel106.h/.c`. Integrated into `module_control106` and the version-1 event replay and CLI.

- Switch-1: decoded the original active-low oscillator range/pulse flags, saw and chorus latch lines. Switch-2: inverted HPF selector plus PWM manual/LFO, ENV polarity and ENV/GATE mode.
- IC40 keeps distinct **register** vs **frame-latched output** at `FRAME_START`. The PF range value is retained as a digital output, **not** used to invent live 8253 clock-phase behavior.
- Raw four-channel ADC interrupt snapshots are banked (`ADC_BANK 0/1`), condition ±1-byte hysteresis and a piecewise endpoint law at the late `ADC_PERFORMANCE` slot, then feed Tune, Portamento, LFO trigger/sensitivity, VCF/DCO bend sensitivity, polarity and magnitude. **ADC-only interrupt input does not activate raw-control mode early**; the earlier ENV/pitch region retains its injected state until ADC servicing.
- `SERIAL_BYTE` interprets module-CPU UART commands 0x80..0xA3: immediate physical voice-off and sustain, note-on followed by note data and scheduler restart, patch command data 0x8E..0xA2 with running status, and deliberately ignored original test-mode 0xA3.
- Parser rejects narrowing overflow before casting to 8-/32-bit event fields. Two negative CTest fixtures guard the exact bug class.
- Existing `PARAMETER` semantic ID scripts remain backwards-compatible. Native bytes and normalized setters coexist, with later input determining the semantic value without retroactively changing original received raw registers.
- Primary disassembly address map, active-low bit truth tables, ADC conversion threshold examples and fidelity boundaries are documented in `docs/PHASE1_NATIVE_PANEL_SERIAL_ADC.md`.

Tests: `tests/unit/test_native_panel.c`, `tests/vectors/p1_native_switch_adc_v1.csv`, `tests/vectors/p1_native_serial_v1.csv`, CMake/CTest CLI smoke for both, and invalid parser vectors.

**CI verified:** native switch/ADC/serial unit tests #209, native serial CSV #213, hardened parser regressions #220, and ADC-only ISR causality regression **#232** all passed. The full suite was green after the final code fix.

**Not claimed:** full external assigner/MIDI CPU, actual ADC clock/bank alternation and ISR collision latency, IC40 line analogue truth table, uPD7811 cycle-accurate condition flags or live PF-clock phase.

## 10. uPD7811 ALU flags / ADC and VCF arithmetic oracle — implemented, green

New isolated regression source: tests/unit/test_upd7811_flag_oracle.c, with **no production helper calls** for expected values.

- NEC skip instructions are represented as explicit 8-bit operations with unsigned borrow/carry and skip-next results (SUBNBX, GTI, SUINB and ADDNC). The ADC reference reconstructs threshold gating and upper-range correction from these primitives.
- All **256×256 = 65,536 pairs** of previous accepted ADC code and new raw ADC code are checked against production native_adc106_process_latest and its conditioned output. Threshold anchors exercise the endpoint branches.
- The VCF reference is **wrapped 16-bit EA with retained underflow-selection flag**, updated by DADDNC/DSUBNB overflow/borrow. A separate split-byte multiplication path supplies independently computed LFO, ENV and Key Follow contributions; final saturation uses EA upper bits and the retained flag.
- **2,016 structured** ENV/pitch/sign vectors and **90,000 deterministic seeded** parameter vectors compare signed unclamped global VCF, per-voice 14-bit output, 12-bit DAC and signed Key Follow. Both positive carry and negative borrow paths are required.
- The oracle's instruction meanings were cross-checked against the independent MAME uPD7810/11 emulator and public IC29 disassembly. It is test code, not a copied CPU/ROM implementation.

**Verified:** GitHub Actions host-tests **#240 passed 18/18**, including the new fm1juno_upd7811_flag_oracle CTest target. No discrepancy was observed within the tested digital state space; no change to the already passing production VCF or ADC formulas was required.

**Important:** Independent arithmetic-model comparisons support these local integer laws, but do not prove cycle-accurate uPD7811 full-program execution, asynchronous interrupts, ADC clock phase or derived note tables. See docs/PHASE1_UPD7811_FLAG_VALIDATION.md for explicit bounds.

## 11. Original IC29 machine-opcode windows — implemented, tested

**This is actual opcode execution from the independently published IC29 disassembly bytes, not just mathematical oracle calculations.** The two bounded uPD7811 window decoders are project test-only implementations whose instruction semantics are cross-checked against MAME, **not a full MAME/uPD7811 CPU port**.

- ADC original code **0x0800..0x0824**: real PC/stack, 8-bit A/B/C, HL/DE RAM pointers, SUBNBX/SUINB/GTI/ADDNC conditional skip, JRE/JR, original four-channel loop, processed RAM writes at DE+8. Tested against the **compiled actual production ADC code** for all **65,536 old/new channel-0 pairs × all four physical channels**, including other-channel crossover/threshold vectors.
- VCF final saturation original code **0x0609..0x0618**: original LXI EA, MOV A,EAH, ONI, ONIW, JRE and FF6B overflow flag. All **262,144 16-bit EA / flag combinations** executed. **229,376** reachable signed input states independently compared with the compiled production VCF control function.
- The original bytes are **not vendored**. Actions downloads public ErroneousBosh/j106roms/ic29.txt into a build-temp file at runtime, extracts those exact PC ranges, and rejects any changed/missing source with **SHA-256 pinned window digests**.
- Strict PC exit and instruction allowlists catch unknown opcodes, unexpected jump/RET, source changes and arithmetic mismatches. The public annotated disassembly is explicitly described as partial/inaccurate/incomplete by its author; the hash authenticates **that transcription**, not the original silicon.

**Verified:** CI **#256** passed the first 65,536 ADC comparisons and 5,054,976 interpreted CPU steps; CI **#264** passed the VCF opcode window; CI **#268** passed with ADC extended to all four channels. All 18 existing CTests remained green.

Files: tests/cpu_ic29/, two additional test-only C executables in CMake, new CI steps in .github/workflows/host-tests.yml, and methodology/limitations in docs/PHASE1_IC29_OPCODE_EXECUTION.md.

**Still open:** original full ROM binary under a separately validated complete uPD7811 engine such as MAME, ADC automatic acquisition/interrupt phase and CPU overlays, complete VCF arithmetic PC path, actual live 8253 phase and ROM-exact divisor/curve proof. Do not mark P1.1 exact completed on the strength of isolated windows.

## 12. Independent MAME CPU core — implemented, externally green

This is the first **separately maintained full CPU interpreter** executing original *published IC29 opcode bytes*, distinct from our locally authored bounded instruction-window runner.

- MAME **v0.285** installed in a separate GitHub Actions job; the existing MAME JUNO-106 skeleton currently attaches **UPD7810** for the module controller, not UPD7811. An isolated, generated synthetic boot ROM seeds registers/RAM and invokes the original SHA-256-pinned ADC/VCF instruction windows. A real MAME debugger breakpoint reads CPU memory/EA. Three separate bounded probes now cover ADC, the VCF clamp, and the original per-voice VCF ENV/Key Follow arithmetic slice.
- **28/28 ADC scenarios passed**, comparing four physical channels (held + conditioned codes) against the compiled fm1juno C production code at the same input state. Key ±1 deadband and correction boundaries were included.
- **52/52 VCF saturation scenarios passed**, covering 13 16-bit EA boundary values × four FF6B flag states. The executed MAME EA result matches compiled VCF code and independent window oracle.
- **42/42 VCF per-voice control scenarios passed** across the original byte window 0x05C1..0x063B: 123 consecutive pinned bytes execute ENV depth/polarity, Portamento-pitch Key Follow, carry/borrow and final saturation. MAME EA agrees with separately compiled production `vcf106_voice_code14`. Initial envelope/pitch/global base/stack are synthetic, and the preceding original global VCF and envelope evolution are **not** executed.
- CI: **MAME independent CPU probe #11 SUCCESS**, 28 + 52 + 42 = **122/122 selected MAME process executions**; **host-tests #302 SUCCESS, 18/18 CTests**. The MAME job runs only for PR changes touching CPU probe paths or by manual dispatch, not on every normal host-test change.
- The original Roland binary is neither present nor reconstructed as a claimed complete image. MAME emits the expected **wrong checksum** warning for synthetic, clearly labeled fixture ROMs. Nothing in these runs certifies original full-ROM / real chip / cycle-accurate interrupts.
- Source and provenance: tests/cpu_ic29/run_mame_cpu_probe.py, tests/cpu_ic29/run_mame_vcf_probe.py, tests/cpu_ic29/run_mame_vcf_chain_probe.py, tests/cpu_ic29/vcf_chain_reference_cli.c, .github/workflows/mame-independent-cpu-probe.yml, and **docs/PHASE1_IC29_OPCODE_EXECUTION.md**.

**Strict state remains P1.1_DIGITAL_IN_PROGRESS**. This is genuine independent CPU execution of three bounded original-byte windows (one overlapping the clamp), not full JUNO-106 firmware execution; no audio engine/WAV or FM-1 deploy is claimed.

## 13. Next implementation slice

1. **MAME UPD7810 CPU integration now exists and passes** for bounded ADC, VCF clamp and per-voice ENV/Key Follow instruction windows. Next: investigate an explicitly configured **UPD7811 device**, verify original chip/ROM mapping and separately validated ROM provenance, and build reproducible PC/PSW/IRQ trace fixtures. Do not equate MAME's current JUNO-106 UPD7810 skeleton with an original powered-up JUNO-106.
2. **DONE (bounded scope):** extend MAME original-byte execution from 0x0609..0x0618 to the full *per-voice arithmetic slice* 0x05C1..0x063B, including original ENV scaling and Key Follow with flag-based clamp. **Still open:** preceding original global VCF/LFO/bend path, actual envelope update and interrupt-safe interleaving. Do not claim original full 0x05C0..0x0640 loop execution.
3. Independently validate actual ADC bank scan/ISR timing, rather than only injected bank snapshots.
4. Verify original 8253 GATE/prescaler range phase. Keep unknown clock phase as MEASURE_PENDING.
5. Preserve the DERIVED_APPROXIMATE pitch divisor/DCO CV fidelity tier until independently calibrated/ROM-exact.

## 14. Original IC29 global VCF opcode slice — verified against MAME

The newly added MAME probe now starts at `0x04D5` and executes the pinned
original bytes covering `0x04CB..0x04F5` (43 bytes). The original control
path loads the cutoff, precomputed LFO and precomputed bend RAM values,
selects positive/negative ADD/SUB flow via the original flag bytes, updates
the `FF6A` overflow/underflow selector and leaves the 16-bit unclamped VCF
base in EA before voice processing.

- `tests/cpu_ic29/run_mame_vcf_global_probe.py`: 64 deterministic curated
  and seeded cases exercising both LFO and bend polarities, zero/max
  values, nontrivial contributions and sign crossings.
- `tests/cpu_ic29/vcf_global_reference_cli.c`: independent compiled CLI
  calling actual production `vcf106_global_raw`; the unsigned components
  are derived through the same production entry point with isolated input
  paths rather than hand-reimplementing its coefficient laws.
- The original bytes are SHA-256 pinned to
  `08a0491367f316ed72992cfe3ecb2cef241efcdeb38b292c0b8eaa58b721ffc2`
  and fetched from the existing public IC29 listing at test time.
- New CMake target `fm1juno_vcf_global_reference` is wired to the separate
  MAME GitHub Actions workflow. **MAME CPU probe #18 SUCCESS** on commit `84c853e`:
  **64/64 global VCF** cases matched production C, alongside 28 ADC,
  52 VCF clamp and 42 ENV/Key Follow cases (186/186 selected runs).
  Host-tests #318 also passed.

This does **not** execute preceding LFO generation, calibration tables or
physical ADC acquisition. The test injects their already-computed integer
values into synthetic RAM. Nor does it execute the original full uninterrupted
global-to-per-voice scheduler, original interrupts or native UPD7811 device.

### Explicit UPD7811 configuration caveat

MAME has an `UPD7811` device type, but its own source maps an **internal
4 KiB ROM** at `0x0000..0x0FFF` and internal RAM at `0xFF00..0xFFFF`.
The shipped MAME JUNO-106 skeleton instead instantiates `UPD7810` and
loads a **separate 8 KiB module ROM region**. Merely renaming the driver CPU
type without resolving the chip's actual internal/external ROM mapping and
obtaining separately authenticated original firmware cannot close the strict
UPD7811/full-ROM fidelity gate. Source: MAME `upd7810.cpp` and
`juno106.cpp`.

## 15. Original global-to-voice VCF transfer — externally verified

**Verified: MAME independent CPU probe #19 SUCCESS on commit `3d28692`; 64/64 new linked VCF scenarios passed.** Together with the earlier 186 independently verified isolated runs, **250/250 selected MAME process runs passed**. GitHub host-tests #321 passed 18/18 on the same test-code commit.

The `tests/cpu_ic29/run_mame_vcf_linked_probe.py` test starts the stock
MAME JUNO-106 `UPD7810` module CPU at `0x04D5` with synthetic working
RAM; executes the published, hash-checked original bytes
`0x04CB..0x0502`, including the global sign selection, 16-bit arithmetic,
`FF6A` flag and original global `PUSH EA` and voice setup; then uses an
**explicitly synthetic seven-byte seam at 0x0503** to bypass the
unverified original envelope update `0x0503..0x05BF`. The bridge pushes
HL (preserving the expected original stack), points DE to the already
computed envelope at FF27, and jumps to the original `POP HL` at
`0x05C0`. Published opcodes then execute `0x05C0..0x063B` through
ENV polarity/scaling, Key Follow and final VCF clamp.

- 180 actual disassembly bytes across the two original regions, pinned
  together by SHA-256:
  `545631d0b7a24a39babc7e1b95dbf29be2c2a042b77e1d1a0fad382d6df89ce5`.
  The bridge is never represented as original code.
- Reuses 64 stable global VCF boundary/polarity cases, with varied
  14-bit ENV, pitch, ENV depth/polarity and Key Follow inputs.
- Compares resulting real MAME EA and FF6A with separately compiled
  **production C** from `fm1juno_vcf_global_reference` and
  `fm1juno_vcf_chain_reference`; also validates stack pointer and
  pitch pointer at exit. The original ROM is not distributed.
- Stage runs in `.github/workflows/mame-independent-cpu-probe.yml`. All 64 linked CPU cases match compiled production output; original global FF6A sign, SP and pitch pointer checks also passed.

**Scope:** This is a **verified** linked original-byte VCF control arithmetic *probe*,
not uninterrupted original whole-ROM execution. It does **not** cover
original ENV evolution, 6-voice looping, ADC scan timing, IRQ interleaving,
UPD7811 internal ROM, 8253 clocks or audio. Keep P1.1 strict gate OPEN
irrespective of this probe's outcome.

## 16. Former next slice — first-voice original ATTACK path now completed

**Achieved for the first voice's ATTACK branch only.** The previous
linked VCF test keeps its independently verified synthetic-seam fixture;
the new independent original-ATTACK test executes the real published
0x0503..0x05BF branch and its original lookup/DAC dependencies.

1. Extract and SHA-256 pin the published `0x0503..0x05BF` envelope-update
   byte range. First model an easily controlled original path (e.g. inactive
   voice/gate, or static sustain) whose expected state can be set explicitly.
2. Map the required RAM, pair registers, flags and stack layout at
   `0x0503`. Catalogue every in-window `CALF` destination and memory-mapped
   DAC/peripheral operation before adding any synthetic test-only stub.
   Only use a stub if it is identified, range-bounded and separately logged.
3. Extend MAME to execute an *original* envelope path before `0x05C0`,
   checking resulting envelope hold, stack and pointer against compiled
   production C. Then expand to Decay/Release branches and seed-set cases.
4. Keep the standalone global, clamp and voice tests green and independent.
   If original-byte behavior disagrees with C, retain the failing
   reproducible fixture; do not patch the production engine speculatively.
5. Investigate the real JUNO-106 IC29 module CPU variant and ROM mapping
   before asserting uPD7811-specific fidelity. MAME UPD7811 has a 4 KiB
   internal ROM map whereas MAME's JUNO-106 skeleton uses an 8 KiB external
   module ROM under UPD7810. Hardware/ROM provenance is required.
6. Continue treating ADC acquisition/IRQ timing, original 8253 phase,
   derived pitch tables and calibrated slot microseconds as OPEN.

**Done so far: 250/250 MAME process scenarios at code commit `3d28692`.
This does not change the strict P1.1 exit criteria.**

## 17. First-voice original IC29 ATTACK → VCF — MAME #25 verified

**MAME independent CPU probe #25 SUCCESS (code commit `1553a03`): 144/144
original ATTACK cases passed.** Existing 250/250 prior CPU scenarios
passed in the same workflow, giving **394/394 selected independent MAME
CPU process scenarios**. Native host-tests #339 and #340 also passed
on the subsequent unit-regression commit `362d537`; #26 reruns the
full MAME set on that commit.

The new `tests/cpu_ic29/run_mame_vcf_original_attack_probe.py` installs
the original published IC29 instructions **continuously** from
`0x04CB..0x063B` (369 bytes), the original `0x082F..0x083C` DAC
output helper (14 bytes), and all 128 published original attack
lookup words `0x0B60..0x0C5F` (256 bytes). It enters at `0x04D5`
and follows the original first-voice **ATTACK** branch, including
envelope addition, exact 0x3FFF clamp, phase bits, PWM DAC, global stack
handoff, ENV modulation, Key Follow and VCF final saturation.
**There is no synthetic instruction bridge within this ATTACK path.**
The harness still injects source-side cutoff/LFO/bend, initial envelope,
pitch, flags, and RAM; there is no full original firmware cold start.

Published source byte SHA-256 checkpoints:
- `0x04CB..0x063B` original 369 code bytes:
  `e3550dc3c7731630b737ede97a47d6029d33929d5b480540d876ce99c42ac365`.
- `0x082F..0x083C` original DAC helper:
  `3357c20d1baeed42f251020e287554626d94b061da5e8bd2f59e7db41c0f8310`.
- `0x0B60..0x0C5F` original 128 16-bit attack coefficients:
  `faef5ad5666a501bfe373f0af4cb345cae8ec6c569821873bb15f69f71ec3eea`.

All 128 table indices plus 16 boundary vectors run as separate MAME
processes and compare against separately compiled production
`envelope106_attack_increment`, `envelope106_attack_tick`,
`vcf106_global_raw` and `vcf106_voice_code14`. The tests also
inspect original FF27/28 envelope RAM, FF33 and FF08 phase masks, FF6A
global sign, EA, SP, HL and voice registers.

### Real discrepancy detected and fixed

**MAME #24 failed only at case 139 (zero-based 138)**, after 138
preceding matches. Exact landing `0x3F80 + 0x007F = 0x3FFF`
produced envelope level `0x3FFF` but left original IC29's FF33/FF08
phase bits **unset**. Our C `envelope106_tick` incorrectly switched
ATTACK→DECAY when level was merely equal to the limit. The original
branch sets these bits only on strict **sum > 0x3FFF**. We corrected
production `src/control106/envelope106.c`, fixed the compiled CLI to
report *phase transition* rather than merely level maximum, and added
a native regression checking equality and the next tick. In MAME #25,
that same index64 exact-boundary vector passed with ENV=0x3FFF and
SAT=0, as did all remaining boundary vectors. The correction is
evidence-led, not an unverified acoustic approximation.

### Next work; strict gate STILL OPEN

1. Exercise the original DECAY and RELEASE branches at `0x0503..0x05BF`
   with the published `0x083D` envelope multiplier helper and
   coefficient RAM, comparing per-phase level and flag evolution with
   native production C.
2. Extend from the first voice to a bounded original six-voice
   control pass, including the original per-voice pointer/stack and
   DAC path, while clearly labeling any unavailable peripheral state.
3. Validate ADC bank/IRQ entry timing, original 8253 phase and real
   module CPU variant/ROM provenance. MAME's JUNO-106 driver still
   instantiates UPD7810 rather than a proven authentic UPD7811 setup.
4. Keep `DERIVED_APPROXIMATE` pitch and DCO CV fidelity tiers until
   real independently calibrated evidence exists.

**P1.1_DIGITAL_IN_PROGRESS remains OPEN. No real original ROM image,
cycle-accurate original machine, physical FM-1 deployment or waveform
engine/audio result has been certified.**

## 18. Original DECAY/RELEASE paths in independent MAME — MAME #31 verified

**Verified MAME independent CPU #31 SUCCESS** on code commit `11634dc`:
**288/288 original DECAY/RELEASE scenarios passed** with matching
compiled production C; all six earlier MAME stages passed in that same
workflow. The independent CPU total is now **682/682 selected process
scenarios** (394 previous + 288 DECAY/RELEASE). Host-tests #351 and #350
also passed on that test-code commit.

- `tests/cpu_ic29/run_mame_vcf_original_decay_release_probe.py`:
  checks both original first-voice DECAY and RELEASE branches across all
  128 coefficient indices each, plus 32 boundary cases (**288 selected
  MAME processes**). The original 0x04CB..0x063B global/ENV/VCF code
  and 0x082F..0x083C DAC helper are byte-pinned through the prior
  first-voice ATTACK runner. The published 0x083D..0x0850 envelope
  multiplication subroutine is newly SHA-256 pinned to
  `bb924bf1d0d80ad60bc14860a2830dd2ce4cd6b9f42bad25064cb5a097319b6c`.
- `tests/cpu_ic29/envelope_decay_release_reference_cli.c` is a
  separate compiled C reference calling the actual production
  `envelope106_decay_release_coefficient`,
  `envelope106_decay_tick` or `envelope106_release_tick`.
- Each run injects the original RAM-domain current envelope, sustain,
  DECAY/RELEASE phase bits, coefficient and precomputed global CUTOFF/
  LFO/BEND, follows the **real original instructions without any
  synthetic in-envelope jump**, then compares the saved FF27/FF28
  envelope, FF33/FF08/FF07 phase flags, final VCF EA, global sign FF6A,
  stack and voice-pointer state to compiled reference.
- New workflow build target:
  `fm1juno_envelope_decay_release_reference`; new workflow stage
  follows the existing 394 independently verified scenarios.

**Strict proof boundary:** synthetic boot and initially injected
coefficient/state are not original whole-ROM execution, autonomous
coefficient lookup, complete six-voice scheduling, original IRQ phase,
nor a real UPD7811 chip. MAME #31 confirms the **original first-voice
ATTACK, DECAY and RELEASE arithmetic paths** (on their separately seeded
entry states), not natural phase transitions through an entire note life.

## 19. Six-voice original control loop — independently verified

**Achieved in a bounded input-state test.** The original published
IC29 CPU executes one uninterrupted six-voice envelope/VCF pass with
18 seeded phase-pattern scenarios (108 compared voice calculations).
The continuation below at section 20 supersedes the earlier plan.
**Focused MAME six-voice gate #2 SUCCESS** on commit `a35e621`,
and host-tests #366/#367 passed at the same code commit.

The public IC29 listing shows a natural loop after the currently
checked per-voice VCF checkpoint:

- `0x063C..0x0640`: load VCF DAC channel from FF0F and call the
  original `0x082F` DAC-output helper;
- `0x0643..0x0649`: double FF34 active-voice bit, advance FF0F voice
  pointer; these operations also advance the per-voice envelope/pitch
  cursors as side effects of the earlier `0x0590/0x05E1` code;
- `0x064B..0x064E`: decide whether all six voices are processed;
  branch back to **original 0x0503** for the next voice; 
- `0x0651`: pop global base value after the sixth voice, then
  continue into the next ADC/output control stage.

Build a separate independent MAME probe with *all six* FF27..FF32
envelope words, FF71..FF7C note-pitch words and six voice phase masks
seeded; exercise a stable family of ATTACK/DECAY/RELEASE phases.
Monitor the six VCF-DAC writes and the final FF0F/FF34/SP/DE/HL
state, and compare all six envelope and VCF values against compiled
production. The required MAME debugger capture must continue past
each 0x063C checkpoint **without rewriting original bytes**. Stop
before 0x0653 ADC scheduling until independently scoped.
Prove branch-local behavior first; do not claim full original schedule,
six-voice audio, original UPD7811 silicon or IRQ timing.

**Current gate: P1.1_DIGITAL_IN_PROGRESS remains OPEN.**

## 20. MAME original six-voice ENV/VCF loop — verified

**Focused GitHub Actions `MAME original six-voice loop gate` #2 SUCCESS
on commit `a35e621`: 18/18 complete six-voice CPU loop scenarios,
108 individual voice ENV+VCF comparisons, six VCF-DAC pre-call and
return checkpoints per loop.** No new production-engine discrepancy
was detected in these inputs. The original 682 earlier CPU processes
were independently verified by MAME #35. The combined tested repertoire
is **700 selected CPU process scenarios** (682 prior + 18 six-voice);
the full seven-stage MAME matrix plus the new eighth stage is also
being exercised under workflow #39.

The new `tests/cpu_ic29/run_mame_six_voice_original_loop.py` starts
at original `0x04D5`, executes the published original bytes through
the complete **`0x04CB..0x0651`** global VCF + six repeated
`0x0503..0x0650` envelope/VCF/DAC loop passes, and stops at
`0x0652` *after* the sixth voice's original `POP EA`.
No synthetic instruction bridge exists inside the validated control
region. There is an explicitly synthetic register/RAM/ROM bootstrap
and synthetic unexecuted stop loop at `0x0652`.

The original newly executed loop-tail bytes are **0x063C..0x0651**
(22 bytes), SHA-256
`839530afc63bf2e980c9f9d4a51df5a0527a092504ad228117f76724ce8fcf25`.
Earlier original hash checks remain in force for 0x04CB..0x063B,
0x082F..0x083C original DAC helper, 0x083D..0x0850 multiplication
helper and 0x0B60..0x0C5F attack lookup. The published disassembly is
unofficial, not independently authenticated original firmware.

There are 18 reproducible cases: all ATTACK, all DECAY, all RELEASE,
and 15 mixed six-voice phase-mask combinations, with varied pitch,
sustain, envelope levels, global cutoff/LFO/bend, ENV depth,
polarity and Key Follow. The case-level coefficient and panel values
are shared among all six voices (as they must be in the original
architecture). A synthetic boot initializes each voice's FF27..FF32
envelope words, FF71..FF7C 8.8 pitch words and FF07/08/11/33
voice masks. Expected updated envelope/VCF values come solely from
separately compiled production C (ATTACK, DECAY/RELEASE, global VCF,
per-voice VCF).

A real MAME debugger records **six** original `0x063C` VCF outputs
and **six** post-`CALF 0x082F` `0x0640` returns. It checks
individual VCF values before and after every original DAC helper,
all six resulting envelope words, phase masks, FF6A global sign,
FF0F voice index `06`, FF34 next voice mask `40`,
SP `FFF0`, HL `FF7D`, and DE `FF31` after original `0x0651`
POP EA. These are **CPU checkpoint and helper-return checks**;
they do not yet measure electrical DAC pin values or timing.

### Test-harness discrepancy fixed

The first focused MAME run #1 reached all six VCF and DAC
checkpoints with correct final pointers but marked failure because
the Python harness concatenated **the same debugger log messages**
from both stdout/stderr and MAME's `error.log`, falsely counting
12 events instead of six. Commit `a35e621` selects the authoritative
log file when it contains those markers. Focused #2 immediately passed
all 18 six-voice cases, including all 108 compiled C comparisons.
This was a harness correction, **not** a production audio/control
math rewrite.

### Next P1.1 verification work

1. Prove timing/ordering across the original loop exit
   `0x0652..` ADC processing and ISR bank changes using verified
   CPU peripheral/interrupt setup, separately from synthetic fixed
   RAM snapshots.
2. Resolve IC29 UPD7810/UPD7811 hardware variant and the real
   external/internal firmware ROM mapping. Do not call the MAME
   JUNO-106 skeleton full original Roland firmware.
3. Validate 8253 GATE/prescaler update timing, pitch/divider/DCO
   curve exactness and calibrated DAC/S&H slot measurements.
4. Keep phase #P1.1 gate open and the analogue/audio engine out
   of scope until reviewed evidence allows the next milestone.

**Current phase remains P1.1_DIGITAL_IN_PROGRESS.**

## 21. Original six-voice → selected ADC-bank service: verified in MAME

**Focused MAME six-voice ADC handoff gate #1 SUCCESS** on test-code commit
`4ab7af7` (GitHub Actions run `41931105705`): **36/36 complete
six-voice-to-ADC paths matched compiled production C**, including
**216 per-voice envelope+VCF comparisons** and **144 ADC channel
comparisons**. The older **700/700** CPU cases also passed in the
prior verified MAME suite, so the combined selected scenario repertoire
now totals **736/736 independent MAME process tests** (700 + 36).
Host-tests #376 and #377 passed on the new code commit. Full-matrix
MAME #44 was still running when this focused result was recorded.

The new `tests/cpu_ic29/run_mame_six_voice_adc_handoff.py` runs
18 original six-voice phase-mix fixtures **twice**, once per ADC
bank. Each run retains all original 0x04CB..0x0651 global/ENV/VCF
loop instructions plus the original DAC and envelope multiplier
helpers. It continues beyond the original six-voice exit into:

- Original `0x0652..0x065E` (13 bytes, SHA-256
  `6ae873b5cc79f06f63dbcad6da2515d0a7ee9f20b01e588c3cdea9494a989400`):
  `PUSH DE`, ADC raw-source pointer setup, bank bit-3
  selection from FF5C, and the bank-1 `CALF 0x0800`.
- Original `0x06E2..0x06E6` (5 bytes, SHA-256
  `381f5e495486c447526db3d58e6e8884bcabec0a48e2631290e264a809423105`):
  bank-0 held-value cursor setup and the same `CALF 0x0800`.
- The already independent checksum-validated original ADC helper
  `0x0800..0x0824` (37 bytes, SHA-256
  `2647a3e3fc0a17a43707a1edc608c25325aab61417abf45d42606dddcf2242c5`).

MAME confirms exactly **one** original ADC CALF and matching return
address per selected bank: `0x06E7` for bank 0 and `0x065F`
for bank 1. The actual CPU exits with HL=`FF61`,
DE=`FF84` or `FF88`, SP=`FFEE`, saved
envelope cursor `FF31` on the stack, and FF0F voice pointer
`06`. The six-voice intermediate VCF/DAC return checks and
phase-mask/end-pointer checks remain active; they cannot be skipped.

Each selected bank's four accepted ADC bytes (FF80..FF87) and four
conditioned bytes (FF88..FF8F) match the **separately compiled
production** `native_adc106_receive_bank` +
`native_adc106_process_latest` behavior. The unselected bank's
eight sentinel bytes remain unchanged. The original bank-select
FF5C bit 3 is set in synthetic input RAM; the test does **not**
show that a real ISR toggled it.

**Strict distinction:** This is a verified **main-loop data-path
handoff and pointer/stack continuity test**, NOT a measured latency,
automatic ADC conversion bank cadence, fired ADC ISR, full original
peripheral sequence, complete original UPD7811 ROM, or physical
DAC/audio certification. The CPU stops immediately after the original
ADC helper returns; original downstream `0x065F..` bend
calculation and `0x06E7..` portamento/LFO processing remain
unexecuted within this new gate.

### Next P1.1 bounded task

**Read `docs/PHASE1_ADC_ISR_FIDELITY_GATE.md` first.** That file pins
original ADC ISR entry/body and DI/SKIT/MKH/EI byte windows, records the
current MAME JUNO-106 analogue-input/CPU-variant gaps, and specifies
separate evidence gates for direct original opcode execution versus
true peripheral IRQ acceptance and timer fidelity.

Trace the original **interrupt-peripheral control boundary**
before asserting precise scheduling. The public listing has
`0x06B3 DI`, `0x06B4 SKIT FAD`,
`0x06B7 MVI MKH,04` and `0x06BA EI`, with the
`0x0737` envelope-cursor restore and the 0x0800 service helper.
A new independent test must explicitly establish the actual
MAME UPD7810 IRQ mask/pending flags, ADC scan bank toggle and
entry/return semantics, distinguish injected FF5C state from
hardware interrupts, and avoid guessing cycle timing from
branch order. If the stock MAME JUNO-106 skeleton lacks the
required peripheral input wiring, record a hardware/ROM
provenance **BLOCKER** instead of claiming IRQ fidelity.
Continue to treat original UPD7811 memory mapping, 8253 phase,
DCO-CV/divisor curve and hardware timing as OPEN.

**Current phase: P1.1_DIGITAL_IN_PROGRESS; PR #3 remains draft/unmerged.**

## 22. Original ADC ISR vector and internal ADC-triggered interrupt — verified

**Focused MAME original ADC ISR gate #4 SUCCESS** at code commit
`5f27ee4` (run `41933092493`) in two independently observed parts:

- **20/20 direct-entry CPU cases PASS:** synthetic boot `JMP
  0x0020` executes the original published interrupt vector and
  `0x0070..0x008C` ISR under MAME UPD7810, then stops before
  RETI at `0x008D`. Real MAME `ANM` and `CR0..CR3` register
  values are injected by explicit debugger expressions. Bank-mode
  XOR, pre-toggle FF5C RAM, four FF5D..FF60 bytes, MKH=05 and
  primary A/BC/DE/HL/SP preservation all match the source semantics.
  **Do not call a synthetic jump an actual interrupt.**
- **4/4 MAME internal-ADC-triggered IRQ/RETI round trips PASS:**
  synthetic boot sets CPU ANM/MKH/interrupt enable and waits in an
  idle loop, never jumps to the interrupt vector. MAME's *internal*
  ADC scan timer generates INTFAD; MAME accepts the IRQ at
  `0x0020`, stacks PSW/PC (`FFF0→FFED`), executes original
  published `0x0070..0x008D`, and returns through the actual
  original `RETI` to the previously interrupted main loop
  (`SP=FFF0`). ANM toggles 00↔08 and 10↔18; FF5C holds the
  pre-toggle mode, MKH=05.

Original ISR source windows pinned:
`0x0020..0x0021` checksum
`15c841dff8407a197a8be456d43283d973c0ba690724b24da5e5d0d6dbc70423`
and `0x0070..0x008D` checksum
`d4578a044eed77c142a2bb1c9f252643e4a95e15a22833a7879b7ea63551ae61`.
The source is an unofficial 4-KiB IC29 transcription, not an
authenticated complete original Roland firmware ROM. These probes
use the **MAME UPD7810** core, not verified actual UPD7811 silicon.

**New cumulative test repertoire: 760 selected independent MAME
CPU process scenarios** = 736 previously verified + 20 direct ISR
+ 4 internal-ADC-peripheral IRQ tests. The new full integrated
MAME workflow runs these last two as separate additional stages.
Native host-test status remains independent.

**Important new verified boundary:** The MAME JUNO-106 driver
does not wire AN0..AN7 to analogue input callbacks; stock MAME
returns **zero** on those open callbacks. The four actually
interrupt-driven cases verify *peripheral timer/IRQ acceptance,
CPU stack/RETI and original ISR bank/control flow* with
zero conversion input. They do **not** verify any nonzero
JUNO-106 physical knob/bender voltage, true original ADC
clock or channel sampling phase. Nor do they execute the
original post-six-voice ADC service **in the same process as
an interrupt-originated sample**. These are the next bounded
requirements; do not silently conflate separate CPU proofs.

For full source-level controls and the next test-only analogue
input fixture plan see `docs/PHASE1_ADC_ISR_FIDELITY_GATE.md`.
Preserve all existing original-ENV/VCF/ADC CPU stages.

**Gate: P1.1_DIGITAL_IN_PROGRESS remains OPEN. PR #3 remains draft.**

## 23. One CPU run: actual MAME ADC IRQ → original ISR → six-voice control → ADC bank — focused CI VERIFIED

**Focused GitHub Actions `MAME ADC IRQ to six-voice bank service` #2
SUCCESS** at code commit `592807a` (run `41976855747`).
**36/36 complete CPU paths** passed, including **216 per-voice
envelope/VCF** comparisons, six real DAC subroutine entry/returns
per process and **144 ADC-channel** comparisons against a separately
compiled C oracle. In combination with the earlier confirmed
**760/760** independent original-byte MAME CPU scenarios, the
selected scenario repertoire is **796/796** (760 + 36). The
full combined MAME matrix now also includes the new stage;
do not claim that specific run is green until verified separately.

New files:
- `tests/cpu_ic29/run_mame_adc_irq_to_six_voice_service.py`
- `tests/cpu_ic29/adc_full_bank_reference_cli.c` and CMake target
  `fm1juno_adc_full_bank_reference`
- `.github/workflows/mame-adc-irq-to-six-voice.yml` (focused CI);
  appended complete-matrix CI stage.

Every new case starts with a **real emulated internal ADC conversion
interrupt** (MAME UPD7810 raises INTFAD without debugger patching
or jumping to an interrupt vector), executes original 0x0020→0x0070
interrupt instructions and real original RETI, returns with
SP `FFF0`, and then proceeds to six **original consecutive**
0x0503 envelope/VCF calculations, six actual original DAC helper
returns, and original 0x0652/0x06E2 ADC bank-selection and 0x0800
hysteresis/conditioning instructions.

**Precisely scoped synthetic part:** This is NOT full original
interrupt-to-main-loop firmware scheduling. A short test-only polling
routine at `0x0400` waits for the ISR to write FF5C, then jumps to
published `0x04D5`. The original source instruction blocks
themselves are never modified or bridged internally, and the
published ISR is entered by genuine *MAME peripheral* interrupt
acknowledgment. All analog AN0..AN7 callbacks are still **unbound
and yield zero** in the stock MAME JUNO-106 skeleton; the new
test never modifies CR0..CR3 through the debugger. FF5D..FF60
zeros are therefore genuinely MAME converter-sourced, not
artificially placed in ISR RAM.

We validate both FF5C bank 0/1 choices for all 18 natural
six-voice ATTACK/DECAY/RELEASE patterns. The selected bank's
four original accepted/conditioned ADC bytes match production
`native_adc106_receive_bank` and
`native_adc106_process_latest` for real converter-source zeros;
the unselected bank retains independent RAM sentinels.
This extends prior proof by linking **event generation,
IRQ/RETI, CPU loop, bank selection and ADC consumption in
one MAME process** rather than separate original instruction
probes.

**Harness defect detected and corrected:** First focused CI #1
timed out because the earlier synthetic six-voice bootstrap
occupied the original 0x0020 and 0x0070 interrupt regions,
which the new probe necessarily replaces with authentic bytes.
Commit `592807a` relocated the *test-only* startup to
`0x0200`, placed a reset JMP there, and reserved the real
interrupt vector/ISR windows without overlap. Focused #2
passed 36/36 with this corrected address map. No production
control algorithm changed.

**Strict gate remains OPEN:** The current MAME driver has no
wired analog AN0..AN7 callbacks, and its machine definition
is UPD7810, not a validated Roland UPD7811/full original ROM.
Original post-interrupt scheduler cycles, ISR collision window,
ADC analogue voltage transfer and physical timer/8253 phase
remain unverified. A **test-only** MAME custom driver with
deterministic **nonzero analogue channel callbacks** and/or
hardware calibration is required before claiming input
peripheral fidelity. No time or PWM/DAC voltage measurement
is inferred from a successful program-path checkpoint.

## 24. TEST-ONLY MAME 0.285 built and executed with 8 distinct nonzero ADC inputs — VERIFIED

**GitHub Actions `MAME 0.285 compiled nonzero ADC callback gate` #3
SUCCESS**, code commit `78e1d510`, [run #41981525577](https://github.com/joakimskurk/fm1juno/actions/runs/41981525577):
**36/36 complete CPU paths PASS** for 18 six-voice patterns × both
analogue ADC channel banks, yielding **216 per-voice ENV/VCF and
144 ADC channel comparisons** against separately compiled production C.
This **closes the bounded, test-only nonzero analogue callback proof**.
It does **not** close the strict original-JUNO hardware/timing fidelity gate.

### Precisely what was built

- Exact upstream MAME 0.285 git source commit
  `f379cd060ac3b2bd567ebbd0957aed059d6895ff`;
  separate machine target, built from
  `SOURCES=src/mame/roland/juno106.cpp SUBTARGET=juno106`.
- `tests/cpu_ic29/patch_mame_juno106_analog_fixture.py`
  inserts eight **test-only deterministic** input callbacks
  `an0_func()..an7_func()` (not modelled Roland physical voltages).
  Modified `juno106.cpp` SHA-256
  `b78fe5a9323281e1fef0531b2f4b6f916d2566860608d4adec02f12211bc6c30`.
- Actual compiled, executable `mame-src/juno106` SHA-256
  `d025443aa7e3fb5275fc6f07c94b0438d51d4e4d43158d54c1f9564f08830b67`
  for this CI runner/toolchain. The complete test executes this
  path rather than the system-installed `/usr/games/mame`.
  Build environment uses Qt **5** (MAME 0.285 requirement).
- The eight independent 8-bit channels are
  AN0..AN3 = `11 2D 49 65` and
  AN4..AN7 = `81 9D B9 D5` (hex). MAME's *own*
  ADC timer reads the callbacks and raises INTFAD; no debugger
  patching of CR0..CR3 occurs. Each original ISR snapshot at
  FF5D..FF60, CPU stack/RETI, six original VCF/DAC returns,
  bank-selected original `0x0800` conditioner and unchanged
  opposite bank is checked against the compiled production reference.
- The test harness selects ANM bank **before** its longer
  synthetic RAM initialization and explicitly clears any
  preexisting FAD interrupt request before enabling IRQs.
  This was necessary because initial build #2 did compile,
  and bank 0 passed, but bank 1 originally received a stale
  conversion from bank 0. Commit `78e1d510` fixed
  **only synthetic fixture startup/IRQ sequencing**.
  The real CPU and original published code paths remain intact.
- Focused compiled gate #3, complete MAME CPU matrix #75,
  separate six-voice original/ADC/ISR gates and host-tests #439
  **all SUCCESS** on the same test-code commit.
  Former **796 selected CPU process scenarios** plus 36 new
  distinct `fixed-eight` input cases = **832/832 selected
  process scenarios verified** across these CI runs.
  This number counts invoked MAME process tests, not
  individual original ROM instructions nor a complete ROM run.

### Verified boundary and remaining work

The **real MAME UPD7810 peripheral** generated the tested
nonzero ADC conversion and interrupt, executed source-pinned
original IC29 `0x0020` ISR vector and `0x0070` handler
and returned with original RETI. A **synthetic test-only**
`0x0400` poll then dispatches to source-pinned original
six-voice control `0x04D5` and bank service
`0x0652 / 0x06E2 / 0x0800`. The original source
segments themselves are executed without bridges inside
those pinned windows.

Still **not verified**: an authenticated complete original
Roland IC29 ROM; physical **UPD7811** vs MAME UPD7810
CPU mapping/interrupt cycles; the original **unsynthesized**
interrupt-to-main-loop scheduler; 8253/DCO/GATE phasing;
hardware voltage transfer and knob calibration; DAC settling,
S/H sample slots, audible waveform, and exact hardware
real-time timing. These remain explicit P1.1 blockers.
See `docs/PHASE1_ADC_ISR_FIDELITY_GATE.md` for the
completed fixture and next proposed original-scheduler
test slice.

**Gate remains `P1.1_DIGITAL_IN_PROGRESS`; PR #3 remains draft
and unmerged. Production DSP/control sources were not changed
for this milestone.**

fm1juno_render remains a trace-only tool. Do not begin MC5534/80017A audio on this PR before reviewed P1.1 exit.
