# 5-Stage Pipelined MIPS CPU (Verilog)

Synthesizable RTL implementation of a pipelined MIPS processor.

## Features
- IF / ID / EX / MEM / WB stages
- Full data hazard resolution via forwarding unit (EX→EX, MEM→EX)
- Branch hazard handling with 2-stage flush
- Self-checking testbench (5 assertions, 0 failures)

## How to run
sudo apt install iverilog
make sim      # runs testbench
make wave     # opens GTKWave
///

# Gate-Level Static Timing Analyzer (C++)

A simplified STA tool modeled after Synopsys PrimeTime.

## Features
- Parses gate-level netlists (.net format)
- Forward pass: arrival times via topological sort
- Backward pass: required times from outputs
- Reports WNS, TNS, slack per node, critical path
- Graphviz DOT export for visual inspection

## How to run
make
make test
