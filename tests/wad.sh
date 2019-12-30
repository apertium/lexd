#!/bin/bash

if [ $# -gt 0 ]
then
  N=$1
else
  N="1"
fi

echo ".twoc -> .twoc.hfst"
time for x in $(seq $N); do hfst-twolc -s wad.twoc -o wad.twoc.hfst; done

echo ".lexc -> .lexc.hfst"
time for x in $(seq $N); do hfst-lexc -s wad.lexc -o wad.lexc.hfst; done

echo ".lexc + .twoc"
time for x in $(seq $N); do hfst-invert wad.lexc.hfst | hfst-compose-intersect -s -1 - -2 wad.twoc.hfst | hfst-invert | hfst-minimize -o wad.hfst; done

echo ".lexd"
time for x in $(seq $N); do ../src/lexd wad.lexd wad.att; done

echo "convert"
time for x in $(seq $N); do hfst-txt2fst wad.att -o wad_d.hfst; done

hfst-expand wad.hfst | sort > wad.txt
hfst-expand wad_d.hfst | sort > wad_d.txt
diff wad.txt wad_d.txt

rm wad.twoc.hfst wad.lexc.hfst wad.hfst wad.att wad_d.hfst wad.txt wad_d.txt
