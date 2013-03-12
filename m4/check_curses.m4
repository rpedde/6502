dnl Check for libcurses
dnl On success, HAVE_LIBCURSES is set to 1 and PKG_CONFIG_REQUIRES is filled when
dnl libcurses is found using pkg-config

AC_DEFUN([CHECK_LIBCURSES],
[
  AC_MSG_CHECKING([libcurses])
  HAVE_LIBCURSES=0

  # Search using pkg-config
  if test x"$HAVE_LIBCURSES" = "x0"; then
    if test x"$PKG_CONFIG" != "x"; then
      PKG_CHECK_MODULES([libcurses], [ncurses], [HAVE_LIBCURSES=1], [HAVE_LIBCURSES=0])
      # if test x"$HAVE_FUSE" = "x1"; then
      #   # if test x"$PKG_CONFIG_REQUIRES" != x""; then
      #   #   PKG_CONFIG_REQUIRES="$PKG_CONFIG_REQUIRES,"
      #   # fi
      #   # PKG_CONFIG_REQUIRES="$PKG_CONFIG_REQUIRES lib"
      # fi
    fi
  fi

  if test x"$HAVE_LIBCURSES" = "x0"; then
    for i in /usr /usr/local /opt/local /opt; do
      if test -f "$i/include/curses.h" -a -f "$i/lib/libcurses.la"; then
        libcurses_CFLAGS="-I$i/include -L$i/lib"
        libcurses_LIBS="-L$i/lib -lcurses"

        HAVE_LIBCURSES=1
      fi
    done
  fi

  # Search the library and headers directly (last chance)
  if test x"$HAVE_LIBCURSES" = "x0"; then
    AC_CHECK_HEADER(curses.h, [], [AC_MSG_ERROR([The libcurses headers are missing])])
    AC_CHECK_LIB(curses, initscr, [], [AC_MSG_ERROR([The libcurses library is missing])])

    libcurses_LIBS="-L$i/lib -lcurses"
    HAVE_LIBCURSES=1
  fi

  if test x"$HAVE_LIBCURSES" = "x0"; then
    AC_MSG_ERROR([libcurses is mandatory.])
  fi

  AC_SUBST(libcurses_LIBS)
  AC_SUBST(libcurses_CFLAGS)
])
