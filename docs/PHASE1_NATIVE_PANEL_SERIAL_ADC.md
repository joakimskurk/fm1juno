# P1.1 — IC29 native switch, ADC bank and serial receive contract

Updated: 2026-11-03
State: **native module-CPU macro-level model implemented/tested**. This is not an original-CPU cycle-accurate emulator, a full panel-scanner/assigner simulator or an analogue 106.

## Primary evidence and ROM addresses

Reference (public disassembly for analysis; **not copied into the implementation**):
https://github.com/ErroneousBosh/j106roms/blob/master/ic29.txt

| ROM region | Observed action |
|---|---|
| $0070..$008D | ADC ISR toggles scan bank, reads a four-channel bank and publishes its raw bytes |
| $008E..$010C | Serial command/data receiver; 0x8E..0xA3 patch command dispatch; patch data increments running command; 0x80..0x87 immediate voice-off/sustain |
| $010D..$014C | Voice-on data triggers note/attack/reset logic and restarts the main loop |
| $014F..$0182 | Switch-1 to IC40 latch bits, pulse enable flag and PF clock-range output |
| $0184..$0196 | Switch-2 to inverted HPF selector in IC40, plus direct flags in later control routines |
| $01A7..$01DE | 7-bit patch controls: double selected depths, shift 14-bit CV control left by seven |
| $0223..$0234 | Serial signed bend quantity: D=0 remains zero; nonzero becomes 2D+1 |
| $02EC..$02F2 | IC40 register emitted to hardware at frame entry |
| $05A6..$05B2 | Switch-2 bit 2 selects ENV vs GATE on individual voice VCA |
| $05D7..$0619 | Switch-2 bit 1 selects add vs subtract VCF envelope |
| $06E2..$0737 | Main-loop ADC bank conditioner consumed as Tune/Portamento/LFO trigger/sensitivity or Bend/Sens |
| $076C..$0770 | Switch-2 bit 0 selects LFO-PWM vs fixed manual carrier |
| $0800..$0824 | ADC per-channel deadband, endpoint correction |

## Switch-1 byte

The host decoder preserves the complete raw incoming byte. Actual normalized effects come from the following **active-low** truth table:

| Raw bit / condition | Software/physical effect |
|---|---|
| bit 4 | Copy raw bit into IC40 bit 4 (saw latch line) |
| bit 5 clear | Set IC40 bit 0 (chorus control line); set → clear it |
| bit 6 set | Set IC40 bit 1 (second chorus control line); clear → clear it |
| bit 3 clear | Pulse/square enabled; set → disabled |
| bit 2 clear | PF prescaler output = 0xC0 (4' selection) |
| bit 2 set and bit 1 clear | PF = 0x40 (8' selection) |
| bits 2 and 1 set | PF = 0x00 (16' selection) |

IC40's bits 2–3 (HPF) are preserved across a switch-1 write. The command updates the **IC40 register** immediately, not the output. The ROM physically emits that register at the next `FRAME_START`. The reference tracks both values.

**Important:** the PF prescaler byte is decoded and retained as a digital output, but it does not force a phase jump on running `Timer8253Voice106` instances; how live hardware clock switching affects the 8253 phase is **MEASURE_PENDING**. This is deliberate, not a claim that the live switch has no real effect.

The raw IC40 line polarities are preserved; the reference does not relabel those lines as guaranteed analogue saw/chorus ON/OFF characteristics without the downstream board truth table.

## Switch-2 byte

The exact IC40 HPF bit transform is:

`ic40_register = (ic40_register & 0xF3) | (((switch2 ^ 0x18) & 0x18) >> 1)`

Therefore the observable two-bit selector is `((switch2 >> 3) ^ 3) & 3`; do not interpret the host selector as an analogue HPF transfer curve.

| Bit in original switch-2 byte | 0 | 1 |
|---|---|---|
| bit 0 | fixed/manual PWM carrier | PWM follows LFO |
| bit 1 | negative/inverted ENV-to-VCF | positive ENV-to-VCF |
| bit 2 | individual voice VCA GATE mode | individual voice VCA ENV mode |
| bits 3–4 | inverted HPF code into IC40 | inverted HPF code into IC40 |

The existing normalized ID set (`RAW106_PWM_MANUAL`, `RAW106_VCF_ENV_INVERTED`, `RAW106_VCA_GATE_MODE`) continues to work for legacy tests. A native switch-2 event updates all three together; a later direct normalized event can override that field without mutating the recorded original switch-2 byte. **Last input wins** on the semantic flag, while the native raw register retains the received hardware byte.

## ADC interrupt and late service

Each incoming ADC snapshot is **one bank of four 8-bit channels**, packed little-endian into a 32-bit word:

`value = ch0 | (ch1<<8) | (ch2<<16) | (ch3<<24)`

- `voice=0` → physical channel bank 0..3 (`FF80..83` raw to processed `FF88..8B`);
- `voice=1` → bank 4..7 (`FF84..87` raw to processed `FF8C..8F`).

The ISR-style `ADC_BANK` event records raw bytes, marks the latest published bank and increments the ADC interrupt counter. It does **not** compute Tune/Bend immediately, and it never restarts the pass. When an ADC snapshot is the first control input, it also does **not** activate semantic raw-control mode until that late ADC slot: injected legacy ENV/pitch harness values remain unchanged in intervening earlier slots.

At a later `ADC_PERFORMANCE` scheduler slot, the latest bank is processed. If multiple interrupts arrived, only the last published bank is serviced by this main-loop call (the earlier snapshot remains in its raw register but is not automatically replayed). This is an explicit *last visible bank* boundary; exact ADC scan conversion cadence and ISR collision timing remain unverified.

For each of that bank's four channels:

1. compare raw byte with previous accepted **raw** byte;
2. only accept change if unsigned absolute difference is **greater than 1**; exact ±1 change stays held;
3. convert the accepted raw value to conditioned 8-bit control:
   - `n = max(0, held_raw - 4)`
   - if `n < 235`, output `n`;
   - otherwise output `min(255, 2*n-235)`.
4. Write the consumed semantic controls only in this slot.

Independent endpoint vectors: raw 0→0, 4→0, 5→1, 238→234, 239→235, 240→237, 249→255, 255→255. The top-end two-slope segment is from the actual $080C..$081A branch structure, not an unverified arbitrary 8-bit normalization.

### Channel use

| Physical ADC channel | Consumed role in current model |
|---:|---|
| 0 | Tune ADC, centered on 0x80 in the existing pitch pipeline |
| 1 | Portamento ADC before coefficient index shift |
| 2 | LFO physical trigger from processed bit 7 |
| 3 | Physical performance LFO sensitivity |
| 4 | VCF bend sensitivity |
| 5 | DCO bend sensitivity |
| 6 | physical bender sign byte; bit 7 clear selects negative |
| 7 | physical bender magnitude |

The higher-order ADC interpolation, original board voltage profile, phase relation to ADC clock, and exact analogue bender center are still open. This does **not** mean raw ADC and 7-bit patch sliders are interchangeable.

## Native serial command + running status

Use `SERIAL_BYTE` to feed the emulated module CPU's UART one **8-bit byte at a time**. A byte with bit 7 set stores the current command; a following 7-bit value supplies data. In the original patch range, every data byte increments the pending command, so sending 0x8E followed by a series of patch values updates subsequent entries.

- `0x80..0x85`: immediate physical voice OFF 0..5;
- `0x86/0x87`: sustain ON/OFF;
- `0x88..0x8D`: pending note-ON for voice 0..5 (following 7-bit note value); the note path restarts the main loop;
- `0x8E/0x8F`: switch-1 / switch-2 receives;
- `0x90..0x9F`: 16 sequential original patch controls, mapped to stable existing raw IDs, which retain their intended parameter-specific scaling at scheduler use;
- `0xA0/0xA1`: negative/positive MIDI bend data (0→0, nonzero→2D+1);
- `0xA2`: MIDI mod depth (raw7 doubled to 0..254);
- `0xA3`: original module's test-mode command is **deliberately ignored** in the normal-play host model;
- out-of-range/unknown bytes are not treated as valid patch data.

The separate `SWITCH1` / `SWITCH2` test events accept a literal 8-bit register value, while **on-wire serial data** has the real 7-bit limitation. Thus `SERIAL_BYTE` is the recommended native-path fidelity test.

This is **module CPU protocol only**; it is not a complete emulation of Roland's assigner MCU, voice allocation/keyboard scan, MIDI parser, external SysEx or test-mode firmware.

## Event-script v1 additions (existing IDs remain unchanged)

All records use `version,sample,frac_q32,type,voice,value,aux`.

| type | voice | value | aux |
|---|---|---|---|
| `SWITCH1` | 255 | original switch-1 byte 0..255 | 0 |
| `SWITCH2` | 255 | original switch-2 byte 0..255 | 0 |
| `ADC_BANK` | 0 or 1 | four packed ADC channel bytes | 0 |
| `SERIAL_BYTE` | 255 | one UART byte 0..255 | 0 |

See `tests/vectors/p1_native_switch_adc_v1.csv` and `tests/vectors/p1_native_serial_v1.csv`. Source tests: `tests/unit/test_native_panel.c`, including an ADC-only ISR test proving an earlier ENV tick uses its previous state until the first ADC slot. CI host-tests #232 passed with this causality guard.

CSV decimal fields are now bounds-checked **before narrowing casts**; invalid voice 511 or value 4294967296 do not alias into valid events. Negative fixtures in `tests/vectors/p1_invalid_*` are expected to fail parsing.

## Remaining strictness gates

- Validate ADC condition and switch-flag edge cases in an **executable original uPD7811 instruction model** for every branch/condition; current law is recovered from disassembly, not cycle-accurate simulated.
- Preserve/verify the original ADC ISR's automatic bank alternation, exact interrupt timing and conversion clock rather than treating supplied bank snapshots as a physical scanner.
- Verify original range-selected 8253 clocks and live clock-phase switching; only PF output bits have been decoded.
- Verify original IC40 bit polarities against chorus/HPF/saw circuit truth tables.
- Certify parameter-receive timing vs DAC latch offsets; macro-order is verified but exact microseconds remain `MEASURE_PENDING`.
- Keep derived count table and DCO-CV clean-room approximations separate from a strict ROM-exact claim; still no audio/WAV.

**Gate state: P1.1 DIGITAL IN PROGRESS**. This slice replaces the semantic-input *only* model with an explicit native receiver, but does not close the entire Phase-1 digital gate.
