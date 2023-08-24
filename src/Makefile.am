AM_LDFLAGS=$(LIBS)
AM_CPPFLAGS = -DLEXD_I18N_DATA='"$(LEXD_I18N_DATA)"' -DLTTB_I18N_DATA='"$(LTTB_I18N_DATA)"'

bin_PROGRAMS = lexd

lexd_SOURCES = lexd.cc lexdcompiler.cc icu-iter.cc

lexd.1:
	$(abs_srcdir)/help2man.sh $(PACKAGE_VERSION)

man_MANS = lexd.1
