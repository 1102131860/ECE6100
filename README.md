# ECE6100 Project2 v2 Tomasulo & Branch Predictor Simulator

This project is split into two parts: the Branch Predictors and the Out-of-Order Processor.

## Simulator Parameters

`-x`: When set, only run the Branch Precditors

`-i`: The path to the input trace file

`-h`: Print the usage information

### Branch Predictor Specific Parameters

`-b`: Select the Branch Predictor to use.

- 0: Always Taken

- 1: Gselect

- 2: Gsplit

`-P`: # of address bits for Gselect/number of overlapping bits for Gsplit

`-H`: # of bits in the Global History Register

### Out-of-Order Processor Specific Parameters

`-a`: # of ALU

`-m`: # of MUL

`-l`: # of LSU

`-s`: # of Reservation Stations per function unit

`-f`: fetch width, dispatch width is the value as well

`-p`: # of Physical Regsiters. There will be (p+32) ROB entries

`-d`: This flag has no argument. If set, cache misses and mispredictions are disabled by the driver.

## Branch Predictor Specifications

We know that stalls due to control flow hazards and presence of instructions such as *function calls*, *direct jump*, *return*, etc, which will modify the control flow of a program and be detrimental to a pipeline processor's performance.

To mitigate these stalls, this projects uses **Branch Prediction** technique, and branch predictor is used to solve *Whether a branch is taken or not* (the first W question).

## Out-of-Order Processor

This project implements an Out-of-order FOCO CPU using **Physical Registers/Register Aliasing Table(RAT)**, **Reservation Stations (RS) Table** and **Reorder-Buffer (ROB) update** methods.

In order to enable correct behavior with memory operations in parallel, it uses a **memory disambiguation algorithm**.

The stages mainly include:

- Fetch stage

- Dispatch stage

- Schedule stage

- Execute stage

- Update stage

## Statistics

This simulator records the statistics for branch predictor and out-of-order procssor. The output will be printed out.

## Experiments

The experiments folder aims to tune out the best configuration that performs at least 90% as well as the best performance and uses as few resources as possible.

The resources utilization is based on `branch_prediction_table_size`, `fetch width`, `dispatch width`, `Num. Pregs`, `reservation station size`, `Num. ALU FUs`, `Num. MUL FUs`, `Num. LSU FUs`, and `ROB entries`.

## How to use the simulator

clone this repository and command `make` or `make FAST=1`, then the ./proj2sim will be built.

Run the simulator like this,

```
./proj2sim -i traces/deepsjeng531_2M.out -b 1 -P 5 -H 10 -s 5 -a 3 -m 2 -l 2 -f 4 -p 64
```
