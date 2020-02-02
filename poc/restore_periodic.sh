#!/bin/sh

BACKUP_NAME='exploit_periodic.backup'

if [ ! -f "./$BACKUP_NAME" ]; then
  echo "[ERROR] backup file $BACKUP_NAME does not exist"
  exit 1
fi

echo "[INFO] restoring 110.clean-tmps"
./portal write /etc/periodic/daily/110.clean-tmps ./$BACKUP_NAME
echo "[INFO] 110.clean-tmps restored"

rm -f ./$BACKUP_NAME
