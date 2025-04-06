import os
import re
import pandas as pd
import numpy as np
import seaborn as sns
import matplotlib.pyplot as plt

# Directory and trace files
directory = "output"
image_direc = "images"
files_list = [
    "deepsjeng531_2M.trace_output.txt",
    "leela541_2M.trace_output.txt",
    "mcf505_2M.trace_output.txt",
    "nab544_2M.trace_output.txt",
    "xz557_2M.trace_output.txt"
]
output_csv = "merge_output.csv"
util_output_csv = "util_output.csv"
optim_output_csv = "optim_output.csv"

def parse_config(text):
    config = {}
    for line in text.splitlines():
        m = re.match(r'([^:]+):\s+(.+)', line)
        if m:
            key = m.group(1).strip()
            value = m.group(2).strip()
            config[key] = value
    return config

def parse_file(file_path):
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    blocks = re.split(r'\n=+\n', content)
    records = []

    for block in blocks:
        block = block.strip()
        if not block:
            continue

        trace = os.path.basename(file_path).split("_")[0]
        record = {"trace": trace}

        # extract SIMULATION CONFIGURATION
        config_match = re.search(
            r'SIMULATION CONFIGURATION \(PROCESSOR\)(.*?)SIMULATION CONFIGURATION \(BRANCH PREDICTOR\)(.*?)SETUP COMPLETE - STARTING SIMULATION',
            block,
            re.DOTALL
        )
        if config_match:
            processor_text = config_match.group(1).strip()
            branch_text = config_match.group(2).strip()
            combined_config = processor_text + "\n" + branch_text
            record.update(parse_config(combined_config))

        # extract SIMULATION OUTPUT
        output_match = re.search(r'SIMULATION OUTPUT\n(.*)', block, re.DOTALL)
        if output_match:
            output_text = output_match.group(1).strip()
            record.update(parse_config(output_text))

        records.append(record)

    return records

def add_branch_pattern_table_resveration_station_size(df):
    # calculate Branch Pattern Table size
    gselect_mask = df["Predictor (M)"].str.strip() == "GSELECT"
    # if Predictor is GSELECT, then use (P + H - 12) else (2*H - P - 12)
    shift_gselect = (df["P"] + df["H"] - 12).clip(lower=0)
    shift_other = (2 * df["H"] - df["P"] - 12).clip(lower=0)
    shift = np.where(gselect_mask, shift_gselect, shift_other).astype(int)
    branch_pattern_table_size = np.left_shift(1, shift)
    # add branch_pattern_table_size into df
    df["branch_pattern_table_size"] = branch_pattern_table_size

    # calculate Reservation Station Table Size
    reservation_station_size = df["Num. SchedQ entries per FU"] * (df["Num. ALU FUs"] + df["Num. MUL FUs"] + df["Num. LSU FUs"])
    # add reservation_station_size into df
    df["reservation_station_size"] = reservation_station_size
    return df

def add_resource_utilization(df):
    # weights
    weight_branch_pattern = 0.5
    weight_fetch_width = 1
    weight_dispatch_width = 1
    weight_num_pregs = 3
    weight_reservation_station = 1.5
    weight_num_alu = 2
    weight_num_mul = 5
    weight_num_lsu = 4
    weight_rob = 4

    # calculate resource_utilization and add it into df
    df["resource_utilization"] = (
        weight_branch_pattern * df["branch_pattern_table_size"] +
        weight_fetch_width * df["Fetch width"] +
        weight_dispatch_width * df["Dispatch width"] +
        weight_num_pregs * df["Num. PREGS"] +
        weight_reservation_station * df["reservation_station_size"] +
        weight_num_alu * df["Num. ALU FUs"] +
        weight_num_mul * df["Num. MUL FUs"] +
        weight_num_lsu * df["Num. LSU FUs"] +
        weight_rob * df["ROB entries"]
    )
    return df

def find_optimum_pipeline_configuration(df):
    optimum_configs = []
    # group by trace
    for trace, group in df.groupby('trace'):
        best_ipc = group['IPC'].max()
        # select IPC is larger or equal 90% best_ipc
        candidate_configs = group[group['IPC'] >= 0.9 * best_ipc]
        # select the row has the minimum resource_utilization
        optimum_config = candidate_configs.loc[candidate_configs['resource_utilization'].idxmin()]
        optimum_configs.append(optimum_config)
    return df.__class__(optimum_configs)  # return DataFrame

def plot_cor_matrix(df):
    cols = ["branch_pattern_table_size", "Fetch width", "Dispatch width", "Num. PREGS", "ROB entries",
            "reservation_station_size", "Num. ALU FUs", "Num. MUL FUs", "Num. LSU FUs", "IPC"]

    # calculate correlation matrix
    corr_matrix = df[cols].corr()

    plt.figure(figsize=(12, 6))
    sns.heatmap(corr_matrix,
                xticklabels=corr_matrix.columns,
                yticklabels=corr_matrix.columns,
                cmap='Blues',
                annot=True,
                center=0,
                annot_kws={'size': 12})
    plt.title('Correlation Matrix')
    plt.savefig(os.path.join(image_direc, "correlation_matrix.png"), bbox_inches='tight', dpi=300)
    print("Save corrlation_matrix.png to", image_direc)

def plot_box_IPC(df):
    # group by trace, analaye IPC
    plt.figure(figsize=(12, 6))
    sns.boxplot(x='trace', y='IPC', data=df)
    plt.title("BoxPlot of IPC by Trace")
    plt.savefig(os.path.join(image_direc, "IPC_box_plot.png"), bbox_inches='tight', dpi=300)
    print("Saved IPC_box_plot.png to", image_direc)

def plot_sactter_ICP_resource_utilizaition(df):
    traces = sorted(df['trace'].unique())

    for trace in traces:
        df_trace = df[df['trace'] == trace].copy()

        # Step 1: Find max IPC and 80% threshold
        max_ipc = df_trace['IPC'].max()
        threshold_80 = 0.8 * max_ipc

        # Step 2: Filter by IPC >= 80%
        df_filtered_80 = df_trace[df_trace['IPC'] >= threshold_80].copy()

        # Step 3: Filter lowest 10% resource utilization in the above
        ru_threshold = df_filtered_80['resource_utilization'].quantile(0.1)
        df_final = df_filtered_80[df_filtered_80['resource_utilization'] <= ru_threshold]

        # Step 4: Create figure for this trace only
        plt.figure(figsize=(8, 6))
        ax = plt.gca()

        # Step 5: Scatter filtered points
        ax.scatter(df_final['resource_utilization'], df_final['IPC'], color='blue', alpha=0.8, label='Filtered Configs')

        # Step 6: Draw 90% IPC horizontal line
        threshold_90 = 0.9 * max_ipc
        ax.axhline(threshold_90, color='green', linestyle='--', linewidth=1.5, label='90% IPC Line')

        # Step 7: Highlight best config (IPC ≥ 90% and min RU)
        df_above_90 = df_trace[df_trace['IPC'] >= threshold_90]
        if not df_above_90.empty:
            best_point = df_above_90.loc[df_above_90['resource_utilization'].idxmin()]
            ax.scatter(best_point['resource_utilization'], best_point['IPC'],
                       color='red', s=100, marker='*', label='Best Config')
            ax.annotate(
                f"IPC: {best_point['IPC']:.2f}\nRU: {best_point['resource_utilization']}",
                (best_point['resource_utilization'], best_point['IPC']),
                textcoords="offset points", xytext=(5, 5), ha='left', fontsize=8, color='red'
            )

        # Step 8: Format and save
        ax.set_title(f"Trace: {trace} (Filtered ≥80% IPC & ≤10% RU)")
        ax.set_xlabel("Resource Utilization")
        ax.set_ylabel("IPC")
        ax.legend()
        plt.tight_layout()

        save_path = os.path.join(image_direc, f"{trace}_scatter_filtered_best.png")
        plt.savefig(save_path, bbox_inches='tight', dpi=300)
        plt.close()
        print(f"Saved: {save_path}")

def plot_gselect_gsplit(df):
    filtered_df = df[
        (df['Num. PREGS'] == 32) &
        (df['ROB entries'] == 64) &
        (df['Num. SchedQ entries per FU'] == 4) &
        (df['Num. ALU FUs'] == 3) &
        (df['Num. MUL FUs'] == 2) &
        (df['Num. LSU FUs'] == 2) &
        (df['Fetch width'] == 4) &
        (df['Dispatch width'] == 4) # &
        # (df['branch_pattern_table_size'] == 16)
    ].copy()

    filtered_df['Branch Prediction Accuracy'] = (
        (filtered_df['Total branch instructions'] - filtered_df['Branch Mispredictions']) / 
        filtered_df['Total branch instructions']
    )

    filtered_df['x_label'] = 'H=' + filtered_df['H'].astype(str) + ', P=' + filtered_df['P'].astype(str)

    traces = filtered_df['trace'].unique()
    for trace_val in traces:
        subset = filtered_df[filtered_df['trace'] == trace_val]
        if subset.empty:
            print(f"[Skipped] Trace {trace_val} has no data.")
            continue

        gselect = subset[subset['Predictor (M)'] == 'GSELECT']
        gsplit = subset[subset['Predictor (M)'] == 'GSPLIT']

        plt.figure(figsize=(10, 6))
        ax1 = plt.gca()

        # Accuracy (left y)
        ax1.plot(gselect['x_label'], gselect['Branch Prediction Accuracy'], marker='o', color='tab:blue', label='GSELECT (Accuracy)')
        ax1.plot(gsplit['x_label'], gsplit['Branch Prediction Accuracy'], marker='o', color='tab:orange', label='GSPLIT (Accuracy)')
        ax1.set_ylabel('Branch Prediction Accuracy', color='tab:blue')
        ax1.tick_params(axis='y', labelcolor='tab:blue')
        ax1.set_xlabel('H and P')
        plt.xticks(rotation=45)

        # IPC (right y)
        ax2 = ax1.twinx()
        ax2.plot(gselect['x_label'], gselect['IPC'], marker='s', linestyle='--', color='tab:blue', label='GSELECT (IPC)')
        ax2.plot(gsplit['x_label'], gsplit['IPC'], marker='s', linestyle='--', color='tab:orange', label='GSPLIT (IPC)')
        ax2.set_ylabel('IPC', color='tab:orange')
        ax2.tick_params(axis='y', labelcolor='tab:orange')

        # Combine legends from both axes
        lines1, labels1 = ax1.get_legend_handles_labels()
        lines2, labels2 = ax2.get_legend_handles_labels()
        ax1.legend(lines1 + lines2, labels1 + labels2, loc='best')

        plt.title(f'Trace: {trace_val}')
        plt.tight_layout()
        save_path = os.path.join(image_direc, f"{trace_val}_accuracy_ipc_dual_axis.png")
        plt.savefig(save_path, dpi=300)
        plt.close()
        print(f"Saved: {save_path}")


if __name__ == '__main__':
    all_records = []
    for file_name in files_list:
        file_path = os.path.join(directory, file_name)
        all_records.extend(parse_file(file_path))
    
    df = pd.DataFrame(all_records)
    df.to_csv(output_csv, index=False)
    print(f"save data to {output_csv}")

    df = pd.read_csv(output_csv)

    df = add_branch_pattern_table_resveration_station_size(df)
    df = add_resource_utilization(df)
    df.to_csv(util_output_csv, index=False)
    print(f"Save utilization resource output to {util_output_csv}")

    df = pd.read_csv(util_output_csv)

    optimum_df = find_optimum_pipeline_configuration(df)
    optimum_df.to_csv(optim_output_csv, index=False)
    print(f"Save optimized configurations for each trace to {optim_output_csv}")

    plot_gselect_gsplit(df)
    plot_box_IPC(df)
    plot_cor_matrix(df)
    plot_sactter_ICP_resource_utilizaition(df)
