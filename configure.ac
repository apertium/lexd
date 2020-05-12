AC_PREREQ(2.61)

m4_define([required_lttoolbox_version], [3.5.2])

AC_INIT([lexd], [0.0.1], [awesomeevildudes@gmail.com])
AM_INIT_AUTOMAKE([lexd], [0.0.1], no-define)
AC_CONFIG_HEADER([lexd_config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CXX
AM_SANITY_CHECK
AC_LANG_CPLUSPLUS

CFLAGS="-Wall -Wextra -Wsign-conversion $CFLAGS"
CXXFLAGS="-Wall -Wextra -Wsign-conversion $CXXFLAGS"

AC_ARG_ENABLE(debug,
              [  --enable-debug  Enable "-g -Wall" compiler options],
              [CXXFLAGS="-g -Wall";CFLAGS="-g -Wall"])

PKG_CHECK_MODULES([LTTOOLBOX], [lttoolbox >= required_lttoolbox_version])
PKG_CHECK_MODULES([ICU_UC], [icu-uc])
PKG_CHECK_MODULES([ICU_IO], [icu-io])

AC_SUBST(LTTOOLBOX_CFLAGS)
AC_SUBST(LTTOOLBOX_LIBS)
AC_SUBST(ICU_UC_CFLAGS)
AC_SUBST(ICU_UC_LIBS)
AC_SUBST(ICU_IO_LIBS)
AC_SUBST(ICU_IO_CFLAGS)

AC_CHECK_FUNCS([setlocale strdup getopt_long])

CPPFLAGS="$CPPFLAGS $CFLAGS $LTTOOLBOX_CFLAGS $ICU_UC_CFLAGS $ICU_IO_CFLAGS"
LIBS="$LIBS $LTTOOLBOX_LIBS $ICU_UC_LIBS $ICU_IO_LIBS"

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
