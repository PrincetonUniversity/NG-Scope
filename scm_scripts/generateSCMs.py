import os
import json
import requests


os.system("rm scm_results.json")
os.system("touch scm_results.json")



with open("../../cellscanner/source/cell_scan_results.json", 'r') as file:
    cell_scan = json.load(file)


for i in range(len(cell_scan)):
    test_freq = round(float(cell_scan[i]["freq"]) * 1000000)
    
    os.system("./gen_config.sh " + str(test_freq) + "L")

    os.system("rm cellcfg.json")
    os.system("rm cell_type.json")

    os.system("timeout 20s ./ngscope -c temp_config.cfg")

    if os.path.exists("cellcfg.json"):
        with open("cellcfg.json", 'r') as file:
            cell_cfg = json.load(file)

        with open("cell_type.json", 'r') as file:
            cell_info = json.load(file)

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

        url = "https://www.googleapis.com/geolocation/v1/geolocate?key=API-KEY"
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

