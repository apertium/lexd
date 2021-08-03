AC_PREREQ(2.61)

AC_INIT([lexd], [1.1.0], [awesomeevildudes@gmail.com])
AM_INIT_AUTOMAKE
AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CXX
AM_SANITY_CHECK
AC_LANG_CPLUSPLUS

CFLAGS="-Wall -Wextra -Wsign-conversion $CFLAGS"
CXXFLAGS="-Wall -Wextra -Werror=missing-field-initializers -Wsign-conversion $CXXFLAGS"

AC_ARG_ENABLE(debug,
              [  --enable-debug  Enable "-g" compiler options],
              [CXXFLAGS="-g $CXXFLAGS";CFLAGS="-g $CFLAGS"])

PKG_CHECK_MODULES([LTTOOLBOX], [lttoolbox >= 3.6.0])
PKG_CHECK_MODULES([ICU_UC], [icu-uc])
PKG_CHECK_MODULES([ICU_IO], [icu-io])

AC_CHECK_FUNCS([getopt_long])

AC_CHECK_HEADERS([string_view])

CPPFLAGS="$CPPFLAGS $CFLAGS $LTTOOLBOX_CFLAGS $ICU_CFLAGS $ICU_UC_CFLAGS $ICU_IO_CFLAGS"
LIBS="$LIBS $LTTOOLBOX_LIBS $ICU_LIBS $ICU_UC_LIBS $ICU_IO_LIBS"

# Checks for highest supported C++ standard
AC_LANG(C++)
AX_CHECK_COMPILE_FLAG([-std=c++20], [CXXFLAGS="$CXXFLAGS -std=c++20"], [
 AX_CHECK_COMPILE_FLAG([-std=c++2a], [CXXFLAGS="$CXXFLAGS -std=c++2a"], [
  AX_CHECK_COMPILE_FLAG([-std=c++17], [CXXFLAGS="$CXXFLAGS -std=c++17"], [
   AX_CHECK_COMPILE_FLAG([-std=c++1z], [CXXFLAGS="$CXXFLAGS -std=c++1z"], [
    AX_CHECK_COMPILE_FLAG([-std=c++14], [CXXFLAGS="$CXXFLAGS -std=c++14"], [
     AX_CHECK_COMPILE_FLAG([-std=c++1y], [CXXFLAGS="$CXXFLAGS -std=c++1y"], [
      AC_MSG_ERROR([Could not enable at least C++1y (C++14) - upgrade your compiler])
     ])
    ])
   ])
  ])
 ])
])

AC_CONFIG_FILES([
                 lexd.pc
                 Makefile
                 src/Makefile
                 ])
AC_OUTPUT
