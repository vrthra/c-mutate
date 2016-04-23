
bin/descmutants: src/descmutants.pl
	mkdir -p bin
	swipl --goal=describe_mutants --stand_alone=true --local=32768 -o ./bin/descmutants -c src/descmutants.pl

P=wc.c
G=build/wc.c/mutants.txt
gen: bin/descmutants $(G)
	./bin/gen.sh programs/$(P)


build/%/mutants.txt: bin/descmutants
	mkdir -p build/$*
	./bin/descmutants < programs/$* > $@


clobber:
	rm -f bin/descmutants
	rm -rf build

clean:
	rm -rf build
#	pl --goal=describe_mutants --stand_alone=true --local=32768 -o descmutants -c descmutants.pl
