#!/bin/bash

# path variables
export DB_PATH="{{DB_PATH}}"
export LOG_PATH="{{LOG_PATH}}"

# command options
ATTEMPTS="sqlite3 $DB_PATH/ip_logins.db '.mode box' '.headers on' 'SELECT * FROM logins ORDER BY last_attempt DESC LIMIT 6;'"
BLOCKED="sudo ipset -L ssh_blocklist | grep -A 1000 'Members'"
LOG="less $LOG_PATH/ssh-monitor.log"
START="{{EXEC_PATH}}"

# Display usage instructions
usage() {
  cat <<EOF
Usage: ssh-monitor [option]

Options:
  attempts   Show the last 6 login attempts from the database.
  blocked    List all currently blocked IPs from the IP set.
  log        View the ssh-monitor log file.
  start      Start the ssh-monitor service.
  stop       Stop the ssh-monitor service.
EOF
}

if [ $# -gt 1 ]; then
    usage
    exit 1
fi

if [ $# -eq 0 ]; then
    if [ "$(id -u)" -ne 0 ]; then
      echo "start application as root"
      exit 1
    fi
    eval "$START"
    exit 0
fi

# Handle command options
case "$1" in
  attempts)
    eval "$ATTEMPTS"
    ;;
  blocked)
    eval "$BLOCKED"
    ;;
  log)
    eval "$LOG"
    ;;
  *)
    echo "Error: Invalid option '$1'"
    usage
    exit 1
    ;;
esac