.PHONY: check

tests = alt anonlex anonpat anonpat-nospaces anonpat-ops lexname-space lextag lexnegtag oneside opt pairs pattag pattag-details revsieve sieve sieveopt
sources = $(foreach test,$(tests),test-$(test).lexd)

check: check-plain check-flags
check-plain: $(foreach src,$(sources),$(src).txt.strings.check)
check-flags: $(foreach src,$(sources),$(src).txt.strings-f.check)

%.lexd.txt: ../../src/lexd %.lexd
	$^ > $@
%.lexd-f.txt: ../../src/lexd %.lexd
	$^ -f > $@

%.lexd.txt.strings: %.lexd.txt
	hfst-txt2fst $< | hfst-fst2strings | LC_ALL=C sort > $@
%.lexd-f.txt.strings: %.lexd-f.txt
	hfst-txt2fst $< | hfst-fst2strings -X obey-flags | LC_ALL=C sort > $@

%.strings.diff: %.strings %.strings.gold
	diff -U0 $^ > $@; [ $$? != 2 ]
%.strings-f.diff: %.strings %.strings.gold
	diff -U0 $^ > $@; [ $$? != 2 ]

%.strings.check: %.strings.diff
	[ -s "$<" ] && cat "$<" && exit 1; touch $@
%.strings-f.check: %.strings-f.diff
	[ -s "$<" ] && cat "$<" && exit 1; touch $@
