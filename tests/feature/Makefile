.PHONY: check

tests = \
  alt \
  anonlex \
  anonlex-modifier \
  anonpat \
  anonpat-modifier \
  anonpat-nospaces \
  anonpat-ops \
  empty-patterns \
  lexdeftag \
  lexname-space \
  lextag \
  lexnegtag \
  oneside \
  opt \
  pairs \
  pattag \
  pattag-coherent \
  pattag-details \
  pattern-independence \
  revsieve \
  sieve \
  sieveopt \
  slots-and-operators-nospace \

sources = $(foreach test,$(tests),test-$(test).lexd)

check: $(foreach src,$(sources),$(src).txt.strings.check)

%.lexd.txt: ../../src/lexd %.lexd
	$^ $(LEXD_TEST_FLAGS) > $@
%.lexd.txt.strings: %.lexd.txt
	hfst-txt2fst $< | hfst-fst2strings -X obey-flags -c 10 | LC_ALL=C sort -u > $@
%.strings.diff: %.strings %.strings.gold
	diff -U0 $^ > $@; [ $$? != 2 ]
%.strings.check: %.strings.diff
	[ -s "$<" ] && cat "$<" && exit 1; touch $@

clean:
	rm *.check
