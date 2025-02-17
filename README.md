# Project 1: Cache Simulator

The goal of this project is to build a cache simulator and verify it is correct via validation outputs of our reference simulator (validation outputs provided).

- Commands
    - `make` to build the program

    - `make validate_grad` to test the program

    - `nohup ./experiment.sh &` to generate experiments data and log file at the back

    - `python best_config.py > best_config.out` to generate csv and images

- Files
    - Makefile

    - Source code 
        - cachesim.cpp
        - cachesim.hpp
        - cacehsim_driver.cpp

    - bash
        - run.sh
        - validate_grad.sh
        - experiment.sh

    - python
        - best_config.py

    - csv
        - experiments.csv
        - best_config.csv

    - output
        - best_config.output

    - log
        - experiments.log

- folder

    - experiments/

    - images/
