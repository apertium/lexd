
SUBDIRS = src

EXTRA_DIST=autogen.sh

timing-test: all
	(cd tests || exit && ./timing.sh wad && ./timing.sh heb)
check: check-plain check-flags check-hypermin
test: check
check-clean:
	+ make -C tests/feature clean
check-plain: all tests/feature
	+ make -C tests/feature check
	+ make -C tests/feature clean
check-flags: all tests/feature
	+ make -C tests/feature check -e LEXD_TEST_FLAGS="-f"
	+ make -C tests/feature clean
check-hypermin: all tests/feature
	+ make -C tests/feature check -e LEXD_TEST_FLAGS="-t -m"
	+ make -C tests/feature clean
