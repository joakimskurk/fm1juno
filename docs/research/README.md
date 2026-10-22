# FM1JUNO research workspace

This directory contains the evidence-backed research that feeds the circuit model specification.

Research policy:

- JUNO-106 is the primary reference instrument.
- JUNO-60 is used only where it clarifies shared topology, exposes an implementation detail hidden inside a 106 hybrid module, or provides a musically useful option for DEEP mode.
- Primary sources outrank secondary analysis.
- A claim that materially changes DSP architecture must have an evidence status.
- Unknowns remain explicit; they are not silently filled with generic VA assumptions.

Evidence labels used throughout:

- `A` — primary Roland documentation / schematic / service procedure.
- `B` — direct teardown, measurement, or reverse-engineering evidence.
- `C` — credible secondary technical analysis.
- `H` — hypothesis requiring measurement or stronger evidence.

The reference engine must preserve observed control-rate behaviour and analogue topology before FM-1 optimisation is attempted.
