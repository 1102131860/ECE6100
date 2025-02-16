#!/bin/bash
set -e

exper_dir="experiments"
log_file="experiments.log"

l1_sizes=(14 15)                    # 14KB (2^14), 32KB (2^15)
l2_sizes=(16 17)                    # 64KB (2^16), 128KB (2^17)
vc_data_storage=7                   # 128B (2^7)
block_sizes=(5 6 7)                 # 32B (2^5) to 64B (2^6)
l1_associativities=(2 3)
l2_associativities=(2 3 4)
replacement_policies=("mip" "lip" "fifo" "random")
early_restart_options=("-E" "")
traces=("gcc" "leela" "linpack" "matmul_naive" "matmul_tiled" "mcf")

tuning() {
    mkdir -p "${exper_dir}"
    echo "=== Experiment Started at $(date) ===" > "$log_file"

    for trace in "${traces[@]}"; do
        output_file="${exper_dir}/${trace}.out"
        echo "Results for trace $trace" > "$output_file"
        echo "==========================================" >> "$log_file"
        echo "Trace: $trace" >> "$log_file"
        
        for er in "${early_restart_options[@]}"; do
            for policy in "${replacement_policies[@]}"; do
                for c1 in "${l1_sizes[@]}"; do
                    for c2 in "${l2_sizes[@]}"; do
                        for b in "${block_sizes[@]}"; do
                            for ((s1=0; s1<=c1 - b; s1++)); do
                                for ((s2=s1; s2<=c2 - b; s2++)); do
                                    upper_bound=$((1 << (vc_data_storage - b)))             # vc can has the most 128B storage
                                    upper_bound=$((upper_bound > 2 ? 2 : upper_bound))
                                    for ((v = 0; v<=upper_bound; v++)); do
                                        config_name="c=$c1 B=$b s=$s1 v=$v C=$c2 S=$s2 P=$policy $er"
                                        echo "Tuning: $config_name..." >> "$log_file"
                                        echo "==========================================" >> "$output_file"
                                        ./cachesim -c $c1 -b $b -s $s1 -v $v -C $c2 -S $s2 -P $policy $er < "traces/${trace}.trace" >> "$output_file"
                                        echo "Completed: $config_name" >> "$log_file"
                                    done
                                done
                            done
                        done
                    done
                done
            done
        done
    done

    echo "=== Experiment Completed at $(date) ===" >> "$log_file"
}

tuning