author: linhtk55-fpt

# dmic_tlm

DMIC TLM model — a TLM-2.0 component that models a digital microphone (PDM/DMIC) interface for SystemC simulations. This repository contains a lightweight model and a testbench to exercise it. It uses Cascaded Integrator Comb (CIC) to filter PDM signals to PCM.

- Simple TLM-2.0 component exposing a DMIC interface for test and simulation.
- Includes a small testbench and a sample `main` to build and run simulations.
- PDM frequency: 3 MHz
- PCM frequency: 48 kHz

Requirements:
- A C++17 toolchain (`g++` or `clang++`).
- SystemC installed (set `SYSTEMC_HOME` in the `Makefile` or to an installation path).

## Usage

- Edit the top of the `Makefile` if needed and set `SYSTEMC_HOME` to your SystemC installation path.

- Build the project:

```
make
```

- Make and run the built executable:

```
make main 
```

The default target builds `out/main` (see `Makefile`).

## Known limitations

- No status/control registers.
