P=programs/wc.c
gen: bin/descmutants
	./bin/gen.sh $(P)

bin/descmutants: src/descmutants.pl
	mkdir -p bin
	swipl --goal=describe_mutants --stand_alone=true --local=32768 -o ./bin/descmutants -c src/descmutants.pl

clobber:
	rm -f bin/descmutants
	rm -rf build

clean:
	rm -rf build
#	pl --goal=describe_mutants --stand_alone=true --local=32768 -o descmutants -c descmutants.pl
