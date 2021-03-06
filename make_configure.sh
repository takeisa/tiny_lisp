#!/bin/sh

cat > Makefile.am <<\
    "--------------"
#AM_YFLAGS=-d
AM_CFLAGS=-g -Wall -O2
bin_PROGRAMS=tlisp
tlisp_SOURCES=main.c mpc.c mpc.h
tlisp_CFLAGS=$(AM_CFLAGS) -O0
tlisp_LDADD=-ledit
#calc_LDADD=@LEXLIB@
--------------

autoscan

sed -e 's/FULL-PACKAGE-NAME/tlisp/' \
    -e 's/VERSION/1/' \
    -e 's|BUG-REPORT-ADDRESS|/dev/null|' \
    -e '10i\
AM_INIT_AUTOMAKE\
'\
    -e '/AC_PROG_CC/a\
AC_PROG_CC_C99\
# AM_PROG_LEX\
# AC_PROG_YACC\
#CFLAGS="-g -O0"\
'\
    < configure.scan > configure.ac

touch NEWS README AUTHORS ChangeLog
autoreconf -iv
./configure
make distcheck
