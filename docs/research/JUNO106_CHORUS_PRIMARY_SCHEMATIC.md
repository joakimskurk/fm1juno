# JUNO-106 chorus — primary jack-board schematic transcription

Status: Phase 0 primary-source transcription. This document records only facts that can be read directly from Roland's JUNO-106 Service Notes jack-board schematic, plus clearly labelled cross-checks. It is deliberately stricter than a clone-BOM transcription.

Primary source:

- Roland, **JUNO-106 Service Notes**, first edition, July 31 1984, jack-board schematic p. 15 / jack-board PCB p. 14.
- Readable scan: https://www.vintagesynthparts.com/wp-content/uploads/2017/03/JUNO-106_SERVICE_NOTES.pdf
- Independent schematic crop: https://www.florian-anwander.de/roland_string_choruses/JUNO-106_schem.jpg

Secondary/cross-check sources are used only where explicitly marked.

## 1. Why this changes the Phase-0 state

Earlier notes correctly refused to invent a 106 filter cascade from reconstruction BOMs. We now have a readable original Roland jack-board schematic in which the complete chorus section is present on one sheet.

That means the following statement is no longer accurate:

> the original 106 node topology is unavailable

The correct state is now:

> the original node topology is available from the Roland schematic, but it has not yet been fully converted into a checked machine-readable netlist / SPICE model.

This is an important distinction. Architecture no longer needs to be inferred from the One-O-Six reconstruction. Numerical DSP coefficients still must wait for a verified transcription and transfer-function calculation.

## 2. Primary IC / calibration map

Directly visible on the Roland jack-board schematic:

```text
IC8   MN3009   BBD channel A
IC9   MN3101   BBD driver/clock channel A
IC10  MN3009   BBD channel B
IC11  MN3101   BBD driver/clock channel B
IC6   TA75558S dual op-amp in the final stereo output section
VR1   BBD bias channel A
VR2   BBD bias channel B
TP1   channel-A chorus test point
TP2   channel-B chorus test point
TP3   chorus-control/LFO test point
TP4   complementary chorus-control/LFO test point
```

Roland's adjustment procedure independently confirms TP1/TP2 + VR1/VR2 as the two chorus-bias calibration paths.

Strict consequence: these are two physical analogue paths with independent BBD bias and shared-but-complementary modulation. A single variable delay duplicated to stereo after processing is not topology-faithful.

## 3. Original signal-path topology now directly visible

The schematic establishes this physical order:

```text
post-voice/global-level signal
 -> chorus input / bypass routing
 -> split to two BBD audio paths

channel A:
 -> input coupling/bias/filter network
 -> IC8 MN3009
 -> BBD output combining/filter/amplifier network
 -> TP1
 -> stereo-A dry/wet/output network

channel B:
 -> input coupling/bias/filter network
 -> IC10 MN3009
 -> BBD output combining/filter/amplifier network
 -> TP2
 -> stereo-B dry/wet/output network

shared/complementary control:
 chorus control generator
 -> TP3 / TP4 complementary trajectories
 -> IC9 / IC11 MN3101 clock networks
 -> IC8 / IC10 BBD clocks
```

The final output section is not merely `dry + delay` in software terms. The schematic shows explicit passive mixing/coupling around each wet test point followed by the IC6 analogue output stages and muting/output transistors.

## 4. Clock-network primary values

The original schematic plus independent repair/modification evidence support:

```text
R124 = 8.2 kOhm   channel-A MN3101 timing network
R137 = 8.2 kOhm   channel-B MN3101 timing network
C53  = 150 pF     channel-A timing capacitor
C57  = 150 pF     channel-B timing capacitor
```

Cross-check: Florian Anwander's Roland chorus survey documents the historical MN3009 -> MN3007 modification by changing JUNO-106 R124/R137 from 8.2 kOhm to roughly one quarter of that value, which independently confirms both designators and their role in the BBD clock oscillator.

The two 150 pF timing capacitors are also supported by original-board repair evidence discussed in the companion clock document.

These values are now strong enough to be treated as original-106 component evidence rather than clone-only values.

## 5. Primary output-mix structure

The Roland schematic visibly provides separate resistor/coupling networks after TP1 and TP2 before the two halves of IC6. The two sides are deliberately mirrored.

Readable principal values around the passive/output mixing region include:

```text
channel A: R71 = 47 kOhm, R70 = 100 kOhm, R72 = 39 kOhm, R54 = 15 kOhm
channel B: R73 = 47 kOhm, R67 = 100 kOhm, R74 = 39 kOhm, R57 = 15 kOhm
```

These values are recorded here as a transcription checkpoint, not yet as a complete transfer function. The exact node connections, coupling-cap values, IC6 feedback components, mute network and output loading must be entered into the netlist before deriving dry/wet voltage gain.

Important implication: the approximately 55/45 dry/wet figure from the One-O-Six reconstruction must remain an **acceptance target**, not a hard-coded pair of mixer coefficients. The original resistor network, BBD insertion loss and final active stage determine the actual ratio.

## 6. Audio-filter topology — what is closed and what is not

### Closed from primary schematic

- there is analogue conditioning before each MN3009;
- the two BBD outputs are not sent directly to the stereo jacks;
- each wet path contains multiple passive/reactive elements and discrete transistor stages around the BBD;
- the two audio paths are intentionally near-mirrors;
- wet-path test points TP1/TP2 occur before the final stereo output network;
- dry and delayed audio are combined in the physical output section;
- the final stereo stage includes IC6 plus discrete muting/output circuitry.

### Still open

- checked node-by-node transcription of every resistor/capacitor around IC8/IC10;
- exact small-signal pre-BBD transfer;
- exact post-BBD transfer;
- BBD source/load impedance contribution;
- clock-dependent MN3009 bandwidth/insertion loss;
- exact dry/wet voltage transfer including IC6 and output loading;
- clipping/headroom transfer around the calibrated bias point;
- mono-normalled output loading/summing law.

So we can now stop asking **what the topology is** and start asking **what the verified transfer function of that topology is**.

## 7. Netlist transcription contract

The next research artifact should be a literal source-of-truth netlist, not hand-written DSP coefficients.

Required conventions:

```text
component designator must match Roland schematic
node name must describe physical role, not DSP interpretation
unknown/unreadable value -> MEASURE_OR_RESCAN, never guessed
all BBD and MN3101 pins retained explicitly
TP1..TP4 retained as named probe nodes
power rails retained for headroom/bias analysis
mute/output transistors retained until proven irrelevant
```

Suggested file:

```text
docs/research/netlists/juno106_jack_chorus_reference.cir
```

Suggested validation stages:

1. DC operating-point sanity without BBD transport;
2. fixed-clock small-signal AC response for both channels;
3. insert measured/parametric MN3009 source/load model;
4. compare left/right transfer symmetry;
5. calculate dry/wet output gain and polarity;
6. compare against a calibrated physical JUNO-106 sweep;
7. only then fit the minimal digital filter structure.

## 8. Relationship to the reconstruction BOM

Alpes Machines / One-O-Six remains useful because it:

- was explicitly designed around the 106 chorus;
- exposes a practical BOM and calibration workflow;
- documents approximately 55% dry / 45% wet at the finished output;
- documents a reported one-channel control-path capacitor asymmetry;
- gives a working adjustable LFO/clock implementation.

But the Roland schematic is now the authority for original designators and topology. Where the reconstruction differs, the original schematic wins for `MODEL_106_STRICT`.

## 9. Phase-0 gate update

The chorus gate should now be interpreted as:

```text
[x] original 106 jack-board schematic located and readable
[x] original dual-BBD / dual-driver / bias / output-stage topology identified
[x] R124/R137 8.2k timing-resistor evidence promoted to primary-supported
[x] C53/C57 150p timing-capacitor evidence strongly cross-checked
[ ] complete checked jack-board chorus netlist transcription
[ ] compute exact fixed-clock analogue transfer functions
[ ] measure/freeze Mode I and II clock trajectories
[ ] measure/freeze BBD insertion loss/bandwidth vs clock
[ ] measure/freeze dry/wet and mono-output gains
[ ] measure/freeze calibrated BBD headroom/nonlinearity/noise
```

No production DSP constant should be frozen from the schematic image alone. The next irreversible step is the checked netlist.