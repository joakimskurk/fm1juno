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

### Original natural six-voice ENV/VCF loop — focused MAME #2 SUCCESS

`tests/cpu_ic29/run_mame_six_voice_original_loop.py` now performs
18 original-byte six-voice *loop* runs (108 per-voice arithmetic
comparisons) with the MAME JUNO-106 UPD7810 core. Source is still
the independently fetched, checksum-pinned **unofficial published**
IC29 disassembly; no genuine 8 KiB Roland firmware has been
distributed or authenticated.

Source windows:
- Existing SHA-256 checks of original `0x04CB..0x063B` (369 bytes),
  real `0x082F..0x083C` DAC helper and
  `0x083D..0x0850` decay/release multiplication helper, plus
  128 16-bit `0x0B60..0x0C5F` ATTACK table words.
- **New** original `0x063C..0x0651` (22 bytes) covering final
  VCF DAC, active bit shift, incrementing FF0F, six-voice
  compare/jump back to 0x0503 and terminal original POP EA.
  SHA-256:
  `839530afc63bf2e980c9f9d4a51df5a0527a092504ad228117f76724ce8fcf25`.

Each case executes the global VCF path at `0x04D5`, followed by
**six actual original envelope/VCF paths within one CPU run**,
interleaving ATTACK, DECAY and RELEASE across six voices. Initial
FF07/FF08/FF11/FF33 bit masks, each FF27..FF32 envelope and
FF71..FF7C pitch, global coefficient/panel values, LFO/bend
contributions, stack and boot ROM are explicitly synthetic
fixture state, not an authentic original power-on event.

The debugger captures 6 original `0x063C` per-voice EA outputs and
6 original `0x0640` post-DAC helper returns; then it observes at
`0x0652` the RAM envelope array, phase masks, FF6A flag,
FF0F=`06`, FF34=`40`, SP=`FFF0`, HL=`FF7D`,
DE=`FF31`. The expected ENV/VCF output is computed by
separately compiled *production* ATTACK, DECAY/RELEASE and VCF C
functions. The test requires exactly six matched outputs and six
matched helper returns per scenario, preventing a simple six-fold
repetition of independent first-voice fixtures.

**GitHub Actions focused six-voice gate #2 SUCCESS** at commit
`a35e621`: **18/18 full CPU scenarios and 108/108 voice
output comparisons pass**. The first run failed because the harness
combined duplicated MAME debugger messages from console and
`error.log`. Fixing the capture to use one authoritative stream
produced a green run without modifying production DSP/control math.
Together with the preceding 682 separately verified CPU cases,
this constitutes **700 selected MAME process scenarios**, not a
claim of 700 unique opcode instructions, full original hardware
timing, genuine UPD7811 memory layout or DAC electrical proof.

### Six-voice exit into original bank-selected ADC helper — verified

**Focused GitHub Actions MAME six-voice ADC handoff gate #1 SUCCESS**
on commit `4ab7af7`: **36/36 bounded complete CPU paths PASS**.
All 36 also re-compare all six prior original ENV/VCF calculations
(216 individual voice comparisons), six corresponding VCF DAC
helper returns per process, original voice-mask/stack exit, and
four native ADC conditioner outputs per process
(144 accepted/conditioned ADC-channel comparisons).

The source-pinned **original** path is:

- Six-voice code from `0x04D5` through `0x0651`, including
  six natural repeats of the original `0x0503..0x0650`
  envelope/VCF loop;
- `0x0652..0x065E` (13 bytes, SHA-256
  `6ae873b5cc79f06f63dbcad6da2515d0a7ee9f20b01e588c3cdea9494a989400`):
  original `PUSH DE`, `HL=FF5D`,
  `BIT 3,FF5C` and branch to the selected channel bank,
  with the bank-1 `CALF 0x0800`;
- `0x06E2..0x06E6` (5 bytes, SHA-256
  `381f5e495486c447526db3d58e6e8884bcabec0a48e2631290e264a809423105`):
  alternate bank-0 cursor and original `CALF 0x0800`;
- shared `0x0800..0x0824` original ADC compare/hysteresis
  and output conditioning helper (37 bytes, SHA-256
  `2647a3e3fc0a17a43707a1edc608c25325aab61417abf45d42606dddcf2242c5`).

The MAME CPU follows each original bank-selection branch once
per fixture, entering exactly one CALF and returning naturally
to `0x06E7` for bank 0 or `0x065F` for bank 1.
A breakpoint immediately after the call observes correct
FF5C bank-selection, SP=`FFEE`, HL=`FF61`,
DE=`FF84` (bank 0) or `FF88` (bank 1),
voice pointer `FF0F=06`, and preserved saved envelope pointer
`FF31` on the original CPU stack. Both banks' accepted
FF80..FF87 and conditioned FF88..FF8F bytes are inspected.
Only the selected bank's four held and four conditioned
bytes change; the unselected bank's sentinels survive.

The *expected* values are obtained from an externally compiled
`fm1juno_native_adc_reference` program calling actual
production `native_adc106_receive_bank` and
`native_adc106_process_latest`. No custom Python clone
of the production ADC transfer law determines PASS/FAIL.
The 36 processes exercise both original bank paths for all
18 six-voice phase patterns. Combined with the prior
700 selected independent MAME scenarios, **736/736**
bounded CPU process scenarios have now passed across
the two distinct verified test stages.

**Scope boundary:** The bank-selector FF5C and all raw ADC
channels are synthetic input registers. This does *not*
execute a real ADC ISR, generate automatic bank alternation,
validate `DI/SKIT FAD/MKH/EI` hardware state or original
interrupt timing. The original post-service bend
(`0x065F..`) and portamento/LFO (`0x06E7..`) paths
remain unexecuted. MAME's existing JUNO-106 CPU is
UPD7810, *not* proven original UPD7811 silicon/full ROM.

### Original 0020 ADC interrupt vector + 0070 ISR: direct and peripheral paths VERIFIED

**Focused MAME original ADC ISR direct-opcode gate #4 SUCCESS**
(code commit `5f27ee4`, GitHub Actions `41933092493`).
The published source checksums are validated on every run:
- `0x0020..0x0021` two-byte interrupt vector JRE,
  SHA-256 `15c841dff8407a197a8be456d43283d973c0ba690724b24da5e5d0d6dbc70423`;
- `0x0070..0x008D` 30-byte original ADC ISR, SHA-256
  `d4578a044eed77c142a2bb1c9f252643e4a95e15a22833a7879b7ea63551ae61`.

Two independent probes now guard a **critical distinction**:

1. `tests/cpu_ic29/run_mame_adc_isr_direct_probe.py`:
   **20/20 CPU case PASS**. A synthetic `JMP 0020`
   follows the original source vector and ISR opcodes with MAME
   debugger-controlled, nonzero ANM/CR0/CR1/CR2/CR3 inputs.
   Checks primary A/BC/DE/HL/SP restoration by `EXA/EXX`,
   FF5C **pre-toggle** ANM, original ANM XOR 08, FF5D..FF60
   raw ADC register copy and MKH=05. Stops at `008D` *before
   RETI*; there was no hardware event and no claim of interrupt
   latency. MAME debugger integer values require explicit
   hexadecimal `0xNN` to avoid radix-induced fixture drift.
2. `tests/cpu_ic29/run_mame_adc_isr_peripheral_probe.py`:
   **4/4 independently generated MAME internal-ADC IRQ/RETI
   cases PASS**. Startup enables MAME UPD7810's
   actual ANM scan, MKH=04 and global interrupt enable, then
   executes an idle self-JMP `0x0010`. It never jumps to
   original vector directly. MAME ADC conversion timers
   generate INTFAD, then the CPU accepts vector `0020`
   with SP `FFF0→FFED` and runs original ISR, including
   `RETI` back to idle loop at SP=`FFF0`.
   All four original scan modes 00/08/10/18 pass; the ISR
   records FF5C=old ANM, toggles bit3 and masks MKH=05.

The stock MAME JUNO-106 machine driver has **no analogue
AN0..AN7 callback wiring**; the built-in CPU callback defaults
to reading zero. Consequently the four peripheral IRQ tests
capture CR0..CR3 all zero. This independently proves the
**emulated CPU's IRQ/stack/RETI integration with the original
ISR opcodes**, not the original JUNO-106 hardware's
analogue conversion voltages/timing or UPD7811 memory map.
The 20 direct-entry tests cover distinct nonzero ADC values
but do not fire a real interrupt.

Selected MAME CPU process repertoire: **760 scenarios**
(736 prior, plus 20 direct original ISR and four genuine
MAME-internal ADC interrupt round trips). The integrated
full-matrix workflow now includes these two stages.
Next phase requires deterministic test-only nonzero analogue
AN0..AN7 callbacks, true IRQ-produced FF5C/CR inputs
followed by original main-loop conditioning in one continuous
CPU pass, CPU-variant/ROM provenance and real timing evidence.
`docs/PHASE1_ADC_ISR_FIDELITY_GATE.md` is the living handoff.

### Causal MAME ADC timer → original interrupt → natural six-voice → ADC conditioner (focused #2)

**Focused GitHub Actions `MAME ADC IRQ to six-voice bank service`
#2 SUCCESS** at test-code commit `592807a`, run `41976855747`:
**36/36 independent MAME CPU process scenarios passed**,
each with an actual MAME UPD7810 internal ADC converter-generated
interrupt followed by original interrupt vector/handler
(`0x0020`, `0x0070..0x008D`) and original RETI,
followed in the *same* process by six original ENV/VCF
passes and the selected original ADC bank conditioner
(`0x0652`, `0x06E2`, `0x0800`).

The test checks original IRQ entry SP=`FFED`, MKH,
pre-toggle ANM captured as FF5C, four ISR-published raw
register bytes FF5D..FF60, real RETI restoration SP=`FFF0`,
the per-voice ENV/VCF and DAC helper checkpoints,
the original main-loop ADC CALF/return, and all eight
accepted/conditioned RAM bytes in both banks. Separately
compiled `fm1juno_adc_full_bank_reference` calls
actual production `native_adc106_receive_bank` and
`native_adc106_process_latest`; it does not reproduce
the ADC law in Python. 216 individual voice and 144
ADC-channel comparisons pass. Combined with the
previous verified 760 scenarios, this is **796 selected
independent process runs**.

**The scheduling handoff is still SYNTHETIC:** Once
original RETI returns to the interrupted polling loop,
a test-only `0x0400` conditional check on ISR-written
FF5C jumps to the authentic `0x04D5` global-VCF entry.
The original six-voice and ADC-service code itself is
continuous/unmodified. To avoid overwriting original
`0x0020` and `0x0070` IRQ instructions, the
test-only bootstrap was relocated to `0x0200`;
this fixed an initial test-harness timeout.

**At the time of this earlier stock-MAME run, ADC
samples were zero** because upstream Juno-106 leaves
`an0_func()..an7_func()` unbound. The subsequent
separately compiled test-only machine verified the
full original ISR/bank data path with eight distinct
nonzero callback-sourced values (details below).
Neither test proves original Roland input-voltage
calibration or exact physical timing.
No audio or wave engine functionality follows from
this check. See `docs/PHASE1_ADC_ISR_FIDELITY_GATE.md`.

### Verified compiled nonzero MAME ADC callback source through authentic ISR and original downstream control

**Compiled test-only MAME 0.285 `juno106` #3 SUCCESS** (GitHub
Actions run `41981525577`, test-code commit `78e1d510`):
36/36 complete CPU process fixtures pass; **216 ENV/VCF voice
and 144 ADC channel comparisons** match separately compiled
production C. Actual MAME-build SHA-256:
`d025443aa7e3fb5275fc6f07c94b0438d51d4e4d43158d54c1f9564f08830b67`;
pinned MAME source commit
`f379cd060ac3b2bd567ebbd0957aed059d6895ff`.

Unlike stock JUNO-106 MAME, this *test-only* machine has eight
`an0_func()..an7_func()` callbacks with distinct fixed **nonzero**
ADC samples: `11 2D 49 65` (bank 0), `81 9D B9 D5`
(bank 1). The real emulated ADC converter reads them into CR0..CR3
and raises INTFAD; published original CPU bytes execute the
`0x0020→0x0070` ISR/RETI and publish the pre-toggle ANM
to FF5C and raw four bytes to FF5D..FF60. Synthetic fixture
code at `0x0400` then dispatches to source-pinned
original `0x04D5` six-voice control, original DAC helper,
and selected original `0x0652/0x06E2→0x0800` conditioner.
MAME matches **six original ENV/VCF and four ADC channels**
per process, and the unselected bank remains unchanged.

The first compiled run revealed an **initialization/interrupt
race** in the test harness: bank 1 initially processed CR
values captured while the ADC still scanned bank 0.
Setting ANM before the longer synthetic startup and clearing
stale pending FAD before enabling interrupts corrected
the fixture; no published IC29 or production-engine
algorithm was changed. The stock-MAME `fixed-eight`
fail-closed negative control is separately verified by
the focused source/stock gate #11.

This adds **36 input-distinct compiled-MAME process scenarios**
to the already verified 796 selected original-byte
MAME processes: **832/832 selected tests PASS** across
the respective green CI runs. This is **not**
832 unique opcodes, an original full ROM, calibrated
hardware analog voltages or a source-reconstructed
JUNO scheduler. See `docs/PHASE1_ADC_ISR_FIDELITY_GATE.md`.

### Original 065F..0737 ADC follow-on, 06B3 FAD mask, 0737 POP — bounded MAME VERIFIED

The source-pinned [MAME original post-ADC FAD gate #5](https://github.com/joakimskurk/fm1juno/actions/runs/42046688020)
is **36/36 PASS**, with independent full CPU matrix
[#87 SUCCESS](https://github.com/joakimskurk/fm1juno/actions/runs/42047063456).
It extends each original ADC peripheral IRQ→ISR/RETI→
six-voice→bank conditioner run through **original**
`065F..0737` and an authentic original 128-byte
`0A00` portamento coefficient table.

Byte fingerprints: `065F..0737` SHA-256
`b9a57a701bb31daeecc064843efc8db94faacfba27c6649b0820eb45dc514bb2`;
`0A00..0A7F` SHA-256
`06d1c862622b5aaa2b7e42d561dbdf2cd424620a8e46cfa0c2c9deb5c484984e`.

Unmodified original `06B3 DI`, `06B4 SKIT FAD`,
`06B7 MVI MKH,04`, `06BA EI`,
`06BB JRE 0737` and `0737 POP DE`
execute naturally after the original ADC service.
The MAME debugger checks SP=`FFEE` with the original
envelope pointer still pushed, and final
SP=`FFF0`, DE=`FF31`. Across 36 input/phase
fixtures, 28 show no additional IRQ, six observe a
second real ADC IRQ while SP is FFEE (IRQ frame FFEB),
and two observe an IRQ after the POP restored SP to
FFF0 (IRQ frame FFED). Original alternate-bank
FF5C mode, MKH and RETI stack invariants are checked.

All six original ENV/VCF/DAC helper outputs and
four ADC channels match separately compiled
production control C; **216 voice + 144 ADC channel
comparisons PASS**. Historical selected MAME CPU
process count is now **868/868** for
the verified stock-MAME follow-on. A second
`fixed-eight` compiled nonzero MAME test is
configured but separately CI-gated.

The `SKIT FAD` NOP was *executed* in all 36 cases;
a test for its skipped-NOP pending-FAD condition is
still needed. The `0400` synthetic post-initial-RETI
dispatch remains, so no entire original Roland
`02EC..04D5` scheduler or hardware timing is
claimed. The next original continuation is
`0738..07B7` VCA/LFO/PWM/noise and jump back
to `02EC`.

### Limitations / strict P1.1 gate still OPEN

The following are now **covered for bounded MAME UPD7810
fixtures**: original source-pinned first-voice
ATTACK/DECAY/RELEASE arithmetic, six naturally looped
envelopes/VCF/DAC helper calls, both ADC service banks,
genuine MAME ADC peripheral-generated IRQ entry/RETI,
and eight distinct fixed callback-sourced ADC channel
values passing through the original ISR and conditioner.
The separately compiled C control engine agrees on all
selected test vectors. This is stronger than a synthetic
ADC register or hand-entered instruction math oracle.

Still OPEN before exact Roland/JUNO-106 firmware/hardware
fidelity can be asserted:

- Authentication of actual original full IC29 firmware
  (the published disassembly is unofficial and may contain
  inaccuracies) and the original UPD7810/UPD7811 CPU
  variant plus memory mapping;
- Full original interrupt-to-main-loop scheduling: the
  linked test uses a declared **synthetic** poll/JMP
  immediately after real RETI. The original
  `0x06B3 DI/SKIT FAD/MKH/EI` scheduler boundary
  and collision windows are not yet verified in the same
  end-to-end path;
- Hardware CPU/ADC clock and interrupt-service cycle timing,
  actual panel-input voltage/ADC curves, physical 8253/PIT,
  DCO pitch/divider, GPIO/clock phase and DAC/S&H aperture;
- Exact output electrical calibration, physical
  resonance/MC5534/80017A analogue audio and a genuinely
  comparable Roland original-audio WAV recording.

Do not begin an MCU waveform/audio engine on the strength of
this CPU gate alone, and do not merge PR #3 until the strictly
defined P1.1 exit evidence is reviewed.

## CI invocation

The standard host CMake/CTest gate remains unchanged. GitHub Actions adds, after the CTests, a byte-source download and two bounded differential commands; the independent MAME CPU checks live in a separate path-gated workflow.

Given a locally provided `ic29.txt` with bytes matching the pinned checksums:

`python3 tests/cpu_ic29/run_adc_opcode_window.py --ic29-disasm PATH --native-cli build/fm1juno_native_adc_reference`

`python3 tests/cpu_ic29/run_vcf_opcode_window.py --ic29-disasm PATH --native-cli build/fm1juno_vcf_clamp_reference`

Independent MAME invocation (Ubuntu package version 0.285) is reproduced by the same dedicated workflow, after building fm1juno_native_adc_reference, fm1juno_vcf_clamp_reference and fm1juno_vcf_chain_reference. The synthetic ROM generator refuses unpinned source bytes and validates a debugger checkpoint before allowing PASS.

The download is an explicit CI dependency, not a hidden fallback to guessed ROM contents. The public disassembly's author explicitly describes it as partial/inaccurate/incomplete; therefore even a verified checksum only authenticates that *published transcription*, not an original Roland chip dump. A changed/unavailable source fails this extra gate. When working offline, run the unchanged CTest suite and pass a separately obtained verified original disassembly file for the opcode gate.

**P1.1_DIGITAL_IN_PROGRESS remains the only valid phase status. No audio/WAV or original Roland unit calibration claims.**
