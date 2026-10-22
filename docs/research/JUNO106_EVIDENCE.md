# JUNO-106 evidence register

Status: Phase 0 working document.

## Source hierarchy

Primary reference: Roland JUNO-106 Service Notes, First Edition, 31 July 1984.
Secondary primary reference: Roland JUNO-106 Owner's Manual and current Roland technical-specification archive.
Supporting evidence: physical teardown/reverse engineering of 80017A, independent circuit analysis, and later Roland model documentation where explicitly marked.

## Confirmed architecture

### CPU/control

**A** — The instrument uses a master CPU on the CPU board and a slave CPU on the module board. The master handles keyboard/panel/MIDI/preset duties; the slave receives key and timbre data, reads Tune/Portamento/Bender/Modulation, writes divide data to the 8253 timers, generates the envelope, generates VCF/VCA control voltages, generates LFO/noise control data, and applies portamento.

**A** — The module-board CPU receives a 12 MHz clock. The DCO audio master oscillator is separate: 8 MHz, followed by range division.

### Control-voltage service cycle

**A** — The slave CPU does not emit independent analogue controls continuously. It produces integrated 12-bit parameter values through one DAC and multiplexed sample/hold channels. The service timing chart shows one full D/A + S/H service cycle of approximately 4.2 ms.

**Implementation consequence:** reference DSP should support a ~4.2 ms control-frame cadence for the multiplexed CV domain before any optional anti-zipper smoothing. Do not silently convert all control modulation to per-sample modulation.

### DCO clock and timers

**A** — An 8 MHz master oscillator feeds divider IC35. Range selection divides by 2, 4 or 8 for 4', 8' and 16' respectively, then feeds two 8253 programmable interval timers (six counters total).

**A** — The timer divide data combines key number with LFO, Bender, Portamento and Tune contributions for each active note.

**A** — The resultant timer output is a rectangular audio-frequency timing waveform.

**Implementation consequence:** model a shared master clock plus integer/range division and per-voice timer counts. Do not treat the authentic 106 DCO as six unrelated free-running floating-point oscillators.

### Wave generator / MC5534A

**A** — Three MC5534A packages each contain two wave-generator channels, giving six voices. For a rectangular DCO timing input, each voice generates:

- divide-by-two rectangular sub oscillator;
- sawtooth;
- variable-width rectangular pulse.

**A** — Saw and pulse amplitude is maintained at approximately 12 Vpp across frequency by a DCO CV from the slave CPU plus range-dependent RC selection. The DCO CV includes key, LFO, Bender, Portamento and Tune but excludes Range itself; Range selects the charge resistor externally.

**A** — The saw capacitor charges from DCO CV through a range-selected resistor and is discharged/reset by the positive-going DCO timing edge.

**A** — Pulse width is created by comparing the saw with PWM CV. Service documentation gives approximately 50% duty at +6 V PWM CV and 95% at +0.6 V; PWM OFF drives the comparator so the pulse output remains high/disabled.

**A** — Service adjustment requires 48–52% duty for the nominal 50% test and 93–97% for the high-PWM test.

**Implementation consequence:** use timer-triggered analogue-inspired charge/reset waveform generation. PWM should be a comparator against that ramp, not a separate ideal pulse oscillator.

### Source mixer

**A** — Saw and pulse are selectable; sub and noise have level controls. The module-board control system includes separate sample/hold values for sub level and noise level.

**Open:** exact source gain laws/headroom through the pre-VCF network should be extracted from the schematic and/or measured. Do not assume equal unity gains.

### VCF/VCA — 80017A

**A/B** — Each voice uses one A1QH80017A/800170 VCF/VCA hybrid. Roland's internal block drawing shows a four-stage IR3109-family filter core and BA662 blocks for resonance control and the final voice VCA. Physical teardown independently confirms one IR3109 plus two BA662 devices and passive components inside the hybrid.

**A** — VCF CV integrates cutoff, ENV, LFO, key follow and bender contributions into one control voltage.

**A** — VCA CV includes ENV and GATE behaviour.

**A** — Specification: VCF cutoff nominal range 5 Hz–50 kHz; resonance can reach self oscillation; ENV modulation ±14 octaves; LFO and bender modulation ±3.5 octaves; key follow +3/−2 octaves in the service specification.

**A** — Factory service alignment calibrates each of the six voices separately for resonance amplitude, VCA gain, VCF frequency and VCF width, proving meaningful per-voice analogue calibration degrees of freedom.

**Implementation consequence:** one shared topology with per-voice calibration terms. Authentic mode may initialise them near matched factory condition; DEEP can expose controlled spread.

### Envelope

**A** — The slave CPU generates the envelope and folds it into VCF/VCA CV as required. Nominal ranges: Attack 1.5 ms–3 s, Decay 1.5 ms–12 s, Sustain 0–100%, Release 1.5 ms–12 s.

**Implementation consequence:** the 106 envelope is fundamentally a digitally generated control process feeding sampled analogue control voltages. It should not be implemented as the JUNO-60 IR3R01 analogue envelope with different constants.

**Open:** exact time law, segment curvature, internal numerical resolution and retrigger state need ROM analysis or direct hardware measurement. Until then, endpoints/ranges and control cadence are confirmed, exact segment math is not.

### Voice assignment

**A** — Service mode documents several assignment behaviours:

- Poly 1 test/unison: all six modules can be assigned simultaneously to one key.
- Poly 2 test/non-rotary: voices are assigned to held keys in CH1→CH6 order; the seventh key can steal CH1.
- Poly 1 + Poly 2 test/rotary: voices assign cyclically and the seventh key steals the first voice.

**Open:** map service-test wording carefully to normal front-panel Poly 1 / Poly 2 behaviour using owner's manual and performance tests before freezing allocator semantics.

### HPF / output path

**A** — Owner documentation explicitly states that HPF position 1 is effectively unprocessed while position 0 boosts low frequencies; positions 2 and 3 progressively raise the high-pass cutoff.

**A** — Schematic places the global HPF after voice summing and before the global output VCA/chorus path.

**C** — Circuit analysis identifies position 0 as an active low-frequency boost network and notes that the 106 buffers the HPF before the global uPC1252 VCA, unlike the 60's more load-dependent connection.

**Implementation consequence:** model the four discrete 106 states, including bass-boost position 0. A generic continuously variable one-pole HPF is not authentic.

### Chorus

**A** — Chorus is global and follows the post-voice path. The 106 provides OFF, I and II, and the owner's manual does not define simultaneous I+II as a normal operating mode.

**A** — Jack-board schematic shows two MN3009 BBD channels driven by two MN3101 clock-driver sections, separate bias trims and surrounding filtering/amplification.

**A** — Service adjustment injects a 10 Vpp, 1 kHz sine and trims each chorus channel so positive and negative halves are symmetrical, confirming bias-dependent nonlinearity/headroom is part of the real circuit.

**Open:** authentic 106 I/II modulation rate, delay excursion, bandwidth and noise spectrum require either direct measurement or a well-supported measured source. Do not transplant JUNO-60 measured values as 106 constants without evidence.

## JUNO-60 lessons allowed into the 106 project

Use 60 evidence only in these categories:

1. **Shared topology hidden inside 106 hybrids** — e.g. IR3109 + BA662 heritage can inform the 80017A model.
2. **Test methodology** — 60 waveform/chorus measurement techniques are useful even when constants differ.
3. **DEEP alternatives** — 60 envelope/clock/HPF/chorus behaviours can become selectable extensions after the 106 model is calibrated.

Do not import 60 constants merely because the panel/function names match.

## High-priority unknowns

1. Exact 8253 programming mode and count/output relationship used by the 106.
2. Timer-count quantisation across the keyboard and during LFO/bend/portamento.
3. Exact digital envelope law and segment resolution.
4. Exact LFO waveform generation, update cadence and delay law.
5. DCO CV transfer law versus note/range for ramp-amplitude compensation.
6. Pre-filter source mixer gains and clipping/headroom.
7. 80017A nonlinear behaviour around resonance/self-oscillation and voice VCA.
8. HPF transfer functions for positions 0–3.
9. Chorus I/II delay/rate/filter/noise/nonlinearity.
10. Normal-mode Poly 1/Poly 2/retrigger/steal semantics.

No unknown in this list may be replaced by a generic VA default without being marked as a temporary hypothesis.
