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
Usage: ssh-monitor [options1] | [options2]

options1:
  attempts   Show the last 6 login attempts from the database.
  blocked    List all currently blocked IPs from the IP set.
  log        View the ssh-monitor log file.
  -h         Show this help menu.

options2:
  -n <number>  Number of max. attempts before IP is blocked.
  -t <seconds> Time window (in seconds) within which max. attempts are registered.

Note: options1 and options2 are mutually exclusive.

EOF
}

# Handle command options
case "$1" in
  attempts)
    eval "$ATTEMPTS"
    exit 0
    ;;
  blocked)
    eval "$BLOCKED"
    exit 0
    ;;
  log)
    eval "$LOG"
    exit 0
    ;;
  *)
    ;;
esac

val_n=3
val_t=3600

while getopts "t:n:h" opt; do
    case $opt in
      h)
        usage
        exit 0
      ;;
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
