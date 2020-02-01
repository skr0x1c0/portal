#!/bin/sh

#  restore_launchd.sh
#  pwndav
#
#  Created by Chakra on 02/02/20.
#  Copyright © 2020 Sreejith Krishnan R. All rights reserved.

TARGET=`find /Library/LaunchDaemons -name "*.plist" -depth 1 -type f -user root -perm -o+r | tail -n 1`

if [ -z "$TARGET" ]; then
  echo "[ERROR] no plist owned by root with read permission exist in /Library/LaunchDaemons"
  exit 1;
fi

echo "[INFO] selected target $TARGET"

# Target file name
TARGET_NAME=`basename -- $TARGET`

# Target backup file name
TARGET_BACKUP=$TARGET_NAME.backup

if [ ! -f "./$TARGET_BACKUP" ]; then
  echo "[INFO] backup file $TARGET_BACKUP does not exist"
  exit 1
fi

echo "[INFO] restoring $TARGET_NAME"
./portal write $TARGET ./$TARGET_BACKUP
echo "[INFO] $TARGET_NAME restored"

rm -f ./$TARGET_BACKUP
