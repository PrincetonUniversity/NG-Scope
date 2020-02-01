#!/bin/bash
# PC IP of where to store the traces
#trPC="yaxiongxie@192.168.1.200"       # PC IP
trPC="alex@192.168.1.52"       # PC IP
servPCName="paws"
#servIP="18.220.169.58"
servIP=$2
#trFolder="~/FanYi/re_PAWS/CCA/static_data"  # Folder to store
trFolder="~/FanYi/re_PAWS/CCA/fixrate_interrupt"
#trFolder="~/FanYi/re_PAWS/CCA/mobility"
#trFolder="~/Research/LTE_CCA/dataFolder"  # Folder to store

cmd="ssh $trPC \"mkdir $trFolder/TR_$1\""
eval "$cmd"

cmd="ssh $trPC \"mkdir $trFolder/TR_$1/lteCCA_trace\""
eval "$cmd"

cmd="scp ubuntu@$servIP:~/cca_server/server_ack_log ./data/"
eval "$cmd"

cmd="scp ubuntu@$servIP:~/cca_server/server_tick_log ./data/"
eval "$cmd"

cmd="scp $servPCName@192.168.2.20:~/LTEScope/build/lib/LTEScope/client_log ./data"
eval "$cmd"

cmd="scp $servPCName@192.168.2.20:~/LTEScope/build/lib/LTEScope/CCA_rate_log ./data"
eval "$cmd"

cmd="scp ./data/server_ack_log $trPC:$trFolder/TR_$1/lteCCA_trace/server_ack_log_3usrp"
eval "$cmd"

cmd="scp ./data/server_tick_log $trPC:$trFolder/TR_$1/lteCCA_trace/server_tick_log_3usrp"
eval "$cmd"

cmd="scp ./data/client_log $trPC:$trFolder/TR_$1/lteCCA_trace/client_log_3usrp"
eval "$cmd"

cmd="scp ./data/CCA_rate_log $trPC:$trFolder/TR_$1/lteCCA_trace/CCA_rate_log_3usrp"
eval "$cmd"

cmd="scp ./dci_log $trPC:$trFolder/TR_$1/lteCCA_trace/dci_log_3usrp"
eval "$cmd"

cmd="scp ./dci_dl_usrp_0_dci_1.txt $trPC:$trFolder/TR_$1/lteCCA_trace/dci_dl_usrp_0_dci_1_3usrp.txt"
eval "$cmd"

cmd="scp ./dci_dl_usrp_0_dci_2.txt $trPC:$trFolder/TR_$1/lteCCA_trace/dci_dl_usrp_0_dci_2_3usrp.txt"
eval "$cmd"

cmd="scp ./dci_dl_usrp_1_dci_1.txt $trPC:$trFolder/TR_$1/lteCCA_trace/dci_dl_usrp_1_dci_1_3usrp.txt"
eval "$cmd"

cmd="scp ./dci_dl_usrp_1_dci_2.txt $trPC:$trFolder/TR_$1/lteCCA_trace/dci_dl_usrp_1_dci_2_3usrp.txt"
eval "$cmd"

cmd="scp ./dci_dl_usrp_2_dci_1.txt $trPC:$trFolder/TR_$1/lteCCA_trace/dci_dl_usrp_2_dci_1_3usrp.txt"
eval "$cmd"

cmd="scp ./dci_dl_usrp_2_dci_2.txt $trPC:$trFolder/TR_$1/lteCCA_trace/dci_dl_usrp_2_dci_2_3usrp.txt"
eval "$cmd"
