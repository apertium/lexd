
SUBDIRS = src

EXTRA_DIST=autogen.sh

timing-test: all
	(cd tests || exit && ./timing.sh wad && ./timing.sh heb)
check: all tests/feature
	+ make -C tests/feature check
test: check
