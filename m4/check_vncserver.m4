dnl Check for libvncserver
dnl On success, HAVE_LIBVNCSERVER is set to 1 and PKG_CONFIG_REQUIRES is filled when
dnl libvncserver is found using pkg-config

AC_DEFUN([CHECK_LIBVNCSERVER],
[
  AC_MSG_CHECKING([libvncserver])
  HAVE_LIBVNCSERVER=0

  # Search using pkg-config
  if test x"$HAVE_LIBVNCSERVER" = "x0"; then
    if test x"$PKG_CONFIG" != "x"; then
      PKG_CHECK_MODULES([libvncserver], [vncserver], [HAVE_LIBVNCSERVER=1], [HAVE_LIBVNCSERVER=0])
      # if test x"$HAVE_FUSE" = "x1"; then
      #   # if test x"$PKG_CONFIG_REQUIRES" != x""; then
      #   #   PKG_CONFIG_REQUIRES="$PKG_CONFIG_REQUIRES,"
      #   # fi
      #   # PKG_CONFIG_REQUIRES="$PKG_CONFIG_REQUIRES lib"
      # fi
    fi
  fi

  if test x"$HAVE_LIBVNCSERVER" = "x0"; then
    for i in /usr /usr/local /opt/local /opt; do
      if test -f "$i/include/rfb/rfb.h" -a -f "$i/lib/libvncserver.la"; then
        libvncserver_CFLAGS="-I$i/include -L$i/lib"
        libvncserver_LIBS="-L$i/lib -lvncserver"

        HAVE_LIBVNCSERVER=1
      fi
    done
  fi

  # Search the library and headers directly (last chance)
  if test x"$HAVE_LIBVNCSERVER" = "x0"; then
    AC_CHECK_HEADER(rfb/rfb.h, [], [AC_MSG_ERROR([The libvncserver headers are missing])])
    AC_CHECK_LIB(vncserver, rfbGetScreen, [], [AC_MSG_ERROR([The libvncserver library is missing])])

    libvncserver_LIBS="-lvncserver"
    HAVE_LIBVNCSERVER=1
  fi

  if test x"$HAVE_LIBVNCSERVER" = "x0"; then
    AC_MSG_ERROR([libvncserver is mandatory.])
  fi

  AC_SUBST(libvncserver_LIBS)
  AC_SUBST(libvncserver_CFLAGS)
])
