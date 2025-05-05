import folium
from folium.plugins import MarkerCluster
import json
from collections import defaultdict


m = folium.Map(location=[40.350288669177814, -74.65208898358767], zoom_start=20)


with open("scm_results.json", 'r') as file:
    scm_results = json.load(file)


cell_loc = defaultdict(list)
for i in range(len(scm_results)):
    cell_loc[scm_results[i]["lat"],scm_results[i]["lng"]].append(scm_results[i])


plot_ids = []

for lt,lg in cell_loc:
    if (lt == -10000):
        continue
    pop_up_text = ""
    depth = 0
    for cell in cell_loc[lt,lg]:

        if cell["id"] in plot_ids:
            continue

        pop_up_text = pop_up_text + "<br>Cell-ID: " + cell["id"] + "<br>" + "Center Freq: " + cell["centerFreq"] + " MHz<br>" + "Type: " +cell["frame_type"] + "<br>" + "Bandwidth: " + cell["bandwidth"] + " MHz<br>" +  "Ref. power: " + cell["pdsch_reference_signal_power_dbm"] + " dBm<br>"
        depth = depth + 120
        iframe = folium.IFrame(pop_up_text,width=200, height=depth)
        folium.Marker(location=[float(cell["lat"]), float(cell["lng"])],
        popup=folium.map.Popup(iframe, parse_html=False, max_width=500,sticky=True),
        icon=folium.Icon(color="blue"),
        ).add_to(m)
        plot_ids.append(cell["id"])


folium.Marker(location=[40.350288669177814, -74.65208898358767],
        #popup=folium.Popup("Probe Location", parse_html=False, max_width=500,default_open=True),
        icon=folium.Icon(color="red"),
        tooltip=folium.Tooltip("Probe Location")
        ).add_to(m)

m.fit_bounds(m.get_bounds(), padding=(1, 1))
m.save("test_mapt.html")








# plot_ids = []



# for i in range(len(scm_results)):
#     if (scm_results[i]["id"] in plot_ids):
#         continue
#     if(scm_results[i]["lat"]!= -10000):
#         pop_up_text = "Center Freq: " + scm_results[i]["centerFreq"] + " MHz<br>" + "Type: " + scm_results[i]["frame_type"] + "<br>" + "Bandwidth: " + scm_results[i]["bandwidth"] + " MHz<br>" + "Cell-ID: " + scm_results[i]["id"] + "<br>" + "Ref. power: " + scm_results[i]["pdsch_reference_signal_power_dbm"] + " dBm"
        
#         iframe = folium.IFrame(pop_up_text,width=200, height=120)
#         folium.Marker(location=[float(scm_results[i]["lat"]), float(scm_results[i]["lng"])],
#         popup=folium.Popup(iframe, parse_html=False, max_width=500),
#         icon=folium.Icon(color="green", icon="ok-sign"),
#         ).add_to(marker_cluster)

#         plot_ids.append(scm_results[i]["id"] )

# m.fit_bounds(m.get_bounds(), padding=(1, 1))
# m.save("test_mapt.html")