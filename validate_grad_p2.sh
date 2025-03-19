# DO NOT RUN THIS SCRIPT ALONE

flags_gsplit_8kb='-b 2 -P 5 -H 10'
# flags_always_taken='-b 0'
flags_big_gsplit="${flags_gsplit_8kb}"' -s 8 -a 4 -m 3 -l 2 -f 8 -p 128'
flags_med_gsplit="${flags_gsplit_8kb}"' -s 5 -a 3 -m 2 -l 2 -f 4 -p 64'
flags_med_nomiss_gsplit="${flags_gsplit_8kb}"' -s 5 -a 3 -m 2 -l 2 -f 4 -p 64 -d'
flags_tiny_gsplit="${flags_gsplit_8kb}"' -s 4 -a 1 -m 1 -l 1 -f 2 -p 32'
# flags_med_always_taken="${flags_always_taken}"' -s 5 -a 3 -m 2 -l 2 -f 4'

main_grad_p2() {
    banner "Testing Tomasulo: med_nomiss config"
    for benchmark in "${benchmarks[@]}"; do
        generate_stats_and_diff med_nomiss_gsplit "$benchmark"
    done
    # banner "Testing Tomasulo: med_always_taken config"
    # for benchmark in "${benchmarks[@]}"; do
    #     generate_stats_and_diff med_always_taken "$benchmark"
    # done
    banner "Testing Tomasulo: med config"
    for benchmark in "${benchmarks[@]}"; do
        generate_stats_and_diff med_gsplit "$benchmark"
    done
    banner "Testing Tomasulo: big config"
    for benchmark in "${benchmarks[@]}"; do
        generate_stats_and_diff big_gsplit "$benchmark"
    done
    banner "Testing Tomasulo: tiny config"
    for benchmark in "${benchmarks[@]}"; do
        generate_stats_and_diff tiny_gsplit "$benchmark"
    done
}
