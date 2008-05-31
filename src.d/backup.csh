#!/bin/csh -x

set BACKUP_DIR = /love/scsi/backup/backup.d/ez-ntp.d

if (! -d $BACKUP_DIR ) then
    mkdir $BACKUP_DIR
endif

cp -p *.c $BACKUP_DIR/.
cp -p *.h $BACKUP_DIR/.
cp -p *.sh $BACKUP_DIR/.
cp -p *.csh $BACKUP_DIR/.
cp -p Makefile* $BACKUP_DIR/.
cp -p INSTALL USAGE WARNINGS $BACKUP_DIR/.
