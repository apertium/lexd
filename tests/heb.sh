#!/bin/bash

if [ $# -gt 0 ]
then
  N=$1
else
  N="1"
fi

echo ".twoc"
time for x in $(seq $N); do hfst-twolc -s heb.twoc -o heb.twoc.hfst; done

echo ".twoc vowels"
time for x in $(seq $N); do hfst-twolc -s heb_vow.twoc -o heb_vow.twoc.hfst; done

echo ".lexc"
time for x in $(seq $N); do hfst-lexc -s heb.lexc -o heb.lexc.hfst; done

echo ".lexc + .twoc"
time for x in $(seq $N); do hfst-invert heb.lexc.hfst | hfst-compose-intersect -1 - -2 heb.twoc.hfst | hfst-invert -o heb.lexc-twoc.hfst; done

echo ".hfst + vowels"
time for x in $(seq $N); do hfst-compose-intersect -1 heb.lexc-twoc.hfst -2 heb_vow.twoc.hfst | hfst-minimize -o heb.hfst; done

echo ".lexd"
time for x in $(seq $N); do ../src/lexd heb.lexd heb.att; done

echo "convert"
time for x in $(seq $N); do hfst-txt2fst heb.att -o heb_d.hfst; done

hfst-expand heb.hfst | sort > heb.txt
hfst-expand heb_d.hfst | sort > heb_d.txt

diff heb.txt heb_d.txt

rm heb.twoc.hfst heb_vow.twoc.hfst heb.lexc.hfst heb.lexc-twoc.hfst heb.hfst heb.att heb_d.hfst heb.txt heb_d.txt
