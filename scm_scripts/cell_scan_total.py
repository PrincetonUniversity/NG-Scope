import os
import json
import requests
import numpy as np
import configparser
import sys
import time

lte_bands = 1.0 * np.array([
    [1, 2110, 0, 18000, 190, 0],
    [2, 1930, 600, 18600, 80, 1],
    [3, 1805, 1200, 19200, 95, 0],
    [4, 2110, 1950, 19950, 400, 1],
    [5, 869, 2400, 20400, 45, 1],
    [6, 875, 2650, 20650, 45, 2],
    [7, 2620, 2750, 20750, 120, 3],
    [8, 925, 3450, 21450, 45, 0],
    [9, 1844.9, 3800, 21800, 95, 2],
    [10, 2110, 4150, 22150, 400, 1],
    [11, 1475.9, 4750, 22750, 48, 4],
    [12, 729, 5010, 23010, 30, 1],
    [13, 746, 5180, 23180, -31, 1],
    [14, 758, 5280, 23280, -30, 1],
    [17, 734, 5730, 23730, 30, 1],
    [18, 860, 5850, 23850, 45, 4],
    [19, 875, 6000, 24000, 45, 4],
    [20, 791, 6150, 24150, -41, 3],
    [21, 1495.9, 6450, 24450, 48, 4],
    [22, 3500, 6600, 24600, 100, 6],
    [23, 2180, 7500, 25500, 180, 1],
    [24, 1525, 7700, 25700, -101.5, 1],
    [25, 1930, 8040, 26040, 80, 1],
    [26, 859, 8690, 26690, 45, 1],
    [27, 852, 9040, 27040, 45, 1],
    [28, 758, 9210, 27210, 55, 2],
    [29, 717, 9660, 0, 0, 1],
    [30, 2350, 9770, 27660, 45, 1],
    [31, 462.5, 9870, 27760, 10, 5],
    [32, 1452, 9920, 0, 0, 3],
    [33, 1900, 36000, 0, 0, 3],
    [34, 2010, 36200, 0, 0, 3],
    [35, 1850, 36350, 0, 0, 1],
    [36, 1930, 36950, 0, 0, 1],
    [37, 1910, 37550, 0, 0, 1],
    [38, 2570, 37750, 0, 0, 3],
    [39, 1880, 38250, 0, 0, 2],
    [40, 2300, 38650, 0, 0, 2],
    [41, 2496, 39650, 0, 0, 0],
    [42, 3400, 41590, 0, 0, 0],
    [43, 3600, 43590, 0, 0, 0],
    [44, 703, 45590, 0, 0, 2],
    [45, 1447, 46590, 0, 0, 2],
    [46, 5150, 46790, 0, 0, 0],
    [47, 5855, 54540, 0, 0, 0],
    [48, 3550, 55240, 0, 0, 0],
    [49, 3550, 56740, 0, 0, 0],
    [50, 1432, 58240, 0, 0, 0],
    [51, 1427, 59090, 0, 0, 0],
    [52, 3300, 59140, 0, 0, 0],
    [64, 0, 60140, 27810, 0, 0], 
    [65, 2110, 65536, 131072, 190, 0],
    [66, 2110, 66436, 131972, 400, 1],
    [67, 738, 67336, 0, 0, 3],
    [68, 753, 67536, 132672, 55, 3],
    [69, 2570, 67836, 0, 0, 3],
    [70, 1995, 68336, 132972, 300, 1],
    [71, 617, 68586, 133122, -46, 1],
    [72, 0, 68936, 133472, 0, 1]
])

print(lte_bands.shape)


list = []

for i in range(lte_bands.shape[0] - 1):
    if (lte_bands[i][5] == 1):
        freq_low = lte_bands[i][1]
        freq_high = freq_low + 0.1*(lte_bands[i+1][2] - lte_bands[i][2])

        curr_freq = freq_low

        while(curr_freq <= freq_high):
            test_freq = round(float(curr_freq) * 1000000)
            print("Testing Frequency: " + str(test_freq / 1000000))
            os.system("./gen_config.sh " + str(test_freq) + "L")
            os.system("rm mib_results.json")
            os.system("rm cellcfg.json")
            os.system("rm cell_type.json")

            
            os.system("timeout 20s ./ngscope -c temp_config.cfg")
            time.sleep(2)

            if os.path.exists("mib_results.json"):
                cell_res = dict()
                with open("mib_results.json", 'r') as file:
                    mib = json.load(file)

                cell_res["mib"] = dict()
                cell_res["mib"]["id"] = mib["id"]
                cell_res["mib"]["freq"] = curr_freq
                cell_res["mib"]["nprb"] = mib["nprb"]
                cell_res["mib"]["pss_power"] = mib["pss_power"]

                if os.path.exists("cellcfg.json"):
                    cell_res["sib_found"] = 1
                    with open("cellcfg.json", 'r') as file:
                        cell_cfg = json.load(file)

                    with open("cell_type.json", 'r') as file:
                        cell_info = json.load(file)
                    cell_info.update(cell_cfg)
                    cell_res["sib"] = dict()
                    cell_res["sib"].update(cell_info)
                else:
                    cell_res["sib_found"] = 0
                    cell_res["sib"] = dict()
                list.append(cell_res)
                with open("cell_scan_detail.json", 'w') as file: 
                    json.dump(list,file)

            curr_freq = curr_freq + 0.1
                    

