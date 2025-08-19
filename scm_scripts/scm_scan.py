import os
import json
import requests
import numpy as np
import configparser
import sys
import time
from io import StringIO

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

APIKEY = str(config['DEFAULT']['apikey'])
probe_lat = float(config['DEFAULT']['probe_lat']) 
probe_lng = float(config['DEFAULT']['probe_lng'])
USRP_CAL_OFFSET = float(config['DEFAULT']['USRP_CAL_OFFSET'])
EXTERNAL_GAIN_OFFSET = float(config['DEFAULT']['EXTERNAL_GAIN_OFFSET'])


os.system("rm temp_scan.json")



probe_freq_idx = 0
probe_band_idx = 0

list = []
while(1):
    os.system("../../cellscanner/source/cellscanner \"temp_scan.json\" 1 \"\" 1 " + str(probe_band_idx) + " " + str(probe_freq_idx))
    if os.path.exists("temp_scan.json"):
        try:
            with open("temp_scan.json", 'r') as file:
                scan_res = json.load(file)
        except Exception as e:
            continue

        if len(scan_res) > 0:
            cell_res = dict()
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
            os.system("rm rsrp_prbs.txt")
            os.system("rm chest_dl_est_config.txt")
            os.system("rm chest_dl_est_real.txt")
            os.system("rm chest_dl_est_imag.txt")

            os.system("timeout 20s ./ngscope -c temp_config.cfg")
            time.sleep(5)

            #Create default json object 

            cell_res["id"] = -10000
            cell_res["enb_id"] = -10000
            cell_res["frame_type"] = "N/A"
            cell_res["bandwidth"] = -10000
            cell_res["centerFreq"] = -10000
            cell_res["nprb"] = -10000
            cell_res["probe_lat"] = -10000
            cell_res["probe_lng"] = -10000
            cell_res["lat"] = -10000
            cell_res["lng"] = -10000
            cell_res["accuracy"] = -10000
            cell_res["mcc"] = -10000
            cell_res["mnc"] = -10000
            cell_res["tac"] = -10000
            cell_res["pdsch_reference_signal_power_dbm"] = -10000

            cell_res["rsrp"] = dict()
            cell_res["rsrp"]["mean"] = -10000
            cell_res["rsrp"]["median"] = -10000
            cell_res["rsrp"]["std"] = -10000
            cell_res["rsrp"]["max"] = -10000
            cell_res["rsrp"]["min"] = -10000

            cell_res["rsrp_per_rb"] = dict()
            cell_res["rsrp_per_rb"]["mean"] = []
            cell_res["rsrp_per_rb"]["median"] = []
            cell_res["rsrp_per_rb"]["std"] = []
            cell_res["rsrp_per_rb"]["max"] = []
            cell_res["rsrp_per_rb"]["min"] = []

            cell_res["measurement_time"] = dict()
            cell_res["measurement_time"]["year"] = -10000
            cell_res["measurement_time"]["month"] = -10000
            cell_res["measurement_time"]["day"] = -10000
            cell_res["measurement_time"]["hour"] = -10000
            cell_res["measurement_time"]["min"] = -10000
            cell_res["measurement_time"]["sec"] = -10000

            cell_res["id"] = scan_res[0]["cell_id"]
            cell_res["centerFreq"] = curr_freq 
            cell_res["nprb"] =  scan_res[0]["prbs"]



            cell_res["raw_data"] = dict()
            cell_res["raw_data"]["pss_scan"] = scan_res[0]


            if os.path.exists("mib_results.json"):
                with open("mib_results.json", 'r') as file:
                    mib = json.load(file)

                cell_res["id"] = mib["id"]
                cell_res["centerFreq"] = curr_freq
                cell_res["nprb"] = mib["nprb"]

                cell_res["raw_data"]["mib"] = dict()
                cell_res["raw_data"]["mib"]["id"] = mib["id"]
                cell_res["raw_data"]["mib"]["freq"] = curr_freq
                cell_res["raw_data"]["mib"]["nprb"] = mib["nprb"]


                if os.path.exists("cellcfg.json"):
                    with open("cellcfg.json", 'r') as file:
                        cell_cfg = json.load(file)

                    with open("cell_type.json", 'r') as file:
                        cell_info = json.load(file)

                    cell_info.update(cell_cfg)

                    cell_res["id"] = cell_info["id"]
                    cell_res["enb_id"] = cell_info["enb_id"]
                    cell_res["frame_type"] = cell_info["frame_type"]
                    cell_res["bandwidth"] = cell_info["bandwidth"]
                    cell_res["centerFreq"] = cell_info["centerFreq"]
                    cell_res["mcc"] = cell_info["mcc"]
                    cell_res["mnc"] = cell_info["mnc"]
                    cell_res["tac"] = cell_info["tac"]
                    cell_res["pdsch_reference_signal_power_dbm"] = cell_info["pdsch_reference_signal_power_dbm"]
                    cell_res["measurement_time"]["year"] = cell_info["measurement_time"]["year"]
                    cell_res["measurement_time"]["month"] = cell_info["measurement_time"]["month"]
                    cell_res["measurement_time"]["day"] = cell_info["measurement_time"]["day"]
                    cell_res["measurement_time"]["hour"] = cell_info["measurement_time"]["hour"]
                    cell_res["measurement_time"]["min"] = cell_info["measurement_time"]["min"]
                    cell_res["measurement_time"]["sec"] = cell_info["measurement_time"]["sec"]
                    cell_res["probe_lat"] = probe_lat
                    cell_res["probe_lng"] = probe_lng


                    cell_info["probe_lat"] = probe_lat
                    cell_info["probe_lng"] = probe_lng

                    query = dict()
                    query["radioType"] = "lte" 
                    query["cellTowers"] = dict()
                    query["cellTowers"]["cellId"] = cell_cfg["id"]
                    #query["cellTowers"]["locationAreaCode"] = cell_cfg["tac"]
                    query["cellTowers"]["mobileCountryCode"] = cell_cfg["mcc"]
                    query["cellTowers"]["mobileNetworkCode"] = cell_cfg["mnc"]
                    query["considerIp"] = "false"


                    json_string = json.dumps(query, indent=4)
                    print(json_string)

                    url = "https://www.googleapis.com/geolocation/v1/geolocate?key=" + APIKEY
                    headers = {"Content-Type": "application/json"}
                    response = requests.post(url, headers=headers, json=query)

                    if response.status_code == 200:
                        # Process the response data
                        print(response.json())
                        cell_res["lat"] = response.json()["location"]['lat']
                        cell_res["lng"] = response.json()["location"]['lng']
                        cell_res["accuracy"] = response.json()["accuracy"]

                    else:
                        # Handle the error
                        print(f"Error: {response.status_code}")
                        print(response.text)
                        cell_res["lat"] = "-10000"
                        cell_res["lng"] = "-10000"
                        cell_res["accuracy"] = "-10000"

                    
                    
                    
                    cell_res["raw_data"]["sib"] = dict()
                    cell_res["raw_data"]["sib"].update(cell_info)

                    #Process CSI Data

                    if os.path.exists("chest_dl_est_config.txt"):
                        with open('chest_dl_est_config.txt', 'r') as f:
                            lines = f.readlines()
                            lines = lines[0:4]
                            lines = [line.strip() for line in lines]
                            nof_ports = int(lines[0])
                            nof_antennas = int(lines[1])
                            nof_re = int(lines[2])
                            nof_sc = int(lines[3]) * 12
                            nof_sym = int(nof_re/nof_sc) 

                        with open('chest_dl_est_real.txt', 'r') as f:
                            lines = f.readlines()
                            lines = [line.strip() for line in lines]
                            lines = [line.split(",") for line in lines]
                            chest_real = np.array(lines).astype(float)

                        with open('chest_dl_est_imag.txt', 'r') as f:
                            lines = f.readlines()
                            lines = [line.strip() for line in lines]

                            lines = [line.split(",") for line in lines]
                            chest_imag = np.array(lines).astype(float)


                        chest = (chest_real) + 1j * (chest_imag)
                        chest = chest.reshape((chest.shape[0],nof_sym,nof_sc))
                        
                        csi_file_name = "csi_dl_" + str(cell_res["enb_id"]) + "_" + str(cell_res["centerFreq"]) + "_data.npz"

                        np.savez(csi_file_name,chest)
                else:
                    cell_res["raw_data"]["sib"] = dict()
                
                if os.path.exists("rsrp.txt"):
                    with open('rsrp.txt', 'r') as f:
                        lines = f.readlines()
                        lines = [line.strip() for line in lines]
                        lines = [line.replace("reference_signal_received_power: ","") for line in lines]
                        lines = [line.replace("dBm","") for line in lines]
                        lines = [float(line) for line in lines]

                        rsrp_arr = np.array(lines) - USRP_CAL_OFFSET - EXTERNAL_GAIN_OFFSET;
                        cell_res["rsrp"] = dict()
                        cell_res["rsrp"]["mean"] = np.mean(rsrp_arr)
                        cell_res["rsrp"]["median"] = np.median(rsrp_arr)
                        cell_res["rsrp"]["std"] = np.std(rsrp_arr)
                        cell_res["rsrp"]["max"] = np.max(rsrp_arr)
                        cell_res["rsrp"]["min"] = np.min(rsrp_arr)
                if os.path.exists("rsrp_prbs.txt"):
                    with open('rsrp_prbs.txt', 'r') as f:
                        lines = f.readlines()
                        lines = [line.strip() for line in lines]
                        lines = [line for line in lines]
    
                        rsrp_array = []

                        for line in lines:
                            data = StringIO(line)
                            numpy_array = np.genfromtxt(data, delimiter=',')    
                            rsrp_array.append(numpy_array)

                        rsrp_array = np.array(rsrp_array) - USRP_CAL_OFFSET - EXTERNAL_GAIN_OFFSET
                        cell_res["rsrp_per_rb"]["mean"] = np.mean(rsrp_array,axis=0).tolist()
                        cell_res["rsrp_per_rb"]["median"] = np.median(rsrp_array,axis=0).tolist()
                        cell_res["rsrp_per_rb"]["std"] = np.std(rsrp_array,axis=0).tolist()
                        cell_res["rsrp_per_rb"]["max"] = np.max(rsrp_array,axis=0).tolist()
                        cell_res["rsrp_per_rb"]["min"] = np.min(rsrp_array,axis=0).tolist()
                else:
                        cell_res["rsrp_per_rb"]["mean"] = []
                        cell_res["rsrp_per_rb"]["median"] = []
                        cell_res["rsrp_per_rb"]["std"] = []
                        cell_res["rsrp_per_rb"]["max"] = []
                        cell_res["rsrp_per_rb"]["min"] = []
            list.append(cell_res)
            with open("cell_scan_detail.json", 'w') as file: 
                json.dump(list,file)
        else:
            break
    else:
        break



