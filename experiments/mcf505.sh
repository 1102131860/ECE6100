#!/bin/bash

#SBATCH --job-name=mcf505
#SBATCH --output=logs/%x_%j.out        # %x = job name, %j = job ID
#SBATCH --error=logs/%x_%j.err
#SBATCH --time=04:00:00                # max execution time
#SBATCH --mem=4G                       # set main memory
#SBATCH --cpus-per-task=4              # use 4 CPU core

# configuration
TRACE_FILE="../traces/mcf505_2M.trace"
CONFIG_FILE="configurations.json"
OUTPUT_FILE="output/$(basename $TRACE_FILE .out)_output.txt"
LOG_FILE="logs/$(basename $TRACE_FILE .out)_log.txt"

# ensure the directories exist
mkdir -p logs
mkdir -p output

echo "Start $(basename $TRACE_FILE) parameter tunning: $(date)" > "$LOG_FILE"
echo "Start $(basename $TRACE_FILE) parameter tunning" > "$OUTPUT_FILE"

# obtain configurations
config_count=$(jq length "$CONFIG_FILE")
echo "Total configurations: $config_count" | tee -a "$LOG_FILE"

# simulation function
run_simulation() {
    local config_str="$1"
    local b=$2 P=$3 H=$4 s=$5 a=$6 m=$7 l=$8 f=$9 p=${10}

    start_time=$(date +%s)
    ../proj2sim -i "$TRACE_FILE" -b $b -P $P -H $H -s $s -a $a -m $m -l $l -f $f -p $p >> "$OUTPUT_FILE" 2>> "$LOG_FILE"
    exit_code=$?
    end_time=$(date +%s)
    duration=$((end_time - start_time))

    if [ $exit_code -eq 0 ]; then
        echo "configurations $config_str complete, took ${duration}seoncd, executed successfully" | tee -a "$LOG_FILE"
    else
        echo "configurations $config_str failde, exit code: $exit_code, took ${duration}seconds" | tee -a "$LOG_FILE"
    fi
}

# Iterate all the parameters
for i in $(seq 0 $((config_count-1))); do
    # extract parameters
    b=$(jq -r ".[$i].b" "$CONFIG_FILE")
    H=$(jq -r ".[$i].H" "$CONFIG_FILE")
    P=$(jq -r ".[$i].P" "$CONFIG_FILE")
    s=$(jq -r ".[$i].s" "$CONFIG_FILE")
    a=$(jq -r ".[$i].a" "$CONFIG_FILE")
    m=$(jq -r ".[$i].m" "$CONFIG_FILE")
    l=$(jq -r ".[$i].l" "$CONFIG_FILE")
    f=$(jq -r ".[$i].f" "$CONFIG_FILE")
    p=$(jq -r ".[$i].p" "$CONFIG_FILE")

    config_str="b${b}_H${H}_P${P}_s${s}_a${a}_m${m}_l${l}_f${f}_p${p}"
    echo "starting running [$((i+1))/$config_count]: $config_str" | tee -a "$LOG_FILE"
    echo "tunning parameters: $config_str" >> "$OUTPUT_FILE"

    run_simulation "$config_str" $b $P $H $s $a $m $l $f $p

    echo "========================================" >> "$OUTPUT_FILE"
    echo "Finished: $((i+1))/$config_count ($(awk "BEGIN {printf \"%.1f\", ($i+1)*100/$config_count}")%)"
done

echo "Finished all paraemters, totally took $config_count configurations" | tee -a "$LOG_FILE"
echo "Time: $(date)" >> "$LOG_FILE"
echo "Save output to $OUTPUT_FILE"
echo "Save log to $LOG_FILE"
