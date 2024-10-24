#!/bin/bash

ATTEMPTS="sqlite3 /mnt/data/programs/ssh-monitor/ip_logins.db '.mode box' '.headers on' 'SELECT * FROM ip_logins >
BLOCKED="sudo ipset -L ssh_blocklist | grep -A 1000 'Members'"
START="nohup sudo /mnt/data/programs/ssh-monitor/monitor >> /mnt/data/programs/ssh-monitor/log.txt &"
LOG="less /mnt/data/programs/ssh-monitor/log.txt"
usage() {
        echo "Usage: $0 {attempts|blocked}"
        exit 1
}

if [ $# -ne 1 ]; then
        usage
fi 

if [ "$1" = "attempts" ]; then
        eval "$ATTEMPTS"
elif [ "$1" = "blocked" ]; then
        eval "$BLOCKED"
elif [ "$1" = "start" ]; then
        eval "$START"
elif [ "$1" = "log" ]; then
        eval "$LOG"
else
        usage
fi
