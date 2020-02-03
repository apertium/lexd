#!/bin/bash

if [ $# -gt 1 ]
then
  N=$1
  F=$2
else
  N="1"
  F=$1
fi

cat "$F.sh2" | while read line
do
  echo $line
  /usr/bin/time -f "  time: %e seconds, maximum memory usage: %M KB" bash -c "for x in {1..$N}; do $line; done"
done

hfst-expand "$F.hfst" | sort > "$F.txt"
hfst-expand "${F}_d.hfst" | sort > "${F}_d.txt"

S=`diff "$F.txt" "${F}_d.txt"`

cat "$F.txt" | wc

rm *.hfst *.att

if [ -n "$S" ]
then
  diff "$F.txt" "${F}_d.txt"
  rm "$F.txt" "${F}_d.txt"
  exit 1
fi
rm "$F.txt" "${F}_d.txt"
