#!/bin/bash
# PC IP of where to store the traces
#trPC="yaxiongxie@192.168.1.200"       # PC IP
trPC="alex@192.168.1.52"       # PC IP
servIP="18.220.169.58"
#trFolder="~/FanYi/re_PAWS/CCA/static_data"  # Folder to store
trFolder="~/FanYi/re_PAWS/CCA/fixrate_interrupt"
#trFolder="~/Research/LTE_CCA/dataFolder"  # Folder to store

CCA_array=("pcc" "sprout" "copa" "vivace" "verus" "cubic" "bbr" "cca")
schemes=${CCA_array[$2]}

# --Tr_1/dciTrace the folder stores the LTE PHY layer dci traces
cmd="ssh $trPC \"mkdir $trFolder/TR_$1/dciTrace/$schemes\""
eval "$cmd"

cmd="scp ./dci_dl_usrp_0_dci_1.txt $trPC:$trFolder/TR_$1/dciTrace/$schemes/"
eval "$cmd"
cmd="scp ./dci_dl_usrp_0_dci_2.txt $trPC:$trFolder/TR_$1/dciTrace/$schemes/"
eval "$cmd"
cmd="scp ./dci_dl_usrp_1_dci_1.txt $trPC:$trFolder/TR_$1/dciTrace/$schemes/"
eval "$cmd"

cmd="scp ./dci_dl_usrp_1_dci_2.txt $trPC:$trFolder/TR_$1/dciTrace/$schemes/"
eval "$cmd"

cmd="scp ./dci_dl_usrp_2_dci_1.txt $trPC:$trFolder/TR_$1/dciTrace/$schemes/"
eval "$cmd"

cmd="scp ./dci_dl_usrp_2_dci_2.txt $trPC:$trFolder/TR_$1/dciTrace/$schemes/"
eval "$cmd"

