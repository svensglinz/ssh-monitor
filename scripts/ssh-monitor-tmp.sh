#!/bin/bash

# path variables
export DB_PATH="{{DB_PATH}}"
export LOG_PATH="{{LOG_PATH}}"

# command options
ATTEMPTS="sqlite3 $DB_PATH/ip_logins.db '.mode box' '.headers on' 'SELECT * FROM logins ORDER BY last_attempt DESC LIMIT 6;'"
BLOCKED="sudo ipset -L ssh_blocklist | grep -A 1000 'Members'"
LOG="less $LOG_PATH/ssh-monitor.log"
EXEC_PATH="{{EXEC_PATH}}"

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
    ;;
esac

val_n=3
val_t=3600

while getopts "t:n:" opt; do
    case $opt in
      t) val_t="$OPTARG"
      ;;
      n) val_n="$OPTARG"
      ;;
    \?)
      exit 1
    esac
done

if [ "$(id -u)" -ne 0 ]; then
       echo "start application as root"
       exit 1
fi

eval "$EXEC_PATH" -n "$val_n" -t "$val_t"

exit 0
