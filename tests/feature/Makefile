.PHONY: check

tests = \
  alt \
  anonlex \
  anonlex-modifier \
  anonpat \
  anonpat-filter \
  anonpat-filter-ops \
  anonpat-modifier \
  anonpat-nospaces \
  anonpat-ops \
  disjoint-opt \
  empty \
  empty-patterns \
  filter-crosstalk \
  lexdeftag \
  lexicon-side-tags \
  lexname-space \
  lextag \
  lexnegtag \
  nontree \
  oneside \
  opt \
  or-filter \
  pairs \
  pattag \
  pattag-coherent \
  pattag-details \
  pattern-independence \
  regex \
  revsieve \
  sieve \
  sieveopt \
  slots-and-operators-nospace \
  xor-filter \
  xor-multi \

att_tests = epsilon-weight weight

sources = $(foreach test,$(tests),test-$(test).lexd)
att_sources = $(foreach test,$(att_tests),test-$(test).lexd)

check-strings: $(foreach src,$(sources),$(O)/$(src).txt.strings.check)
check-att: $(foreach src,$(att_sources),$(O)/$(src).txt.check)
check: check-strings check-att

O=.

$(O):
	mkdir $(O)
$(O)/%.lexd.txt: ../../src/lexd %.lexd | $(O)
	$^ $(LEXD_TEST_FLAGS) > $@
$(O)/%.lexd.txt.strings: $(O)/%.lexd.txt
	hfst-txt2fst $< | hfst-fst2strings -X obey-flags -c 10 | LC_ALL=C sort -u > $@
$(O)/%.strings.diff: $(O)/%.strings %.strings.gold
	diff -U0 $^ > $@; [ $$? != 2 ]
$(O)/%.strings.check: $(O)/%.strings.diff
	[ -s "$<" ] && cat "$<" && exit 1; touch $@
$(O)/%.lexd.txt.diff: $(O)/%.lexd.txt %.lexd.txt.$(O).gold
	diff -U0 $^ > $@; [ $$? != 2 ]
$(O)/%.lexd.txt.check: $(O)/%.lexd.txt.diff
	[ -s "$<" ] && cat "$<" && exit 1; touch $@
clean:
	rm $(foreach src,$(sources),$(O)/$(src).txt.strings.check)
	rm $(foreach src,$(att_sources),$(O)/$(src).txt.check)
	rmdir $(O)
