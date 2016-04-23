#!/usr/bin/env bash
# See LICENSE (GPL V2)
if test $# = 1
then
  SOURCE=$1
else
  echo Usage: $0 source-filename
  exit 1
fi

name=$(basename $SOURCE)
myname=build/$name/
mkdir -p $myname/mutants

DESCS=$myname/mutants.txt
total_mutants=$(wc -l < $DESCS)
echo Number of mutant descriptions generated:  $total_mutants

echo Generating mutants of ${SOURCE}...

for mutated_line in $(seq 1 $total_mutants)
do
  mymutant=build/$name/mutants/${mutated_line}_${name}

  mut_line_no=$(sed -ne "$mutated_line s/:.*//p" < $DESCS)
  before_mutant=$(($mut_line_no - 1))
  after_mutant=$(($mut_line_no + 1))

  echo Generating mutant source file $mymutant with mutated line $mut_line_no
  sed -ne "1,$before_mutant p" < $SOURCE >$mymutant
  sed -ne "$mutated_line s/^[^:]*://p" < $DESCS >>$mymutant
  sed -ne "$after_mutant,$ p" < $SOURCE >>$mymutant
done

echo Number of mutants generated in $PWD: $mutated_line

exit 0
