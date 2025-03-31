#!/bin/bash

BIN_PATH="{{BIN_PATH}}"
DB_PATH="{{DB_PATH}}"
LOG_PATH="{{LOG_PATH}}"
APP_NAME="{{APP_NAME}}"

if [ "$(id -u)" -ne 0 ]; then
  echo "please run script as root"
  exit 1
fi

echo "uninstalling $APP_NAME"
echo "*******************************"
echo ""

# remove binaries
if [ -f "$BIN_PATH/$APP_NAME" ]; then
  echo "removing $BIN_PATH/$APP_NAME"
  rm -f "$BIN_PATH/$APP_NAME"
fi

if [ -f "$BIN_PATH/$APP_NAME-bin" ]; then
  echo "removing $BIN_PATH/$APP_NAME-bin"
    rm -f "$BIN_PATH/$APP_NAME-bin"
fi

# removing log dir ?
if [ -d "$LOG_PATH" ]; then
  echo "Would you like to remove the log files ($LOG_PATH)? (Y/N)"
  read -r  REMOVE_LOG_DIR
  if [[ $REMOVE_LOG_DIR =~ ^[Yy]$ ]]; then
    echo "removing logs"
    rm -rf "$LOG_PATH"
  fi
fi

# removing log dir ?
if [ -d "$DB_PATH" ]; then
  echo "would you like to remove the database ($DB_PATH) ? (Y/N)"
  read -r REMOVE_DB_DIR
  if [[ $REMOVE_DB_DIR =~ ^[Yy]$ ]]; then
    echo "removing database"
    rm -rf "$DB_PATH"
  fi
fi

echo ""
echo "$APP_NAME successfully uninstalled"
echo "*******************************"
