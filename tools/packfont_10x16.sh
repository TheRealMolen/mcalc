#!/bin/sh

TOOLS_PATH=$(dirname $0)
RES_PATH=$TOOLS_PATH/../res
OUT_PATH=$TOOLS_PATH/../src/extern

python3 $TOOLS_PATH/packfont.py $RES_PATH/font-10x16.png $RES_PATH/font-10x16.layout.txt -W 10 -H 16 -o $OUT_PATH/font-10x16.c

