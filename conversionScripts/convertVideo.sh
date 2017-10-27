#!/bin/bash
ffmpeg -i $1 -r $2 $filename%03d.png
mkdir pbl
find . -maxdepth 1 -name "*.png" -exec "./doPebbleConvert.sh" "{}" ";"
rm ./*.png
