.PHONY: check

tests = alt oneside opt pairs revsieve sieve sieveopt
sources = $(foreach test,$(tests),test-$(test).lexd)

check: $(foreach src,$(sources),$(src).txt.strings.check)
%.lexd.txt: ../../src/lexd %.lexd
	$^ > $@
%.lexd.txt.strings: %.lexd.txt
	hfst-txt2fst $< | hfst-fst2strings | LC_ALL=C sort > $@
%.strings.diff: %.strings %.strings.gold
	diff -U0 $^ > $@; [ $$? != 2 ]
%.strings.check: %.strings.diff
	[ -s "$<" ] && cat "$<" && exit 1; touch $@