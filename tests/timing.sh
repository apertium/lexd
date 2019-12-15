#!/bin/bash
echo ".twoc -> .twoc.hfst"
time hfst-twolc test.twoc -o test.twoc.hfst

echo ".lexc -> .lexc.hfst"
time hfst-lexc test.lexc -o test.lexc.hfst

echo ".lexc + .twoc"
time hfst-invert test.lexc.hfst | hfst-compose-intersect -1 - -2 test.twoc.hfst | hfst-minimize -o test.hfst

echo ".lexd"
time ../src/lexd test.lexd test.att

echo "convert"
time hfst-txt2fst test.att | hfst-invert -o test_d.hfst
