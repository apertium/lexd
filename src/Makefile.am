AM_LDFLAGS=$(LIBS)

bin_PROGRAMS = lexd

lexd_SOURCES = lexd.cc lexdcompiler.cc icu-iter.cc

lexd.1:
	$(abs_srcdir)/help2man.sh $(PACKAGE_VERSION)

man_MANS = lexd.1
