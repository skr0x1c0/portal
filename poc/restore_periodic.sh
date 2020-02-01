#!/bin/sh

#  restore_periodic.sh
#  pwndav
#
#  Created by Chakra on 02/02/20.
#  Copyright © 2020 Sreejith Krishnan R. All rights reserved.

BACKUP_NAME='exploit_periodic.backup'

if [ ! -f "./$BACKUP_NAME" ]; then
  echo "[INFO] backup file $BACKUP_NAME does not exist"
  exit 1
fi

echo "[INFO] restoring zprofile"
./portal write /etc/periodic/daily/110.clean-tmps ./$BACKUP_NAME
echo "[INFO] zprofile restored"

rm -f ./$BACKUP_NAME
