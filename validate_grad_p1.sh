# DO NOT RUN THIS SCRIPT ALONE


benchmarks=( deepsjeng531 leela541 mcf505 xz557 nab544 )
spotlight_benchmark=leela541 # test this benchmark with all configs

flags_gsplit='-b 2 -P 7 -H 12'
flags_gsplit_small='-b 2 -P 7 -H 11'
flags_gselect='-b 1 -P 9 -H 8'
flags_gselect_small='-b 1 -P 8 -H 7'

flags_bp_gsplit='-x '"${flags_gsplit}"
flags_bp_gsplit_small='-x '"${flags_gsplit_small}"
flags_bp_gselect='-x '"${flags_gselect}"
flags_bp_gselect_small='-x '"${flags_gselect_small}"

main_grad_p1() {
    banner "Testing GSelect branch predictor"
    for benchmark in "${benchmarks[@]}"; do
        generate_stats_and_diff bp_gselect "$benchmark"
    done
    generate_stats_and_diff bp_gselect_small "${spotlight_benchmark}"

    banner "(Grad only) Testing GSplit branch predictor"
    for benchmark in "${benchmarks[@]}"; do
        generate_stats_and_diff bp_gsplit "$benchmark"
    done
    generate_stats_and_diff bp_gsplit_small "${spotlight_benchmark}"
}
