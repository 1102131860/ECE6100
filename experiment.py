import subprocess

# define experiments and policies
policies = ["MI", "MSI", "MESI", "MOSI", "MOESIF"]
experiments = [f"experiment{i}" for i in range(1, 6)]

# executative file path
sim_trace_exe = "./sim_trace"

for exp in experiments:
    output_file = f"record_{exp}.out"
    with open(output_file, 'w') as outfile:
        for policy in policies:
            cmd = [sim_trace_exe, "-t", f"experiments/{exp}", "-p", policy]
            outfile.write(f"=== Experiment: {exp}, Policy: {policy} ===\n")
            result = subprocess.run(cmd, capture_output=True, text=True)
            outfile.write(result.stdout)
            outfile.write("\n\n")
    print(f"{output_file} written.")
