#!/bin/sh

BACKUP_NAME='exploit_zprofile.backup'

if [ ! -f "./$BACKUP_NAME" ]; then
  echo "[ERROR] backup file $BACKUP_NAME does not exist"
  exit 1
fi

echo "[INFO] restoring zprofile"
./portal write /etc/zprofile ./$BACKUP_NAME
echo "[INFO] zprofile restored"

rm -f ./$BACKUP_NAME
