#!/bin/bash

# For local start
servIP="18.220.169.58"
cmd="ssh alex@192.168.1.20 \"~/pantheon/src/experiments/test.py remote --sender remote --schemes bbr -t 20 ubuntu@$servIP:~/pantheon\"" 
eval "$cmd"
