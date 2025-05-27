import os
import json
import requests


with open("../../cellscanner/source/cell_scan_results.json", 'r') as file:
    cell_scan = json.load(file)


for i in range(len(cell_scan)):
    cell_scan[i]["metadata"] = dict()
    cell_scan[i]["metadata"]["failed_attempts"] = 0;



with open("cell_list.json", 'w') as file:
    json.dump(cell_scan,file)