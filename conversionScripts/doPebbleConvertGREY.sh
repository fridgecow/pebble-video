#!/bin/bash
convert $1 \
  -adaptive-resize '144x168>' \
  -fill '#FFFFFF00' -opaque none \
  -type Grayscale -colorspace Gray \
  -black-threshold 30% -white-threshold 70% \
  -ordered-dither 2x1 \
  -colors 2 -depth 1 \
  -define png:compression-level=9 -define png:compression-strategy=0 \
  -define png:exclude-chunk=all \
  pbl/$1
