# P1.1 — Original IC29 opcode-window execution (ADC and VCF)

Date: 2026-11-04
State: **executed original published opcode bytes in bounded, independently coded NEC-uPD7811 instruction-window interpreters; original whole-CPU MAME/cycle-accurate execution STILL OPEN**.

## What was run, and what was not

**Actually run:** the NEC uPD7811 *machine opcodes* as published at the selected addresses in the public Roland IC29 module firmware disassembly, not merely an oracle built from the source's comments or mathematical formulas.

**Not run:** the full 8KB JUNO-106 module ROM under MAME's whole uPD7811 CPU core. The current runners are small CPU decoders for the relevant original instruction *windows*, covering their real register/memory effects and conditional-skip branch instructions, tested separately against compiled host production functions. The runners are original project test code rather than a rehosted MAME core. Never label these as equivalent to cycle-exact, fully independent hardware execution.

Primary source, externally retrieved at test time:
https://github.com/ErroneousBosh/j106roms/blob/master/ic29.txt

Instruction semantics were checked against MAME's independently maintained BSD-3-Clause NEC CPU implementation:
https://github.com/mamedev/mame/blob/master/src/devices/cpu/upd7810/upd7810_opcodes.cpp

For historical context, the existing MAME JUNO-106 driver remains a **skeleton** marked NOT_WORKING and MACHINE_NO_SOUND:
https://github.com/mamedev/mame/blob/master/src/mame/roland/juno106.cpp

## Reproducible byte provenance / no original ROM vendoring

The GitHub Actions workflow downloads the public IC29 annotated disassembly to a temporary build directory. Test scripts **parse the bytes at their original program-counter addresses**, not the mnemonic names. They refuse to execute if the byte window is incomplete or the fixed digest is different.

| Window | Purpose | SHA-256 of **actual extracted bytes** |
|---|---|---|
| 0x0800..0x0824 inclusive | Four-channel ADC filter and correction | `2647a3e3fc0a17a43707a1edc608c25325aab61417abf45d42606dddcf2242c5` |
| 0x0609..0x0618 inclusive | VCF final 16-bit over-/underflow saturation | `998e3790863ac4f1aca79f78c7bec54de371fdd14ab11aafa24c3ae480fd86a8` |
| 0x05C1..0x063B inclusive | Per-voice VCF ENV scaling, polarity, Key Follow and final clamp (MAME only) | `3b77378007f1fb96c884d707263af7a5cf9984dbe6df00b86d6cdf62c979be04` |
| 0x04CB..0x04F5 inclusive | Global VCF cutoff + precomputed LFO + bend signed selection and FF6A carry/borrow (MAME only, new probe) | `08a0491367f316ed72992cfe3ecb2cef241efcdeb38b292c0b8eaa58b721ffc2` |
| 0x04CB..0x0502 and 0x05C0..0x063B inclusive (concatenated) | Linked global-to-one-voice VCF, synthetic bridge at 0x0503; original segments only | `545631d0b7a24a39babc7e1b95dbf29be2c2a042b77e1d1a0fad382d6df89ce5` |

These are checksums of four bounded *instruction ranges* (the VCF voice path overlaps the clamp), not evidence of a verified SHA-256/CRC for the complete original ROM. **No original Roland ROM binary or full disassembly is committed in fm1juno.**

### Tests and actual execution

- `tests/cpu_ic29/run_adc_opcode_window.py`: narrow instruction decoder for the window with real PC changes, `HL/DE`, register A/B/C operations, skip-one-next-instruction semantics (including two-byte prefixes), bounded stack/RET, byte-addressed RAM, and independent opcode-length handling. Starts with the original ADC raw-register/processed-register bank layout.
- `tests/cpu_ic29/native_adc_reference_cli.c`: test-only executable **linked to the real compiled `fm1juno_core`**. It accepts 256×256 old-accepted/new ADC pairs and emits four pairs of held/conditioned channel bytes, with alternating edge cases for the remaining three channels. This checks the loop, memory address increment and `DE+8` write offset, not merely channel 0.
- `tests/cpu_ic29/run_vcf_opcode_window.py`: executes the original VCF clamp PC flow with real `EA` register, `MOV A,EAH`, `ONI`, `ONIW`, `LXI EA` and `JRE` encodings. It tests all 65,536 EA register words and all 4 combinations of flag bit 0 plus an unrelated flag bit: **262,144 opcode cases**.
- `tests/cpu_ic29/vcf_clamp_reference_cli.c`: real compiled production VCF function, invoked with a signed global intermediate and no ENV/Key Follow so final clamp behavior can be isolated. **229,376** reachable signed-input cases are compared directly. Cases with EA in 0..0x3fff but underflow flag set are executed by the opcode runner but cannot be represented as that same signed intermediate (they would require an unreachable out-of-range negative number), so **those are not falsely advertised as production comparisons**.

Both opcode interpreters enforce a narrow instruction allowlist: unknown branch targets, unknown opcodes, bad RET, incomplete source text, source hash changes and test mismatches are hard failures.

### Verified CI runs

- **#256:** first ADC opcode-runner; 5,054,976 interpreted instruction steps; all 65,536 channel-0 history comparisons passed. All 18 existing CTests passed.
- **#264:** original VCF clamp opcode-runner passed 262,144 EA/flag combinations, including 229,376 comparisons against compiled production C. All 18 existing CTests passed.
- Following #264, ADC comparison was extended to **all four channels**, still running all 65,536 combinations of old/new channel-0 history; count now represents 65,536 × 4 channel output comparisons. **CI #268 passed** with the new four-channel comparison and both opcode windows. Existing CTest: 18/18; ADC: 65,536 × 4 channel pairs; VCF: 262,144 opcode cases / 229,376 compiled-production comparisons.

### Independent MAME CPU core execution — verified 2026-11-04

**This is a different layer from our local bounded interpreter.** GitHub Actions installs the real **MAME v0.285** Ubuntu package, starts its existing JUNO-106 driver with the standalone **uPD7810 CPU device** for the module controller, and runs three bounded published machine-opcode probes in that independently maintained CPU implementation.

Test-only harness, with **no genuine Roland ROM distributed or claimed**:

- Tests create 8192-byte synthetic assigner and module ROM files in an isolated temporary directory. A deliberately small synthetic uPD7810 bootstrap sets the CPU V page, stack, HL/DE or EA, relevant RAM operands and then calls/jumps into the original disassembly-extracted, SHA-256-verified IC29 instruction bytes.
- MAME correctly reports ROM **checksum mismatches** because these fixtures are synthetic. That warning is expected, never suppressed or treated as validation of original full-chip firmware.
- A debugger breakpoint after the original instruction window logs actual MAME CPU memory/register values via debugger logerror. MAME's supported Qt debugger runs with QT_QPA_PLATFORM=offscreen. The subprocess must exit successfully **and** produce the tagged breakpoint result; a missing trace is a test failure.
- The debugger script uses MAME's supported CPU breakpoints and memory/register expressions. No original instrument software audio or patches are run.
- The output is independently compared against the **compiled project production C** (not a Python copy of its formulas). Original public opcodes are separately hashed and checked by the earlier window runner.

**Actual executed samples:**

| MAME module CPU | Inputs actually executed | Compare target | Outcome |
|---|---:|---|---|
| IC29 ADC $0800..$0824 | 28 selected previous/current ADC combinations, **all four channels** | compiled native_adc106 held + conditioned RAM (8 values per run) | **28/28 PASS** |
| IC29 VCF clamp $0609..$0618 | 13 EA boundary words × all 4 combinations of FF6B low flag bits = **52 runs** | independently compiled vcf106_voice_code14 plus existing instruction-window oracle | **52/52 PASS** |
| IC29 VCF voice path $05C1..$063B | 42 signed base/ENV/pitch/depth/polarity/Key Follow cases | independently compiled vcf106_voice_code14 with nonzero ENV and Key Follow | **42/42 PASS** |

**Verified runs:**
- **MAME CPU probe #2:** the first 28 ADC scenarios passed, after fixing an initial unsupported console-debugger option.
- **MAME CPU probe #4:** full two-stage gate passed **28 ADC + 52 VCF cases**, with all 4 ADC-channel bytes checked per case. MAME package: **v0.285**.
- **MAME CPU probe #11** (commit `5c6ffc7`): all three stages passed **28 ADC + 52 VCF clamp + 42 VCF voice path = 122/122 independent MAME process cases**. The new voice-path stage verifies 123 pinned published instruction bytes from `$05C1` to `$063B` including ENV and Key Follow, exercising both signs and final clamp. It starts at `$05C1` with synthetic input registers, stack, RAM and global sign; it does **not** execute the original preceding ENV update or global cutoff/LFO/bend generation. The run passed with no production VCF code changes.
- In parallel **host-tests #286** passed **18/18 CTests** plus the existing exhaustive original-byte tests (65,536 old/new ADC pairs x four channels and 262,144 wrapped VCF clamp flag-state cases). **Host-tests #302** also passed 18/18 on the new chain-test commit.

Files:
- tests/cpu_ic29/run_mame_cpu_probe.py — actual MAME ADC core subprocess + synthetic fixtures and PC/RAM capture;
- tests/cpu_ic29/run_mame_vcf_probe.py — actual MAME VCF EA/FF6B capture;
- tests/cpu_ic29/run_mame_vcf_chain_probe.py and vcf_chain_reference_cli.c — new whole-CPU execution of the per-voice VCF arithmetic slice, with SHA-256 source pin and compiled production C comparison;
- .github/workflows/mame-independent-cpu-probe.yml — separate, path-gated PR/dispatch workflow; ordinary host-tests stay lightweight.

**Exactness boundary:** MAME's JUNO-106 skeleton driver currently uses UPD7810 devices, not UPD7811, although the relevant machine instructions are shared. Our separate *full MAME CPU execution of these windows* therefore constitutes independent opcode / register-state evidence, but not an 8-KiB original full-firmware run on correctly configured uPD7811 silicon. Nor does it independently prove ADC conversion clocks, UART ISR races, exact CPU-cycle latency or analogue sample-and-hold timing.

### Global VCF MAME probe — verified

`run_mame_vcf_global_probe.py` executes 43 original disassembly bytes
`0x04CB..0x04F5`, entering at `0x04D5` and stopping before the
`0x04F6` stack-push. Original relative branch destinations within
this window are not reinterpreted by a project CPU: MAME's maintained
UPD7810 core executes the bytes. Synthetic RAM supplies the original
cutoff, precomputed LFO contribution, precomputed bend and sign flags;
the original code must produce the expected wrapped EA and FF6A sign
selector. A compiled `vcf_global_reference_cli.c` calls
`vcf106_global_raw` in production C to obtain expected full signed
results and isolated positive input components. The 64 stable cases
exercise all polarity combinations, near-zero crossings and boundaries.

The probe does not validate source-side LFO/bend construction,
original CPU interrupts, uPD7811 internal ROM, full 8 KiB code, or
whole scheduler continuity. **MAME CPU probe #18 SUCCESS:**
64/64 compiled-production comparisons passed.

**uPD7811 mapping hazard:** MAME's UPD7811 device maps 4 KiB
internal program ROM while its JUNO-106 skeleton driver explicitly uses
UPD7810 and an 8 KiB external module ROM image. An UPD7811 test
requires a validated chip/ROM provenance and correct memory mapping,
not just a device-name swap.

### Global-to-voice linked execution — MAME CI #19 VERIFIED

`run_mame_vcf_linked_probe.py` uses 64 deterministic initial states and
launches each in one actual MAME UPD7810 process. The synthetic setup
loads the already computed cutoff, LFO contribution, bend contribution,
precomputed 14-bit envelope and pitch into CPU RAM, then enters the
published IC29 path at `0x04D5`. MAME executes original bytes covering:

1. `0x04CB..0x0502` (56 contiguous source bytes), including sign-based
   global VCF add/sub, original FF6A overflow state and the global EA
   stack save + setup for the first voice.
2. `0x05C0..0x063B` (124 contiguous source bytes), including original
   `POP HL`, transfer FF6A→FF6B, envelope and Key Follow scaling,
   polarity selection, final clamp and 16-bit original-CPU arithmetic.

**The two regions are *not* unmodified contiguous firmware execution.**
At `0x0503` a deliberate, synthetic seven-byte bridge executes
`PUSH HL; LXI DE,FF27; JMP 05C0` and bypasses the original
`0x0503..0x05BF` envelope evolution. This retains the stack protocol
and correct post-envelope DE cursor but injects the already computed
envelope instead of calculating it from the original program. The bridge
is kept distinct from the 180 pinned original source bytes.
A MAME debugger breakpoint at `0x063C` captures EA, FF6A/FF6B, SP
and HL, checks against compiled production C and demands successful
process termination. This independently exercises global→voice state
handoff without claiming the full Roland program or autonomous
interrupts. The comparison reuses existing `vcf_global_reference_cli`
and `vcf_chain_reference_cli` C executables rather than adding a
Python clone of production signal arithmetic.

**Verified GitHub Actions MAME CPU probe #19** on commit `3d28692`:
all **64/64 linked CPU scenarios PASS**; terminal CI result **SUCCESS**.
Real MAME EA, FF6A, FF6B, SP and HL checks completed. The four
preceding stages also succeeded (28 ADC + 52 clamp + 42 voice + 64 global),
so **250/250 selected independent MAME CPU process scenarios** passed.
This tally is a sum of distinct synthetic test invocations, not an assertion
of complete instruction or full original firmware coverage. Host-tests
#321 also passed 18/18 on the same code commit. The new test's synthetic
bridge still prevents any claim of uninterrupted original ROM/ENV/IRQ timing.

### Original first-voice ATTACK → VCF: 144/144 MAME #25 SUCCESS

The **new** `run_mame_vcf_original_attack_probe.py` is deliberately
distinct from the prior 64-case *synthetic bridge* test: it installs
the original contiguous published IC29 opcode bytes from
`0x04CB..0x063B` and executes the **actual** original first-voice
ATTACK path through `0x0503..0x05BF` before naturally reaching
`0x05C0..0x063B`. No synthetic jump bypasses the envelope for this
ATTACK path. The test also installs the original `CALF 0x082F`
DAC-output helper and all 128 original ATTACK lookup coefficients.

Byte-source SHA-256, pinned and checked at test time:
- Full original `0x04CB..0x063B` 369-byte window:
  `e3550dc3c7731630b737ede97a47d6029d33929d5b480540d876ce99c42ac365`.
- Original `0x082F..0x083C` 14-byte DAC helper:
  `3357c20d1baeed42f251020e287554626d94b061da5e8bd2f59e7db41c0f8310`.
- Original `0x0B60..0x0C5F` 256-byte, 128-word ATTACK lookup table:
  `faef5ad5666a501bfe373f0af4cb345cae8ec6c569821873bb15f69f71ec3eea`.

These are checksums of the **published transcription** only, not
proof of a recovered full Roland firmware image. The test boot is
synthetic, initializing the original code's cutoff, precomputed LFO,
bend, voice active/phase flags, initial 14-bit envelope, pitch, ENV
depth, Key Follow, PWM value and stack. MAME's real UPD7810 CPU
executes the original first-voice ATTACK, 0x3FFF ceiling/phase
conditions, real DAC helper and VCF instructions with no debugger
register edits. At `0x063C`, the debugger reads EA, envelope RAM
FF27/FF28, phase masks FF33/FF08, FF6A, SP, HL and voice state.
It compares output against **separately compiled production C** for
global VCF, attack increments/tick, and final VCF.

**First attempt found a real fidelity bug:** MAME #24 failed at
scenario index138 (139th test): `0x3F80+0x007F=0x3FFF` set no decay
bits in published original IC29 path but C advanced immediately to
DECAY. The original flag branch is strict `sum > 0x3FFF`, not merely
`sum >= 0x3FFF`. We changed `envelope106_tick`'s phase transition
check and its compiled reference CLI, and added a native unit
regression. **MAME independent CPU #25 SUCCESS:** all 128 ATTACK
indices plus 16 boundary cases pass (**144/144**), in addition to all
250 earlier MAME cases (**394/394 selected independent process runs**).
Native host-tests #339/#340 passed on the unit-regression code commit.

The original DECAY and RELEASE branches, natural six-voice loop, full
module ROM cold boot, original UPD7811 device, interrupt scheduling,
ADC conversion phase and analogue audio remain **unverified**.

### DECAY/RELEASE execution via original 083D multiplier — verified in MAME #31

New `run_mame_vcf_original_decay_release_probe.py` executes both
first-voice DECAY and RELEASE opcode branches without bypassing the
original envelope section at `0x0503..0x05BF`. In addition to the
already source-pinned continuous `0x04CB..0x063B` firmware slice
and original DAC helper `0x082F..0x083C`, it now executes the
unmodified 20-byte original `CALF 0x083D` multiply routine
(`0x083D..0x0850`), pinned by SHA-256
`bb924bf1d0d80ad60bc14860a2830dd2ce4cd6b9f42bad25064cb5a097319b6c`.

The compiled `envelope_decay_release_reference_cli.c` calls
production C decay/release coefficient and tick functions; injected
initial-state RAM supplies the 16-bit coefficient and sustain state.
MAME performs the original branching, intermediate integer
multiplication, phase-mask updates, RAM save, original DAC call
and global-to-per-voice VCF arithmetic. A real debugger checkpoint
reads FF27/28 envelope, FF07/08/33 phase bytes, EA, FF6A and
stack/pitch/voice registers. The 288 deterministic MAME cases cover
128 original coefficient indices for each path plus 32 boundary cases.

**MAME CPU #31 SUCCESS on commit `11634dc`: 288/288 DECAY/RELEASE
process scenarios passed.** Each coefficient index (0..127) was verified
on both branch types, plus 32 explicitly selected edge conditions;
all 250 earlier ADC/VCF and 144 original ATTACK CPU tests also passed
in the same workflow. **682/682** selected original-byte CPU executions
passed in total, not 682 independent original instructions or full ROM.

The test does not model original parameter generation, serial/ADC IRQ
schedule, six-voice natural loop, authentic UPD7811 ROM or analogue
audio. The original 083D multiplier is now directly verified under
the MAME UPD7810 driver for these input states.

### Limitations / open strict gate

The **earlier bounded test interpreter** is not MAME's CPU core. The new MAME harness above now executes both windows using independently maintained MAME CPU code. However neither harness currently models/validates:

- original full-ROM cold start, interrupts and interrupt acceptance windows;
- CPU cycle timing, interrupt flags, peripherals, ADC clock conversion and autonomous scan bank switching;
- all uPD7811 PSW overlay-register behavior (the bounded ADC entry assumes L1 is clear for MVI A branches);
- original source-side generation of LFO/bend contributions, unverified DECAY/RELEASE envelope branches, natural six-voice iteration and post-0x063C DAC/scheduler path; the original first-voice ATTACK/global-VCF/ENV/Key Follow path **has** now executed continuously in the independent MAME CPU, albeit with synthetic initial inputs;
- instruction-level execution of DCO pitch/divisor construction, 8253 GPIO phase, live range changes or S/H aperture offsets;
- measured original-unit behavior; hardware and MAME source are secondary evidence, **not** a bit-identical chip-fidelity certification.

The next high-priority job is to run more original-byte control flow under an **explicit UPD7811-configured CPU device** with a separately validated original ROM provenance, then extend beyond injected ADC/VCF window entries to original-entry serial/global-VCF flow and timing/IRQ cases. The current independent MAME CPU probes are valuable verified partial evidence; do not reclassify them as full Roland CPU or full original ROM fidelity.

## CI invocation

The standard host CMake/CTest gate remains unchanged. GitHub Actions adds, after the CTests, a byte-source download and two bounded differential commands; the independent MAME CPU checks live in a separate path-gated workflow.

Given a locally provided `ic29.txt` with bytes matching the pinned checksums:

`python3 tests/cpu_ic29/run_adc_opcode_window.py --ic29-disasm PATH --native-cli build/fm1juno_native_adc_reference`

`python3 tests/cpu_ic29/run_vcf_opcode_window.py --ic29-disasm PATH --native-cli build/fm1juno_vcf_clamp_reference`

Independent MAME invocation (Ubuntu package version 0.285) is reproduced by the same dedicated workflow, after building fm1juno_native_adc_reference, fm1juno_vcf_clamp_reference and fm1juno_vcf_chain_reference. The synthetic ROM generator refuses unpinned source bytes and validates a debugger checkpoint before allowing PASS.

The download is an explicit CI dependency, not a hidden fallback to guessed ROM contents. The public disassembly's author explicitly describes it as partial/inaccurate/incomplete; therefore even a verified checksum only authenticates that *published transcription*, not an original Roland chip dump. A changed/unavailable source fails this extra gate. When working offline, run the unchanged CTest suite and pass a separately obtained verified original disassembly file for the opcode gate.

**P1.1_DIGITAL_IN_PROGRESS remains the only valid phase status. No audio/WAV or original Roland unit calibration claims.**
