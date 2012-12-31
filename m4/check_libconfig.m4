dnl Check for libconfig
dnl On success, HAVE_LIBCONFIG is set to 1 and PKG_CONFIG_REQUIRES is filled when
dnl libconfig is found using pkg-config

AC_DEFUN([CHECK_LIBCONFIG],
[
  AC_MSG_CHECKING([libconfig])
  HAVE_LIBCONFIG=0

  # Search using pkg-config
  if test x"$HAVE_LIBCONFIG" = "x0"; then
    if test x"$PKG_CONFIG" != "x"; then
      PKG_CHECK_MODULES([libconfig], [libconfig], [HAVE_LIBCONFIG=1], [HAVE_LIBCONFIG=0])
      # if test x"$HAVE_FUSE" = "x1"; then
      #   # if test x"$PKG_CONFIG_REQUIRES" != x""; then
      #   #   PKG_CONFIG_REQUIRES="$PKG_CONFIG_REQUIRES,"
      #   # fi
      #   # PKG_CONFIG_REQUIRES="$PKG_CONFIG_REQUIRES lib"
      # fi
    fi
  fi

  for i in /usr /usr/local /opt/local /opt; do
    if test -f "$i/include/libconfig.h" -a -f "$i/lib/libconfig.la"; then
      libconfig_CFLAGS="-I$i/include -L$i/lib"
      libconfig_LIBS="-L$i/lib -lconfig"

      HAVE_LIBCONFIG=1
    fi
  done

  # Search the library and headers directly (last chance)
  if test x"$HAVE_LIBCONFIG" = "x0"; then
    AC_CHECK_HEADER(libconfig.h, [], [AC_MSG_ERROR([The libconfig headers are missing])])
    AC_CHECK_LIB(config, config_init, [], [AC_MSG_ERROR([The libconfig library is missing])])

    libconfig_LIBS="-L$i/lib -lconfig"
    HAVE_LIBCONFIG=1
  fi

  if test x"$HAVE_LIBCONFIG" = "x0"; then
    AC_MSG_ERROR([libconfig is mandatory.])
  fi

  AC_SUBST(libconfig_LIBS)
  AC_SUBST(libconfig_CFLAGS)
])
