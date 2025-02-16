import os
import re

traces = ["gcc", "leela", "linpack", "matmul_naive", "matmul_tiled", "mcf"]
dr = "/experiments"
L1_config_pattern = r"L1 \(C,B,S\): \((\d+),(\d+),(\d+)\). Replace policy: MIP"
L2_config_pattern = r"L2 \(C,B,S\): \((\d+),(\d+),(\d+)\). Replace policy: (\w+). Early Restart: (\w+)"
vc_config_pattern = r"Victim cache entries: (\d+)"
L1_AAT_pattern = r"L1 average access time \(AAT\): (\d*\.\d+)"
L2_AAT_pattern = r"L2 average access time \(AAT\): (\d*\.\d+)"


def data_storage(config):
    return 2**int(config[0])


def tag_storage(config):
    meta_data = 1 if len(config) > 3 else 2
    C, B, S = int(config[0]), int(config[1]), int(config[2])
    return int(2**(C - B) * (64 - (C - S) + meta_data) / 8)  # convert to bytes


def extract_from_files():
    # file_paths = [os.path.join(dr, trace + ".out") for trace in traces]
    file_paths = ["./test.txt"]

    dict_trace = {}
    for i, file_path in enumerate(file_paths):
        dict_trace[traces[i]] = []
        with open(file_path, "r", encoding="utf8") as input_file:
            L1_config, vc_config, L2_config = tuple(), tuple(), tuple()
            L1_AAT, L2_AAT, L1_DS, L1_TS, L2_DS, L2_TS = 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
            for line in input_file:
                if line.startswith("L1 (C,B,S):"):
                    L1_config = re.search(L1_config_pattern, line).groups()
                    L1_DS = data_storage(L1_config)
                    L1_TS = tag_storage(L1_config)
                if line.startswith("Victim cache entries:"):
                    vc_config = re.search(vc_config_pattern, line).groups()
                if line.startswith("L2 (C,B,S):"):
                    L2_config = re.search(L2_config_pattern, line).groups()
                    L2_DS = data_storage(L2_config)
                    L2_TS = tag_storage(L2_config)
                if line.startswith("L1 average access time (AAT):"):
                    L1_AAT = float(re.search(L1_AAT_pattern, line).groups()[0])
                if line.startswith("L2 average access time (AAT):"):
                    L2_AAT = float(re.search(L2_AAT_pattern, line).groups()[0])
                    dict_trace[traces[i]].append((L1_AAT, L2_AAT, L1_DS, L1_TS, L2_DS, L2_TS, L1_config, vc_config, L2_config))
    
    return dict_trace


if __name__== "__main__":
    dict_trace = extract_from_files()
    print(dict_trace)

    for trace, outputs in dict_trace.items():
        best_config = min(outputs, key=lambda x: (x[0] + x[1], x[2] + x[3] + x[4] + x[5]))
        print(f"The best_config is when L1: {best_config[6]}, vc: {best_config[7]}, L2: {best_config[8]}")
