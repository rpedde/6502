dnl Check for libsdl
dnl On success, HAVE_LIBSDL is set to 1 and PKG_CONFIG_REQUIRES is filled when
dnl libsdl is found using pkg-config

AC_DEFUN([CHECK_LIBSDL],
[
  AC_MSG_CHECKING([libsdl])
  HAVE_LIBSDL=0

  # Search using pkg-config
  if test x"$HAVE_LIBSDL" = "x0"; then
    if test x"$PKG_CONFIG" != "x"; then
      PKG_CHECK_MODULES([libsdl], [sdl], [HAVE_LIBSDL=1], [HAVE_LIBSDL=0])
      # if test x"$HAVE_FUSE" = "x1"; then
      #   # if test x"$PKG_CONFIG_REQUIRES" != x""; then
      #   #   PKG_CONFIG_REQUIRES="$PKG_CONFIG_REQUIRES,"
      #   # fi
      #   # PKG_CONFIG_REQUIRES="$PKG_CONFIG_REQUIRES lib"
      # fi
    fi
  fi

  for i in /usr /usr/local /opt/local /opt; do
    if test -f "$i/include/SDL/SDL.h" -a -f "$i/lib/libsdl.la"; then
      libsdl_CFLAGS="-I$i/include/SDL -L$i/lib"
      libsdl_LIBS="-L$i/lib -lsdl"

      HAVE_LIBSDL=1
    fi
  done

  # Search the library and headers directly (last chance)
  if test x"$HAVE_LIBSDL" = "x0"; then
    AC_CHECK_HEADER(SDL/SDL.h, [], [AC_MSG_ERROR([The libsdl headers are missing])])
    AC_CHECK_LIB(sdl, SDL_Init, [], [AC_MSG_ERROR([The libsdl library is missing])])

    libsdl_LIBS="-L$i/lib -lsdl"
    HAVE_LIBSDL=1
  fi

  if test x"$HAVE_LIBSDL" = "x0"; then
    AC_MSG_ERROR([libsdl is mandatory.])
  fi

  AC_SUBST(libsdl_LIBS)
  AC_SUBST(libsdl_CFLAGS)
])
