
SUBDIRS = src

EXTRA_DIST=autogen.sh

test: all
	(cd tests || exit && ./timing.sh wad && ./timing.sh heb)
