
SUBDIRS = src

EXTRA_DIST=autogen.sh

test: all
	(cd tests || exit && ./wad.sh && ./heb.sh)
