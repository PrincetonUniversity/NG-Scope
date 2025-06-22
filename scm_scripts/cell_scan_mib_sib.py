import os
import json
import requests
import numpy as np
import configparser
import sys
import time


os.system("rm temp_scan.json")

probe_freq_idx = 0
probe_band_idx = 0

list = []
while(1):
    os.system("../../cellscanner/source/cellscanner \"temp_scan.json\" 1 \"\" 1 " + str(probe_band_idx) + " " + str(probe_freq_idx))
    if os.path.exists("temp_scan.json"):
        with open("temp_scan.json", 'r') as file:
            scan_res = json.load(file)
        if len(scan_res) > 0:
            cell_res = dict()
            cell_res["scan"] = scan_res[0]
            probe_freq_idx = int(scan_res[0]["freq_idx"])
            probe_band_idx = int(scan_res[0]["band_idx"])
            curr_freq = scan_res[0]["freq"]
            test_freq = round(float(curr_freq) * 1000000)
        
            print("Detail Probe: " + str(test_freq / 1000000))
            os.system("./gen_config.sh " + str(test_freq) + "L")
            os.system("rm mib_results.json")
            os.system("rm cellcfg.json")
            os.system("rm cell_type.json")
            os.system("timeout 20s ./ngscope -c temp_config.cfg")
            time.sleep(5)

            if os.path.exists("mib_results.json"):
                with open("mib_results.json", 'r') as file:
                    mib = json.load(file)

                cell_res["mib"] = dict()
                cell_res["mib"]["id"] = mib["id"]
                cell_res["mib"]["freq"] = curr_freq
                cell_res["mib"]["nprb"] = mib["nprb"]

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
        else:
            break
    else:
        break



