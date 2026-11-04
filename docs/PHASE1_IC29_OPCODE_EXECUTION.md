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

These are checksums of the two short *instruction windows*, not evidence of a verified SHA-256/CRC for the complete original ROM. **No original Roland ROM binary or full disassembly is committed in fm1juno.**

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

### Limitations / open strict gate

The instruction-window interpreter is **not MAME's CPU core**. It does not currently model:

- original full-ROM cold start, interrupts and interrupt acceptance windows;
- CPU cycle timing, interrupt flags, peripherals, ADC clock conversion and autonomous scan bank switching;
- all uPD7811 PSW overlay-register behavior (the bounded ADC entry assumes L1 is clear for MVI A branches);
- the full VCF 0x05c0..0x0640 dataflow on original ROM bytes; only its final clamp PC window was executed and the remainder is covered by separate independent arithmetic oracle tests;
- instruction-level execution of DCO pitch/divisor construction, 8253 GPIO phase, live range changes or S/H aperture offsets;
- measured original-unit behavior; hardware and MAME source are secondary evidence, **not** a bit-identical chip-fidelity certification.

The next high-priority job is to **execute these same tests against a genuine separately maintained full uPD7811 core** (MAME CPU device or an independently validated equivalent) with RAM/PC injected at the routine entry, then expand from the local ADC/VCF windows to full serial/VCF execution. That step needs an appropriate host adapter and ROM provenance policy; do not imply this bounded runner already satisfies it.

## CI invocation

The standard host CMake/CTest gate remains unchanged. GitHub Actions adds, after the CTests, a byte-source download and two separate differential commands.

Given a locally provided `ic29.txt` with bytes matching the pinned checksums:

`python3 tests/cpu_ic29/run_adc_opcode_window.py --ic29-disasm PATH --native-cli build/fm1juno_native_adc_reference`

`python3 tests/cpu_ic29/run_vcf_opcode_window.py --ic29-disasm PATH --native-cli build/fm1juno_vcf_clamp_reference`

The download is an explicit CI dependency, not a hidden fallback to guessed ROM contents. The public disassembly's author explicitly describes it as partial/inaccurate/incomplete; therefore even a verified checksum only authenticates that *published transcription*, not an original Roland chip dump. A changed/unavailable source fails this extra gate. When working offline, run the unchanged CTest suite and pass a separately obtained verified original disassembly file for the opcode gate.

**P1.1_DIGITAL_IN_PROGRESS remains the only valid phase status. No audio/WAV or original Roland unit calibration claims.**
