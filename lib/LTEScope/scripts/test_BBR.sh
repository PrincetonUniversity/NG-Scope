#!/bin/bash

# For local start
cmd="ssh alex@192.168.1.20 \"~/pantheon/startCCATest.sh bbr\" "
#cmd="~/pantheon/startCCATest.sh bbr"
eval "$cmd"
#
#pid_Pantheon=$!
#
#a=0
## check whether the Pantheon program is finished or not
#while (ps | grep "$pid_Pantheon" >/dev/null)
#do
#    sleep 1
#    a=$[$a+1]
#    echo $a
#done
#
#echo "done!"
