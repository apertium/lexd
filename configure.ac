AC_PREREQ(2.61)

AC_INIT([lexd], [1.3.5], [awesomeevildudes@gmail.com])
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

PKG_CHECK_MODULES([LTTOOLBOX], [lttoolbox >= 3.7.1])
PKG_CHECK_MODULES([ICU_UC], [icu-uc])
PKG_CHECK_MODULES([ICU_IO], [icu-io])

AC_CHECK_FUNCS([getopt_long])

CPPFLAGS="$CPPFLAGS $CFLAGS $LTTOOLBOX_CFLAGS $ICU_CFLAGS $ICU_UC_CFLAGS $ICU_IO_CFLAGS"
LIBS="$LIBS $LTTOOLBOX_LIBS $ICU_LIBS $ICU_UC_LIBS $ICU_IO_LIBS"

# Checks for highest supported C++ standard
AC_LANG(C++)
for version in 26 2c 23 2b 20 2a 17; do
  version_flag="-std=c++${version}"
  AX_CHECK_COMPILE_FLAG([${version_flag}], [break], [version_flag=none])
done
AS_IF([test "$version_flag" == none], [
  AC_MSG_ERROR([Could not enable at least C++17 - upgrade your compiler])
])
CXXFLAGS="$CXXFLAGS ${version_flag}"

_found_utf8=no
for ipath in /usr /usr/local /opt /opt/local; do
  for upath in utf8cpp utfcpp utf8; do
    AC_CHECK_FILE("$ipath/include/$upath/utf8.h", [CPPFLAGS="-I$ipath/include/$upath $CPPFLAGS"; _found_utf8=yes])
  done
done
if test $_found_utf8 = no
then
  AC_CHECK_HEADER([utf8cpp/utf8.h], [CPPFLAGS="-I/usr/include/utf8cpp/ $CPPFLAGS"], [
    AC_CHECK_HEADER([utf8.h], [], [AC_MSG_ERROR([Could not find utf8.h from utf8cpp / utfcpp])])
  ])
fi

AC_CONFIG_FILES([
                 lexd.pc
                 Makefile
                 src/Makefile
                 ])
AC_OUTPUT
