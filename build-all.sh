#!/bin/env bash

rm -rf release
mkdir release
TEMP=$PATH

# Miyoo
source path-miyoo
make -f Makefile.bittboy zip
mv yatka.zip release/yatka-bittboy.zip
make -f Makefile.bittboy clean
export PATH=$TEMP

# RetroFW + OpenDingux (dual opk)
source path-retrofw
make -f Makefile.retrofw yatka-retrofw
mv yatka-retrofw yatka
make -f Makefile.retrofw clean
mv yatka yatka-retrofw
export PATH=$TEMP

source path-gcw0
make -f Makefile.gcw0 yatka-gcw0
mv yatka-gcw0 yatka
make -f Makefile.gcw0 clean
mv yatka yatka-gcw0
export PATH=$TEMP

mksquashfs skins sfx music icon.png README.md LICENSE.md yatka.gcw0.desktop yatka-gcw0 yatka.retrofw.desktop yatka-retrofw yatka-dual.opk -noappend -no-xattrs
mv yatka-dual.opk release/
rm -f yatka-retrofw yatka-gcw0
