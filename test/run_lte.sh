#!/bin/bash

#
# Copyright 2013-2021 Software Radio Systems Limited
#
# This file is part of srsRAN
#
# srsRAN is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# srsRAN is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Affero General Public License for more details.
#
# A copy of the GNU Affero General Public License can be found in
# the LICENSE file in the top-level directory of this distribution
# and at http://www.gnu.org/licenses/.
#

epc_pid=0
enb_pid=0
ue_pid=0

print_use(){
  echo "Please call script with srsRAN build path as first argument and number of PRBs as second (number of component carrier is optional)"
  echo "E.g. ./run_lte.sh [build_path] [nof_prb] [num_cc]"
  exit -1
}

kill_lte(){
  echo "Shutting down LTE network"
  # Allow to gracefully exit
  kill -SIGTERM $ue_pid
  sleep 4
  kill -SIGTERM $enb_pid
  sleep 4
  kill -SIGTERM $epc_pid
  sleep 4

  # Force kill if they are still running
  if ps -p $ue_pid > /dev/null
  then
    echo "Killing UE"
    kill -9 $ue_pid
  fi
  if ps -p $enb_pid > /dev/null
  then
    echo "Killing eNB"
    kill -9 $enb_pid
  fi
  if ps -p $epc_pid > /dev/null
  then
    echo "Killing EPC"
    kill -9 $epc_pid
  fi

  if [ -f ./srsRAN.backtrace.crash ]; then
    echo "Rename backtrace"
    mv ./srsRAN.backtrace.crash srsRAN.backtrace.log
  fi

  # Delete netns
  ip netns delete $ue_netns

  # Don't exit with return code ..
}

valid_num_prb()
{
    case "$1" in
    "6"|"15"|"25"|"50"|"75"|"100")
        return 0;;
    *)
        echo "Invalid number of PRBs. Possible values are [6, 15, 25, 50, 75, 100]."
        return 1;;
    esac
}

check_ping()
{
  loss_rate=$(cat ./${nof_prb}prb_screenlog_ping_ul.log | grep -oP '\d+(?=% packet loss)')
  if [ "$loss_rate" != "0" ] 2>/dev/null; then
    echo "Error. Detected $loss_rate per-cent loss rate in UL ping."
    exit 1
  fi

  loss_rate=$(cat ./${nof_prb}prb_screenlog_ping_dl.log | grep -oP '\d+(?=% packet loss)')
  if [ "$loss_rate" != "0" ] 2>/dev/null; then
    echo "Error. Detected $loss_rate per-cent loss rate in DL ping."
    exit 1
  fi
}

check_ue()
{
  # check that app exited correctly
  last_line=$(cat ./${nof_prb}prb_screenlog_srsue.log | tail -n1)
  if [[ "$last_line" != *"---  exiting  ---"* ]]; then
    echo "Error. srsUE didn't seem to have exited properly."
    exit 1
  fi
  
  # Check UE logs don't contain Error from any layer
  num_error=$(cat ./${nof_prb}prb_ue.log | grep "\[E\]" | wc -l)
  if [ "$num_error" != "0" ] 2>/dev/null; then
    echo "Error. Detected $num_error error messages in UE logs. We should finish without any error."
    exit 1
  fi

  # Check UE logs don't contain Warning from any layer
  num_warning=$(cat ./${nof_prb}prb_ue.log | grep "\[W\]" | wc -l)
  if [ "$num_warning" != "0" ] 2>/dev/null; then
    echo "Error. Detected $num_warning warning messages in UE logs. We should finish without any."
    exit 1
  fi

  # Check PDSCH
  num_error=$(cat ./${nof_prb}prb_ue.log | grep "KO" | wc -l)
  if [ "$num_error" != "0" ] 2>/dev/null; then
    echo "Error. Detected $num_error PDSCH errors in UE logs. We should finish without any error."
    exit 1
  fi

  # Check PUCCH ack
  num_error=$(cat ./${nof_prb}prb_ue.log | grep "PUCCH" | grep "ack=0" | wc -l)
  if [ "$num_error" != "0" ] 2>/dev/null; then
    echo "Error. Detected $num_error PUCCH errors in UE logs. We should finish without any error."
    exit 1
  fi

  # Check PHICH
  num_error=$(cat ./${nof_prb}prb_ue.log | grep "hi=0" | wc -l)
  if [ "$num_error" != "0" ] 2>/dev/null; then
    echo "Error. Detected $num_error PHICH errors in UE logs. We should finish without any error."
    exit 1
  fi

  # Check CQI is 15
  num_error=$(cat ./${nof_prb}prb_ue.log | grep "PUCCH" | grep "cqi=" | grep -v "cqi=15" | wc -l)
  if [ "$num_error" != "0" ] 2>/dev/null; then
    echo "Error. Detected $num_error CQI != 15 in UE logs. We should finish with all cqi=15."
    exit 1
  fi

  # Check number of PRACHs
  num_prach=$(cat ./${nof_prb}prb_ue.log | grep "preamble=" | wc -l)
  if [ "$num_prach" != "1" ] 2>/dev/null; then
    echo "Error. Detected $num_prach PRACH(s). But should be only 1."
    exit 1
  fi

  # Check duplicate TB at MAC
  num_error=$(cat ./${nof_prb}prb_ue.log | grep "duplicate" | wc -l)
  if [ "$num_error" != "0" ] 2>/dev/null; then
    echo "Error. Detected $num_error duplicate TBs in UE logs. We should finish without any duplicate."
    exit 1
  fi
}

check_enb()
{
  # check that app exited correctly
  last_line=$(cat ./${nof_prb}prb_screenlog_srsenb.log | tail -n1)
  if [[ "$last_line" != *"---  exiting  ---"* ]]; then
    echo "Error. srsENB didn't seem to have exited properly."
    exit 1
  fi

  # Check PRACH results
  num_prach=$(cat ./${nof_prb}prb_screenlog_srsenb.log | grep RACH: | wc -l)
  if [ "$num_prach" != "1" ] 2>/dev/null; then
    echo "Error. Detected $num_prach PRACH(s). But should be only 1."
    exit 1
  fi

  # check [E]
  num_error=$(cat ./${nof_prb}prb_enb.log | grep "\[E\]" | wc -l)
  if [ "$num_error" != "0" ] 2>/dev/null; then
    echo "Error. Detected $num_error error messages in eNB logs. We should finish without any error."
    exit 1
  fi

  # Check [W]
  num_warning=$(cat ./${nof_prb}prb_enb.log | grep "\[W\]" | wc -l)
  if [ "$num_warning" != "0" ] 2>/dev/null; then
    echo "Error. Detected $num_warning warning messages in eNB logs. We should finish without any."
    exit 1
  fi

  # Check for PUSCH errors
  num_error=$(cat ./${nof_prb}prb_enb.log | grep "KO" | wc -l)
  if [ "$num_error" != "0" ] 2>/dev/null; then
    echo "Error. Detected $num_error PUSCH errors in eNB logs. We should finish without any error."
    exit 1
  fi

  # Check PUCCH ack
  num_error=$(cat ./${nof_prb}prb_enb.log | grep "PUCCH" | grep "ack=0" | wc -l)
  if [ "$num_error" != "0" ] 2>/dev/null; then
    echo "Error. Detected $num_error PUCCH errors in eNB logs. We should finish without any error."
    exit 1
  fi

  # Check CQI is 15
  num_error=$(cat ./${nof_prb}prb_enb.log | grep "PUCCH" | grep "cqi=" | grep -v "cqi=15" | wc -l)
  if [ "$num_error" != "0" ] 2>/dev/null; then
    echo "Error. Detected $num_error CQI != 15 in eNB logs. We should finish with all qci=15."
    exit 1
  fi
}

check_epc()
{
  # check that app exited correctly
  last_line=$(cat ./${nof_prb}prb_screenlog_srsepc.log | tail -n1)
  if [[ "$last_line" != *"---  exiting  ---"* ]]; then
    echo "Error. srsEPC didn't seem to have exited properly."
    exit 1
  fi

  # check [E]
  num_error=$(cat ./${nof_prb}prb_epc.log | grep "\[E\]" | wc -l)
  if [ "$num_error" != "0" ] 2>/dev/null; then
    echo "Error. Detected $num_error error messages in EPC logs. We should finish without any error."
    exit 1
  fi
}

# check if build path has been passed
if ([ ! $1 ])
then
  print_use
  exit
fi
build_path="$1"

# check number of PRBs
if ([ ! $2 ])
then
  print_use
  exit
fi
nof_prb="$2"

# check number of CC
num_cc="1"
if ([ $3 ])
then
  num_cc="$3"
fi
echo "Using $num_cc component carrier(s) in srsENB"

base_srate="23.04e6"
if ([ "$nof_prb" == "75" ])
then
  base_srate="15.36e6"
fi

if ([ "$nof_prb" == "15" ])
then
  base_srate="3.84e6"
fi

# Check for LTE binaries in build path
if [ ! -x "$build_path/srsenb/src/srsenb" ]; then
  echo "Error! srsENB binary not found or not executable!"
  exit 1
fi

if [ ! -x "$build_path/srsepc/src/srsepc" ]; then
  echo "Error! srsEPC binary not found or not executable!"
  exit 1
fi

if [ ! -x "$build_path/srsue/src/srsue" ]; then
  echo "Error! srsUE binary not found or not executable!"
  exit 1
fi

# Check PRB parameter
if ! valid_num_prb "$nof_prb"; then
  exit 1
fi

# Adding netns
ue_netns="ue1"
ip netns add $ue_netns
if [ $? -ne 0 ]
then
  echo "Couldn't add netns. Missing permissions? Network namespace $ue_netns already exists?"
  exit 1
fi

epc_args="$build_path/../srsepc/epc.conf.example \
          --hss.db_file=$build_path/../srsepc/user_db.csv.example \
          --log.filename=./${nof_prb}prb_epc.log"
enb_args="$build_path/../srsenb/enb.conf.example \
          --enb_files.sib_config=$build_path/../srsenb/sib.conf.example \
          --enb_files.rb_config=$build_path/../srsenb/rb.conf.example \
          --rf.device_name=zmq \
          --log.all_level=info \
          --enb.n_prb=$nof_prb \
          --log.filename=./${nof_prb}prb_enb.log"

ue_args="$build_path/../srsue/ue.conf.example \
         --rf.device_name=zmq \
         --rf.rx_gain 60 \
         --stack.have_tti_time_stats=false \
         --gw.netns=$ue_netns \
         --log.all_level=info \
         --log.filename=./${nof_prb}prb_ue.log \
         --pcap.enable=mac \
         --pcap.mac_filename=./${nof_prb}prb_ue.pcap"

if ([ "$num_cc" == "2" ])
then
  enb_args="$enb_args --enb_files.rr_config=$build_path/../srsenb/rr_2ca.conf.example \
            --rf.device_args=\"fail_on_disconnect=true,base_srate=${base_srate},id=enb,tx_port0=tcp://*:2000,tx_port1=tcp://*:2002,rx_port0=tcp://localhost:2001,rx_port1=tcp://localhost:2003,tx_freq0=2630e6,tx_freq1=2636e6,rx_freq0=2510e6,rx_freq1=2516e6\""
  ue_args="$ue_args --rf.dl_earfcn=2850,2910 --rf.nof_carriers=2 --rrc.ue_category=7 --rrc.release=10 \
           --rf.device_args=\"tx_port0=tcp://*:2001,tx_port1=tcp://*:2003,rx_port0=tcp://localhost:2000,rx_port1=tcp://localhost:2002,id=ue,base_srate=${base_srate},tx_freq0=2510e6,tx_freq1=2516e6,rx_freq0=2630e6,rx_freq1=2636e6\""
else
  enb_args="$enb_args --enb_files.rr_config=$build_path/../srsenb/rr.conf.example \
            --rf.device_args=\"fail_on_disconnect=true,tx_port0=tcp://*:2000,rx_port0=tcp://localhost:2001,id=enb,base_srate=${base_srate}\""
  ue_args="$ue_args --rf.device_args=\"tx_port0=tcp://*:2001,rx_port0=tcp://localhost:2000,id=ue,base_srate=${base_srate}\""
fi

# Remove existing log files
log_files=$(ls -l | grep ${nof_prb}prb_)
if [ ! -z "$log_files" ]; then
  echo "Deleting existing log files for ${nof_prb} PRB."
  eval "rm -Rf ${nof_prb}prb_*.log"
fi

# Run srsEPC
echo "Starting srsEPC"
screen -S srsepc -dm -L -Logfile ./${nof_prb}prb_screenlog_srsepc.log $build_path/srsepc/src/srsepc $epc_args
sleep 2
epc_pid=$(pgrep srsepc)
if [ -z "$epc_pid" ]
then
  echo "Couldn't start EPC."
  kill_lte
  exit 1
fi
echo "srsEPC's PID is $epc_pid"

# Run srsENB
echo "Starting srsENB"
screen -S srsenb -dm -L -Logfile ./${nof_prb}prb_screenlog_srsenb.log $build_path/srsenb/src/srsenb $enb_args
sleep 2
enb_pid=$(pgrep srsenb)
if [ -z "$enb_pid" ]
then
  echo "Couldn't start ENB"
  kill_lte
  exit 1
fi
echo "srsENB's PID is $enb_pid"

# Run srsUE
echo "Starting srsUE"
screen -S srsue -dm -L -Logfile ${nof_prb}prb_screenlog_srsue.log $build_path/srsue/src/srsue $ue_args
sleep 2
ue_pid=$(pgrep srsue)
if [ -z "$ue_pid" ]
then
  echo "Couldn't start UE"
  kill_lte
  exit 1
fi
echo "srsUE's PID is $ue_pid"


# Wait until UE is connected
timeout=20
tun_dev="tun_srsue"

echo "Waiting for $tun_dev to become available for ${timeout}s .."
wait_time=0
ip=$(ip netns exec $ue_netns ip addr show $tun_dev | grep "inet\b" | awk '{print $2}' | cut -d/ -f1)
while [ -z $ip ] && [ $wait_time -ne $timeout ]
do
  # Wait 1s before next attempt
  sleep 1
  let wait_time++
  ip=$(ip netns exec $ue_netns ip addr show $tun_dev | grep "inet\b" | awk '{print $2}' | cut -d/ -f1)
done

if [ ! -z $ip ]; then
  echo "$tun_dev is up with IP $ip!"
  sleep 1
else
  echo "$tun_dev didn't get up after ${timeout}s"
  kill_lte
  exit 1
fi

# extract MME IP
baseip=`echo $ip | cut -d"." -f1-3`
mme_ip=$baseip".1"

echo "MME IP is $mme_ip"

# show UE/eNB console trace
screen -S srsue -X stuff "t$(printf \\r)"
screen -S srsenb -X stuff "t$(printf \\r)"

# Run tests now

# run UL ping test
echo "Run UL ping"
ip netns exec $ue_netns screen -dm -L -Logfile ./${nof_prb}prb_screenlog_ping_ul.log ping $mme_ip -c 3
sleep 4

# sleep 3s to get RRC connection release
echo "Run DL ping"
screen -dm -L -Logfile ./${nof_prb}prb_screenlog_ping_dl.log ping $ip -c 3
sleep 4

# run UDP DL (rate must not be more than max DL rate for 6 PRB)
echo "Run DL iperf"
screen -dm -L -Logfile ./${nof_prb}prb_screenlog_iperf_dl.log iperf -c $ip -u -t 1 -i 1 -b 1M
sleep 3

# Stop all running LTE components and remove netns
kill_lte

# check results
check_ping
check_ue
check_enb
check_epc

echo "All tests passed!"

exit 0
