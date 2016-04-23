descmutants: src/descmutants.pl
	mkdir -p bin
	swipl --goal=describe_mutants --stand_alone=true --local=32768 -o ./bin/descmutants -c src/descmutants.pl

clean:
	rm descmutants
#	pl --goal=describe_mutants --stand_alone=true --local=32768 -o descmutants -c descmutants.pl
