#!/bin/sh
# generateMutants.sh -- generate mutants of file
# Copyright (c) 2005 Jamie Andrews (andrews [[at]] csd.uwo.ca)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
#
# Changes:
#   None (first public release).
#   08/22/2011 by Chaoqiang Zhang
#   Randomly generate some mutants using user specified number.

if test $# = 1
then
  SOURCE=$1
else
  echo Usage: $0 source-filename
  exit 1
fi

name=$(basename $SOURCE)
myname=build/$name/
mkdir -p $myname
mkdir -p $myname/mutants

DESCS=$myname/MutantDescs.txt
echo Generating mutant descriptions into file ${DESCS}...
# Assumes that executable "descmutants" has been made and is accessible.
./bin/descmutants <$SOURCE >$DESCS
Total=`wc -l < $DESCS`
echo Number of mutant descriptions generated:  $Total

echo Generating mutants of ${SOURCE}...
Mutant=0
# Starting with code 100001 because that makes it much easier for
# shell scripts that are going to step through all mutants.
MutantCode=100000

for Mutant in $(seq 1 $Total)
do
  mutated_line=$(sed -ne "$Mutant s/:.*//p" < $DESCS)
  LineNoM1=`expr $mutated_line - 1`
  LineNoP1=`expr $mutated_line + 1`
  mymutant=build/$name/mutants/${Mutant}_${name}

  echo Generating mutant source file $mymutant with mutated line $mutated_line
  sed -ne "1,$LineNoM1 p" < $SOURCE >$mymutant
  sed -ne "$Mutant s/^[^:]*://p" < $DESCS >>$mymutant
  sed -ne "$LineNoP1,$ p" < $SOURCE >>$mymutant
done

echo Number of mutants generated in `pwd`: $Mutant

exit 0
