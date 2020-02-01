#!/bin/bash
# PC IP of where to store the traces
trPC="alex@192.168.1.52"       # PC IP
#trFolder="~/FanYi/re_PAWS/CCA/static_data"  # Folder to store
trFolder="~/FanYi/re_PAWS/CCA/fixrate_interrupt"
#trFolder="~/FanYi/re_PAWS/CCA/mobility"
panPC="alex@192.168.1.20"   # pantheon PC
panDataFolder="~/pantheon/src/experiments/data"
time=20
CCA_array=("pcc" "sprout" "copa" "vivace" "verus" "cubic" "bbr" )

schemes=${CCA_array[$2]}

#analyze the trace into pdf
cmd="ssh $panPC \"~/pantheon/analyz_trace.sh $schemes\""
eval "$cmd"

# Create folder in the destinate PC that stores our traces
# --Tr_1 the home folder to store the trace
cmd="ssh $trPC \"mkdir $trFolder/TR_$1\""
eval "$cmd"
# --Tr_1/PantheonReport the pantheon report folder
cmd="ssh $trPC \"mkdir $trFolder/TR_$1/PantheonReport\""
eval "$cmd"
# --Tr_1/PantheonTrace the pantheon trace folder
cmd="ssh $trPC \"mkdir $trFolder/TR_$1/PantheonTrace\""
eval "$cmd"
# --Tr_1/dciTrace the folder stores the LTE PHY layer dci traces
cmd="ssh $trPC \"mkdir $trFolder/TR_$1/dciTrace\""
eval "$cmd"

# copy the pantheon logs
cmd="ssh $panPC \"scp $panDataFolder/${schemes}_acklink_run1.log $trPC:$trFolder/TR_$1/PantheonTrace/\""
eval "$cmd"
cmd="ssh $panPC \"scp $panDataFolder/${schemes}_datalink_run1.log $trPC:$trFolder/TR_$1/PantheonTrace/\""
eval "$cmd"
cmd="ssh $panPC \"scp $panDataFolder/${schemes}_stats_run1.log $trPC:$trFolder/TR_$1/PantheonTrace/\""
eval "$cmd"
cmd="ssh $panPC \"scp $panDataFolder/${schemes}_inNet_log.txt $trPC:$trFolder/TR_$1/PantheonTrace/\""
eval "$cmd"
cmd="ssh $panPC \"scp $panDataFolder/${schemes}_outNet_log.txt $trPC:$trFolder/TR_$1/PantheonTrace/\""
eval "$cmd"

# copy the pantheon report in pdf
cmd="ssh $panPC \"scp $panDataFolder/pantheon_summary.pdf $trPC:$trFolder/TR_$1/PantheonReport/pantheon_summary_$schemes.pdf\""
eval "$cmd"
cmd="ssh $panPC \"scp $panDataFolder/pantheon_report.pdf $trPC:$trFolder/TR_$1/PantheonReport/pantheon_report_$schemes.pdf\""
eval "$cmd"
cmd="ssh $panPC \"scp $panDataFolder/pantheon_summary_mean.pdf $trPC:$trFolder/TR_$1/PantheonReport/pantheon_summary_mean_$schemes.pdf\""
eval "$cmd"

# copy the LTE dci traces
# cmd="scp ~/LTEScope/LTEScope/build/lib/LTEScope/dci_log $trPC:$trFolder/TR_$1/dciTrace/dci_log_$1.log"
# eval "$cmd"

