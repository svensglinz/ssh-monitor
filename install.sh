#!/bin/bash

set -e

# USER CONFIGURABLE
BIN_PATH="/usr/local/bin"
DB_PATH="/var/lib/ssh-monitor"
LOG_PATH="/var/log/ssh-monitor"
APP_NAME="ssh-monitor"

# DO NOT CHANGE
SCRIPT_PATH="scripts/ssh-monitor.sh"
UNINSTALL_PATH="uninstall.sh"

# script must be run as root
if [ "$(id -u)" -ne 0 ]; then
  echo "Please run script as root"
  exit 1
else
  USER=$SUDO_USER
fi

# check if a specific utility is installed
check_and_install() {
  echo -n "checking if $1 is installed... "
  (command -v "$1" > /dev/null 2>&1 && echo "installed") || {
    echo "not installed. Installing..."
    apt update
    apt install -y "$1"
  }
}

rollback() {
  echo "Rolling back changes..."

  # remove log-folder if created
  if [ -d "$LOG_PATH" ]; then
    rm -rf "$LOG_PATH"
  fi

  # remove db-folder if created
  if [ -d "$DB_PATH" ]; then
    rm -rf "$DB_PATH"
  fi

  # remove script if created
  if [ -f "$SCRIPT_PATH.tmp" ]; then
    rm -f "$SCRIPT_PATH.tmp"
  fi

  # clean install
  make clean
}

trap rollback ERR

# check dependencies
check_and_install "sqlite3"
check_and_install "ipset"
check_and_install "iptables"
check_and_install "less"
check_and_install "make"

# installing application
echo "installing $APP_NAME"
make

# create directories for logs and database
if [ ! -d "$DB_PATH" ]; then
    echo "saving the database in $DB_PATH"
    mkdir -p "$DB_PATH" || { echo "Failed to create $LOG_PATH"; exit 1; }
fi

if [ ! -d "$LOG_PATH" ]; then
  echo "Saving logs in $LOG_PATH/$APP_NAME"
  mkdir -p "$LOG_PATH/$APP_NAME" || { echo "Failed to create $LOG_PATH"; exit 1; }
fi

# replace placeholders in user script and finalize install
sed "s|{{DB_PATH}}|$DB_PATH|g; s|{{LOG_PATH}}|$LOG_PATH|g; s|{{EXEC_PATH}}|$BIN_PATH/$APP_NAME-bin|g;" "$SCRIPT_PATH" > "$SCRIPT_PATH.tmp"
install -m 775 "bin/ssh-monitor" "$BIN_PATH/$APP_NAME-bin" # not to be executed by user
install -m 775 "$SCRIPT_PATH.tmp" "$BIN_PATH/$APP_NAME"
rm "$SCRIPT_PATH.tmp"

# finish uninstall script
sed "s|{{DB_PATH}}|$DB_PATH|g; s|{{LOG_PATH}}|$LOG_PATH|g; s|{{APP_NAME}}|$APP_NAME|g; s|{{BIN_PATH}}|$BIN_PATH|g;" "$UNINSTALL_PATH" > "$UNINSTALL_PATH.tmp"
mv "$UNINSTALL_PATH.tmp" "$UNINSTALL_PATH"
chmod +x "$UNINSTALL_PATH"

echo "Installation complete!"
echo "Run uninstall.sh to uninstall the application"
