#!/bin/bash

DEVICE_PATH=${1:-$ANDROID_BUILD_TOP/device/softwinner/wing-common}
PROJECT_PATH="${PWD/$ANDROID_BUILD_TOP\//}"

if [ -d $DEVICE_PATH/overlay/$PROJECT_PATH ] ; then
  PATCHES=`ls $DEVICE_PATH/overlay/$PROJECT_PATH/*.patch`
  for f in $PATCHES; do
    if git apply --check $f &> /dev/null ; then
      echo Applying `basename $f` to $PROJECT_PATH
      git am $f
    fi
  done
fi
