import os
import re
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

dr = "./experiments"
plot_dr = "./images"
output_csv = "experiments.csv"
result_csv = "best_config.csv"

traces = ["gcc", "leela", "linpack", "matmul_naive", "matmul_tiled", "mcf"]
L1_config_pattern = r"L1 \(C,B,S\): \((\d+),(\d+),(\d+)\). Replace policy: MIP"
L2_config_pattern = r"L2 \(C,B,S\): \((\d+),(\d+),(\d+)\). Replace policy: (\w+). Early Restart: (\w+)"
vc_config_pattern = r"Victim cache entries: (\d+)"
L1_AAT_pattern = r"L1 average access time \(AAT\): (\d*\.\d+)"
L2_AAT_pattern = r"L2 average access time \(AAT\): (\d*\.\d+)"


def extract_from_files(dr, traces):
    file_paths = [os.path.join(dr, trace + ".out") for trace in traces]
    data_list = []

    for i, file_path in enumerate(file_paths):
        with open(file_path, "r", encoding="utf8") as input_file:
            P = ""
            E = False
            L1_AAT, L2_AAT = 0.0, 0.0
            c, b, s, v, C, S, = 0, 0, 0, 0, 0, 0
            L1_DS, L1_TS, L2_DS, L2_TS = 0, 0, 0, 0

            for line in input_file:
                if line.startswith("L1 (C,B,S):"):
                    c, b, s = tuple(map(int, re.search(L1_config_pattern, line).groups()))
                    L1_DS = 2**c
                    L1_TS = (2**(c - b) * (64 - (c - s) + 2)) // 8

                if line.startswith("Victim cache entries:"):
                    v = int(re.search(vc_config_pattern, line).groups()[0])

                if line.startswith("L2 (C,B,S):"):
                    matches = re.search(L2_config_pattern, line).groups()
                    C, b, S = tuple(map(int, matches[:3]))
                    L2_DS = 2**C
                    L2_TS = (2**(C - b) * (64 - (C - S) + 1)) // 8  # convert to bytes
                    P = matches[3]
                    E = True if matches[4] == "Enabled" else False

                if line.startswith("L1 average access time (AAT):"):
                    L1_AAT = float(re.search(L1_AAT_pattern, line).groups()[0])

                if line.startswith("L2 average access time (AAT):"):
                    L2_AAT = float(re.search(L2_AAT_pattern, line).groups()[0])

                    data_list.append({
                        "Trace": traces[i],
                        "L1_AAT": L1_AAT,
                        "L2_AAT": L2_AAT,
                        "L1_DS": L1_DS,
                        "L1_TS": L1_TS,
                        "L2_DS": L2_DS,
                        "L2_TS": L2_TS,
                        "c": c,
                        "b": b,
                        "s": s,
                        "v": v,
                        "C": C,
                        "S": S,
                        "P": P,
                        "E": E
                    })
    
    return pd.DataFrame(data_list)


def find_best_configs(df):
    best_configs = []
    
    for trace in df['Trace'].unique():
        trace_df = df[df['Trace'] == trace]
        
        # Find minimum total AAT for this trace
        min_total_AAT = trace_df['total_AAT'].min()
        min_AAT_configs = trace_df[trace_df['total_AAT'] == min_total_AAT]
        
        # Among those, find the one with minimum total size
        best_config = min_AAT_configs.loc[min_AAT_configs['total_size'].idxmin()]
        best_configs.append(best_config)
    
    results_df = pd.DataFrame(best_configs)
    
    return results_df

    
def plot_line_AAT_Storage(df, cache_level, group_cols, output_dir):
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    avg_df = df.groupby(group_cols)[['total_AAT', 'total_size']].mean().reset_index()
    x_col = 's' if cache_level == 'L1' else 'S'
    palette = 'Set2' if cache_level == 'L1' else 'Set1'

    fig, ax1 = plt.subplots(figsize=(12, 6))
    sns.lineplot(ax=ax1, data=avg_df, x=x_col, y='total_AAT', hue='b', 
                    marker='o', markersize=10, linewidth=2.5, linestyle='-', palette=palette, legend=True)
    ax1.set_xlabel(f'{cache_level} Set Associativity ({x_col})')
    ax1.set_ylabel('Average Total Access Time (AAT) / ns', color='tab:blue')
    ax1.tick_params(axis='y', labelcolor='tab:blue')

    ax2 = ax1.twinx()
    sns.lineplot(ax=ax2, data=avg_df, x=x_col, y='total_size', hue='b', 
                    marker='s', markersize=10, linewidth=2.5, linestyle='--', palette=palette, legend=True)
    ax2.set_ylabel('Average Total Cache Storage / bytes', color='tab:red')
    ax2.tick_params(axis='y', labelcolor='tab:red')

    # Combine legends
    lines1, labels1 = ax1.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    combined_lines = lines1 + lines2
    combined_labels = [f'{2**int(b)}B (AAT)' for b in avg_df['b'].unique()] + \
                    [f'{2**int(b)}B (Storage)' for b in avg_df['b'].unique()]

    ax1.get_legend().remove()
    ax2.get_legend().remove()
    ax1.legend(combined_lines, combined_labels, title='Block Size', 
            loc='upper left', bbox_to_anchor=(1.15, 0.5))

    plt.title(f'Average Total Cache AAT and Storage vs {cache_level} Set Associativity')
    plt.tight_layout(rect=[0, 0, 1, 0.95])
    plt.savefig(os.path.join(output_dir, f'avg_{cache_level}_AAT_storage.png'), dpi=300)
    plt.close()

    print(f"\nAverage Total AAT and Storage for each Trace, set associativity and block size:")
    print(avg_df.to_string(index=False))


def plot_bar_trace(df, x_col, y_col, x_str, y_str, output_dir):
    print(y_str)
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    avg_df = df.groupby(['Trace', x_col])[y_col].mean().reset_index()

    plt.figure(figsize=(12, 6))
    sns.barplot(data=avg_df, x='Trace', y=y_col, hue=x_col, palette='Set1')
    plt.title(f'{y_str} vs {x_str}')
    plt.xlabel('Trace')
    plt.ylabel(f'{y_str}')
    plt.grid(True, alpha=0.3)
    plt.legend(title=f'{x_str}')
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir , f'{x_str}_vs_{y_str}.png'), dpi=300)
    plt.close()

    print(f"\n{y_str} for each Trace and {x_str}:")
    print(avg_df.to_string(index=False))
    

if __name__ == "__main__":
    # Set the style for all plots
    sns.set_theme(style="whitegrid")

    # Extract from experiments files
    traces_df = extract_from_files(dr, traces)
    traces_df['total_AAT'] = traces_df['L1_AAT'] + traces_df['L2_AAT']
    traces_df['total_size'] = traces_df['L1_DS'] + traces_df['L1_TS'] + traces_df['L2_DS'] + traces_df['L2_TS']
    
    # Plot some graphs
    plot_line_AAT_Storage(traces_df, 'L1', ['s', 'b'], plot_dr)
    plot_line_AAT_Storage(traces_df, 'L2', ['S', 'b'], plot_dr)
    plot_bar_trace(traces_df, 'v', 'total_AAT', 'Victim Cache Entry', 'Average Total AAT', plot_dr)
    plot_bar_trace(traces_df, 'P', 'total_AAT', 'Replacement Policy', 'Average Total AAT', plot_dr)
    plot_bar_trace(traces_df, 'E', 'total_AAT', 'Early Restart', 'Average Total AAT', plot_dr)
    
    # Find the best configs
    results_df = find_best_configs(traces_df)
    print("Best configurations:")
    for _, config in results_df.iterrows():
        print("="*50)
        print(f"Trace: {config['Trace']}")
        print(f"Total AAT: {config['total_AAT']:.3f} ns")
        print(f"Total Size: {config['total_size']} bytes")
        print("\nCache Parameters:")
        print(f"L1 - c: {config['c']}, b: {config['b']}, s: {config['s']}")
        print(f"L2 - C: {config['C']}, S: {config['S']}")
        print(f"Victim Cache Entries: {config['v']}")
        print(f"Replace Policy: {config['P']}")
        print(f"Early Restart: {'Enabled' if config['E'] else 'Disabled'}")

    # Save to csv
    traces_df.to_csv(output_csv, index=False)
    print(f"\nSave extracted data to f{output_csv}")
    results_df.to_csv(result_csv, index=False)
    print(f"Save best configurations to f{result_csv}")