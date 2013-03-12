dnl Check for libsdl
dnl On success, HAVE_LIBSDL is set to 1 and PKG_CONFIG_REQUIRES is filled when
dnl libsdl is found using pkg-config

AC_DEFUN([CHECK_LIBSDL2],
[
  AC_MSG_CHECKING([libsdl2])
  HAVE_LIBSDL=0

  # Search using pkg-config
  if test x"$HAVE_LIBSDL" = "x0"; then
    if test x"$PKG_CONFIG" != "x"; then
      PKG_CHECK_MODULES([libsdl], [sdl2], [HAVE_LIBSDL=1], [HAVE_LIBSDL=0])
    fi
  fi

  if test x"$HAVE_LIBSDL2" = "x0"; then
    for i in /usr /usr/local /opt/local /opt; do
      if test -f "$i/include/SDL2/SDL.h" -a -f "$i/lib/libsdl2.la"; then
        libsdl_CFLAGS="-I$i/include/SDL2 -L$i/lib"
        libsdl_LIBS="-L$i/lib -lsdl2"

        HAVE_LIBSDL=1
      fi
    done
  fi

  # Search the library and headers directly (last chance)
  if test x"$HAVE_LIBSDL" = "x0"; then
    AC_CHECK_HEADER(SDL2/SDL.h, [], [AC_MSG_NOTICE([The libsdl2 headers are missing])])
    AC_CHECK_LIB(sdl2, SDL_Init, [ HAVE_LBISDL=1; libsdl_LIBS="-L$i/lib -lsdl2" ], [AC_MSG_NOTICE([The libsdl2 library is missing])])
  fi

  AC_SUBST(libsdl_LIBS)
  AC_SUBST(libsdl_CFLAGS)
])
