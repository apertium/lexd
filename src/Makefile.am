AM_LDFLAGS=$(LIBS)
AM_CPPFLAGS = -DAXD_I18N_DATA='"$(AXD_I18N_DATA)"' -DALT_I18N_DATA='"$(ALT_I18N_DATA)"'

bin_PROGRAMS = lexd

lexd_SOURCES = lexd.cc lexdcompiler.cc icu-iter.cc

lexd.1:
	$(abs_srcdir)/help2man.sh $(PACKAGE_VERSION)

man_MANS = lexd.1
