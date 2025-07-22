import os
import json
import requests
import numpy as np
import configparser
import sys

# APIKEY = ""
# probe_lat = 40.350288669177814 
# probe_lng = -74.65208898358767
# MAX_CELL_RETRY = 5


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
config['DEFAULT']

APIKEY = str(config['DEFAULT']['apikey'])
probe_lat = float(config['DEFAULT']['probe_lat']) 
probe_lng = float(config['DEFAULT']['probe_lng'])
MAX_CELL_RETRY = int(config['DEFAULT']['max_cell_retry'])
USRP_CAL_OFFSET = float(config['DEFAULT']['USRP_CAL_OFFSET'])

os.system("rm scm_results.json")
os.system("touch scm_results.json")
os.system("rm rsrp.txt")
# with open("../../cellscanner/source/cell_scan_results.json", 'r') as file:
#     cell_scan = json.load(file)

with open("cell_list.json", 'r') as file:
    cell_scan = json.load(file)

for i in range(len(cell_scan)):
    if(float(cell_scan[i]["metadata"]["failed_attempts"]) >= MAX_CELL_RETRY):
        print("Skipping cell, max attempts reached")
        continue

    test_freq = round(float(cell_scan[i]["freq"]) * 1000000)
    
    os.system("./gen_config.sh " + str(test_freq) + "L")

    os.system("rm cellcfg.json")
    os.system("rm cell_type.json")

    os.system("timeout 20s ./ngscope -c temp_config.cfg")

    if os.path.exists("cellcfg.json"):

        cell_scan[i]["metadata"]["failed_attempts"] = 0

        with open("cellcfg.json", 'r') as file:
            cell_cfg = json.load(file)

        with open("cell_type.json", 'r') as file:
            cell_info = json.load(file)

        if os.path.exists("rsrp.txt"):
            with open('rsrp.txt', 'r') as f:
                lines = f.readlines()
                lines = [line.strip() for line in lines]
                lines = [line.replace("reference_signal_received_power: ","") for line in lines]
                lines = [line.replace("dBm","") for line in lines]
                lines = [float(line) for line in lines]

                rsrp_arr = np.array(lines) - USRP_CAL_OFFSET;
                cell_info["rsrp"] = dict()
                cell_info["rsrp"]["mean"] = np.mean(rsrp_arr)
                cell_info["rsrp"]["median"] = np.median(rsrp_arr)
                cell_info["rsrp"]["std"] = np.std(rsrp_arr)
                cell_info["rsrp"]["max"] = np.max(rsrp_arr)
                cell_info["rsrp"]["min"] = np.min(rsrp_arr)


        else:
                cell_info["rsrp"] = dict()
                cell_info["rsrp"]["mean"] = -10000
                cell_info["rsrp"]["median"] = -10000
                cell_info["rsrp"]["std"] = -10000
                cell_info["rsrp"]["max"] = -10000
                cell_info["rsrp"]["min"] = -10000
                

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
            cell_info["lat"] = response.json()["location"]['lat']
            cell_info["lng"] = response.json()["location"]['lng']
            cell_info["accuracy"] = response.json()["accuracy"]

        else:
            # Handle the error
            print(f"Error: {response.status_code}")
            print(response.text)
            cell_info["lat"] = "-10000"
            cell_info["lng"] = "-10000"
            cell_info["accuracy"] = "-10000"




        cell_info.update(cell_cfg)
        list = []
        with open("scm_results.json", 'r') as file: 
            try: 
                scm_entries = json.load(file)
                list.extend(scm_entries)
                list.append(cell_info)
            except:
                list.append(cell_info)

        with open("scm_results.json", 'w') as file: 
            json.dump(list,file)
    else:
        #NG-Scope cannot read cell
        cell_scan[i]["metadata"]["failed_attempts"] = float(cell_scan[i]["metadata"]["failed_attempts"]) + 1


with open("cell_list.json", 'w') as file:
    json.dump(cell_scan,file)





