import json
from itertools import product

# GSelect arguements space
gselect_HP = [(6, 9), (8, 7), (6, 10), (8, 8), (10, 6)]
# GSplit arguements space
gsplit_HP = [(8, 1), (10, 5), (12, 9), (14, 13), (8, 0), (10, 4), (12, 8), (14, 12), (16, 16)]
# s arguments sapce
s_values = [4, 6, 8]
# a, m, l, f arguments space
fu_configs = [
    (1, 1, 1, 2), (1, 1, 1, 4), (1, 1, 1, 8), (1, 1, 2, 2), (1, 1, 2, 4), (1, 1, 2, 8),
    (2, 1, 1, 2), (2, 1, 1, 4), (2, 1, 1, 8), (2, 1, 2, 2), (2, 1, 2, 4), (2, 1, 2, 8),
    (2, 2, 1, 2), (2, 2, 1, 4), (2, 2, 1, 8), (2, 2, 2, 2), (2, 2, 2, 4), (2, 2, 2, 8),
    (3, 2, 2, 4), (3, 2, 2, 8), (3, 3, 2, 4), (3, 3, 2, 8)
]
# p arguments space
p_values = [32, 64, 128]

gselect_space = list(product([1], gselect_HP, s_values, fu_configs, p_values))
gsplit_space = list(product([2], gsplit_HP, s_values, fu_configs, p_values))

# merge together
total_configurations = gselect_space + gsplit_space

expanded_configs = []
for b, (H, P), s, (a, m, l, f), p in total_configurations:
    expanded_configs.append({
        'b': b,
        'H': H,
        'P': P,
        's': s,
        'a': a,
        'm': m,
        'l': l,
        'f': f,
        'p': p
    })

# Save to a JSON file
with open('configurations.json', 'w') as f:
    json.dump(expanded_configs, f, indent=2)
