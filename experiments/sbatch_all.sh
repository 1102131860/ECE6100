#!/bin/bash

for script in *.sh; do
    # skip sbatch_all.sh itself
    [ "$script" == "sbatch_all.sh" ] && continue

    echo "submit $script ..."
    sbatch "$script"
done