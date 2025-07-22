import os
import json
import requests
import numpy as np
import configparser
import sys
import time

if len(sys.argv) < 2:
    print("Config file not provided")
    sys.exit()

if os.path.exists(sys.argv[1]):
    pass
else:
    print("Config file does not exist")
    sys.exit()


config = configparser.ConfigParser()
config.read(sys.argv[1])

USRP_CAL_OFFSET = float(config['DEFAULT']['USRP_CAL_OFFSET'])


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
            os.system("rm rsrp.txt")

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
                
                if os.path.exists("rsrp.txt"):
                    with open('rsrp.txt', 'r') as f:
                        lines = f.readlines()
                        lines = [line.strip() for line in lines]
                        lines = [line.replace("reference_signal_received_power: ","") for line in lines]
                        lines = [line.replace("dBm","") for line in lines]
                        lines = [float(line) for line in lines]

                        rsrp_arr = np.array(lines) - USRP_CAL_OFFSET;
                        cell_res["rsrp"] = dict()
                        cell_res["rsrp"]["mean"] = np.mean(rsrp_arr)
                        cell_res["rsrp"]["median"] = np.median(rsrp_arr)
                        cell_res["rsrp"]["std"] = np.std(rsrp_arr)
                        cell_res["rsrp"]["max"] = np.max(rsrp_arr)
                        cell_res["rsrp"]["min"] = np.min(rsrp_arr)
                else:
                        cell_res["rsrp"] = dict()
                        cell_res["rsrp"]["mean"] = -10000
                        cell_res["rsrp"]["median"] = -10000
                        cell_res["rsrp"]["std"] = -10000
                        cell_res["rsrp"]["max"] = -10000
                        cell_res["rsrp"]["min"] = -10000
            list.append(cell_res)
            with open("cell_scan_detail.json", 'w') as file: 
                json.dump(list,file)
        else:
            break
    else:
        break



