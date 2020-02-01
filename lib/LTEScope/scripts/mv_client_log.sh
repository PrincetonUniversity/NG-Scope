#!/bin/bash
# PC IP of where to store the traces
cmd="cp ./data/client-ack-log ./data/client_ack_log_conLen_${1}_pktInt_${2}_tr_${3}"
eval "$cmd"
