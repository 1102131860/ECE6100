# DO NOT RUN THIS SCRIPT ALONE

student_stat_dir=student_outs
ta_stat_dir=ref_outs
trace_dir=traces

banner() {
    local message=$1
    printf '%s\n' "$message"
    yes = | head -n ${#message} | tr -d '\n'
    printf '\n'
}

student_stat_path() {
    local config=$1
    local benchmark=$2

    printf '%s' "${student_stat_dir}/${config}_${benchmark}_2M.out"
}

ta_stat_path() {
    local config=$1
    local benchmark=$2

    printf '%s' "${ta_stat_dir}/${config}_${benchmark}_2M.out"
}

human_friendly_flags() {
    local config=$1

    local config_flags_var=flags_$config
    local flags="${!config_flags_var}"
    if [[ -n $flags ]]; then
        printf '%s' "$flags"
    else
        printf '(none)'
    fi
}

generate_stats() {
    local config=$1
    local benchmark=$2

    local config_flags_var=flags_$config

    bash run.sh ${!config_flags_var} -i "${trace_dir}/${benchmark}_2M.trace" >"$(student_stat_path "$config" "$benchmark")"
}

generate_stats_and_diff() {
    local config=$1
    local benchmark=$2

    printf '==> Running %s %s...\n' "$config" "$benchmark"
    generate_stats "$config" "$benchmark"
    if diff -u "$(ta_stat_path "$config" "$benchmark")" "$(student_stat_path "$config" "$benchmark")"; then
        printf 'Matched!\n\n'
    else
        printf '\nPlease examine the differences printed above. Benchmark: %s. Config name: %s. Flags to cachesim used: %s\n\n' "$benchmark" "$config" "$(human_friendly_flags "$config")"
    fi
}


