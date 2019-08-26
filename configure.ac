AC_PREREQ(2.61)

m4_define([required_libxml_version], [2.6.17])
m4_define([required_apertium_version], [3.5.2])
m4_define([required_lttoolbox_version], [3.5.0])

AC_INIT([apertium-recursive], [0.0.1], [awesomeevildudes@gmail.com])
AM_INIT_AUTOMAKE([apertium-recursive], [0.0.1], no-define)
AC_CONFIG_HEADER([rtx_config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CXX
AM_SANITY_CHECK
AC_LANG_CPLUSPLUS

CFLAGS="-Wall -Wextra $CFLAGS"
CXXFLAGS="-Wall -Wextra $CXXFLAGS"

AC_ARG_ENABLE(debug,
              [  --enable-debug  Enable "-g -Wall" compiler options],
              [CXXFLAGS="-g -Wall";CFLAGS="-g -Wall"])

PKG_CHECK_MODULES([LTTOOLBOX], [lttoolbox >= required_lttoolbox_version])

AC_SUBST(LTTOOLBOX_CFLAGS)
AC_SUBST(LTTOOLBOX_LIBS)

PKG_CHECK_MODULES([APERTIUM], [apertium >= required_apertium_version])

AC_SUBST(APERTIUM_CFLAGS)
AC_SUBST(APERTIUM_LIBS)

PKG_CHECK_MODULES([LIBXML], [libxml-2.0 >= required_libxml_version])

AC_SUBST(LIBXML_CFLAGS)
AC_SUBST(LIBXML_LIBS)

# Checks for libraries.
AC_CHECK_LIB(xml2, xmlReaderForFile)

AC_CHECK_FUNCS([setlocale strdup getopt_long])

AC_CHECK_DECLS([fread_unlocked, fwrite_unlocked, fgetc_unlocked, fputc_unlocked, fputs_unlocked, getopt_long, fgetwc_unlocked, fputwc_unlocked, fgetws_unlocked, fputws_unlocked])

CPPFLAGS="$CPPFLAGS $CFLAGS $APERTIUM_CFLAGS $LIBXML_CFLAGS"
LIBS="$LIBS $APERTIUM_LIBS $LIBXML_LIBS -lz"

# Checks for highest supported C++ standard
AC_LANG(C++)
AX_CHECK_COMPILE_FLAG([-std=c++20], [CXXFLAGS="$CXXFLAGS -std=c++20"], [
 AX_CHECK_COMPILE_FLAG([-std=c++2a], [CXXFLAGS="$CXXFLAGS -std=c++2a"], [
  AX_CHECK_COMPILE_FLAG([-std=c++17], [CXXFLAGS="$CXXFLAGS -std=c++17"], [
   AX_CHECK_COMPILE_FLAG([-std=c++1z], [CXXFLAGS="$CXXFLAGS -std=c++1z"], [
    AX_CHECK_COMPILE_FLAG([-std=c++14], [CXXFLAGS="$CXXFLAGS -std=c++14"], [
     AX_CHECK_COMPILE_FLAG([-std=c++1y], [CXXFLAGS="$CXXFLAGS -std=c++1y"], [
      AX_CHECK_COMPILE_FLAG([-std=c++11], [CXXFLAGS="$CXXFLAGS -std=c++11"], [])
     ])
    ])
   ])
  ])
 ])
])

AC_CONFIG_FILES([
                 Makefile
                 src/Makefile
                 ])
AC_OUTPUT
