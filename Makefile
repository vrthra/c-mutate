
bin/descmutants: src/descmutants.pl
	mkdir -p bin
	swipl -G100g -T20g -L2g --goal=describe_mutants --stand_alone=true -o ./bin/descmutants -c src/descmutants.pl

P=wc.c
G=build/$(P)/mutants.txt
gen: bin/descmutants $(G)
	./bin/gen.rb programs/$(P)

descr: build/$(P)/mutants.txt
	@echo done

build/%/mutants.txt: bin/descmutants
	mkdir -p build/$*
	./bin/descmutants < programs/$* > $@

gcc=gcc -fprofile-arcs -ftest-coverage

clobber:
	rm -f bin/descmutants
	rm -rf build

clean:
	rm -rf build
#	pl --goal=describe_mutants --stand_alone=true --local=32768 -o descmutants -c descmutants.pl
