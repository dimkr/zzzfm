dnl Process this file with autoconf to produce a configure script.

AC_INIT([zzzfm], [1.0.7])
AC_CONFIG_SRCDIR([src/main.c])
AM_INIT_AUTOMAKE([dist-bzip2] [subdir-objects])
AM_MAINTAINER_MODE

AM_SILENT_RULES([yes])

AM_DISABLE_STATIC

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_LIBTOOL
AM_PROG_CC_STDC
AC_PROG_INTLTOOL
AC_HEADER_STDC

# sysconfdir defaults to /usr/local/etc if this is not done:
test "$sysconfdir" = '${prefix}/etc' && sysconfdir=/etc

# libtool option to control which symbols are exported
# right now, symbols starting with _ are not exported
LIBTOOL_EXPORT_OPTIONS='-export-symbols-regex "^[[^_]].*"'
AC_SUBST(LIBTOOL_EXPORT_OPTIONS)

# check for mmap()
AC_FUNC_MMAP

AC_CHECK_LIB(m, pow)
AC_CHECK_LIB(X11, XClearWindow)

AC_DEFINE(G_DISABLE_ASSERT, [1], [do not enable g_asserts])
AC_DEFINE(G_LOG_DOMAIN, "zzzFM")

AC_ARG_WITH([bash-path],
        AC_HELP_STRING([--with-bash-path=PATH], [Absolute path to GENUINE bash v4 - other shells lack features REQUIRED by zzzFM (default: /bin/bash)]),
        AC_DEFINE_UNQUOTED(BASHPATH, "$withval", [path to bash]),
        AC_DEFINE_UNQUOTED(BASHPATH, "/bin/bash", [path to bash]))
bash_path="$withval"
# IMPORTANT: For security reasons, an absolute path must be used for genuine
# bash v4.  zzzFM runs bash as root and you WILL open root exploits if you
# disable this check, or run bash based on $PATH.
if test ! -z "$bash_path"; then
    if test ! "${bash_path:0:1}" = "/"; then
        AC_MSG_ERROR([Fatal Error: Option --with-bash-path requires absolute path.])
    fi
    echo "Modifying zzzfm-auth to use bash path..."
    bash_esc="${bash_path//\//\\/}"
    sed "s/\(\/bin\/bash\)/$bash_esc/" src/zzzfm-auth.bash > src/zzzfm-auth
else
    cp -pf src/zzzfm-auth.bash src/zzzfm-auth
fi

# gtk2 or gtk3
PKG_PROG_PKG_CONFIG
gtk2_modules="gtk+-2.0 >= 2.18.0 \
              gthread-2.0 \
              gobject-2.0"
gtk3_modules="gtk+-3.0 >= 3.0.0 \
              gthread-2.0 \
              gobject-2.0"

AC_ARG_WITH([gtk3], AS_HELP_STRING([--with-gtk3], [Build with GTK3 interface instead of GTK2 (default: with GTK2 unless GTK2 not installed)]), with_gtk3=$withval, with_gtk3="no")

AC_ARG_WITH([gtk2], AS_HELP_STRING([--with-gtk2], [Build with GTK2 interface only (default: with GTK2 unless GTK2 not installed)]), with_gtk2=$withval, with_gtk2="no")

if test x"$with_gtk3" = x"yes"; then
    # user enabled - gtk3 required
    PKG_CHECK_MODULES(GTK, [$gtk3_modules])
    AC_DEFINE([HAVE_GTK3], [1], [Use GTK3])
elif test x"$with_gtk2" = x"yes"; then
    # user enabled - gtk2 required
    PKG_CHECK_MODULES(GTK, [$gtk2_modules])
else
    # disabled - use gtk3 only if gtk2 not installed
    PKG_CHECK_MODULES(GTK, [$gtk2_modules], with_gtk3="no", [
       PKG_CHECK_MODULES(GTK, [$gtk3_modules], with_gtk3="yes", with_gtk3="no")
    ])
    if test x"$with_gtk3" = x"yes"; then
        AC_DEFINE([HAVE_GTK3], [1], [Use GTK3])
    else
        PKG_CHECK_MODULES(GTK, [$gtk2_modules])
    fi
fi
AC_SUBST([GTK_CFLAGS])
AC_SUBST([GTK_LIBS])

# Default make options
# disable deprecated warnings for now due to GTK2/GTK3 build compat
CPPFLAGS="$CPPFLAGS -Wno-deprecated-declarations -Wformat -Wformat-security -Wreturn-type -Wunused-value"

dnl linker optimizations
AC_MSG_CHECKING([whether $LD accepts --as-needed])
case `$LD --as-needed -v 2>&1 </dev/null` in
*GNU* | *'with BFD'*)
  LDFLAGS="$LDFLAGS -Wl,--as-needed"
  AC_MSG_RESULT([yes])
  ;;
*)
  AC_MSG_RESULT([no])
  ;;
esac
AC_MSG_CHECKING([whether $LD accepts -O1])
case `$LD -O1 -v 2>&1 </dev/null` in
*GNU* | *'with BFD'*)
  LDFLAGS="$LDFLAGS -Wl,-O1"
  AC_MSG_RESULT([yes])
  ;;
*)
  AC_MSG_RESULT([no])
  ;;
esac
AC_MSG_CHECKING([whether $LD accepts -Bsymbolic-functions])
case `$LD -Bsymbolic-functions -v 2>&1 </dev/null` in
*GNU* | *'with BFD'*)
  LDFLAGS="$LDFLAGS -Wl,-Bsymbolic-functions"
  AC_MSG_RESULT([yes])
  ;;
*)
  AC_MSG_RESULT([no])
  ;;
esac

AC_ARG_ENABLE(
    [cast-checks],
    AS_HELP_STRING([--enable-cast-checks],
                   [enable Glib casting checks (default: disable)]),
    cast_checks=$enableval, cast_checks="no")
G_CAST_CHECKS=""
if test x"$cast_checks" = "xno"; then
    G_CAST_CHECKS="-DG_DISABLE_CAST_CHECKS"
fi
AC_SUBST(G_CAST_CHECKS)








PKG_CHECK_MODULES(LIBUDEV, [libudev >= 143])
AC_SUBST(LIBUDEV_CFLAGS)
AC_SUBST(LIBUDEV_LIBS)









AC_ARG_ENABLE([inotify],
    AS_HELP_STRING([--disable-inotify],
        [disable Linux inotify kernel support (requires fam/gamin instead) (default: enable)]),
    use_inotify=$enableval, use_inotify="yes")

if test x"$use_inotify" = x"yes"; then
    dnl Linux inotify is enabled.
    dnl So, both of FAM and gamin are not needed.
  AC_DEFINE(USE_INOTIFY, 1, [Whether to enable Linux inotify support])
else
    dnl ***************************
    dnl *** Check for Gamin/FAM ***
    dnl ***************************
    dnl *Modified from XFCE Thunar*

    FAM_CFLAGS=""
    FAM_LIBS=""
    have_libfam=no
    AC_CHECK_HEADERS([fam.h],
    [
        AC_CHECK_LIB([fam], [FAMOpen], [have_libfam="yes" FAM_LIBS="-lfam"])
    ])

    if test x"$have_libfam" = x"yes"; then
        dnl Define appropriate symbols
        AC_DEFINE([HAVE_FAM_H], [1], [Define to 1 if you have the <fam.h> header file.])
        AC_DEFINE([HAVE_LIBFAM], [1], [Define to 1 if the File Alteration Monitor is available.])

        dnl Check for FAMNoExists (currently Gamin only)
        save_LIBS="$LIBS"
        LIBS="$LIBS $FAM_LIBS"
        AC_CHECK_FUNCS([FAMNoExists])
        LIBS="$save_LIBS"
    else
        AC_MSG_ERROR([Fatal Error: no fam or gamin detected.])
    fi
fi
AC_SUBST([FAM_CFLAGS])
AC_SUBST([FAM_LIBS])

AC_ARG_ENABLE([largefile],
    AS_HELP_STRING([--enable-largefile],
        [enable Large file support (default: yes)]),
    largefile=$enableval, largefile="yes")

if test x"$largefile" = x"yes"; then
  CPPFLAGS="$CPPFLAGS -D_LARGEFILE_SOURCE -D_REENTRANT -D_FILE_OFFSET_BITS=64"
  AC_DEFINE(_LARGEFILE64_SOURCE, 1, [Whether to enable large file support])
  AC_DEFINE(_FILE_OFFSET_BITS, 64, [File offset bits])
fi

AC_ARG_ENABLE(
    [desktop-integration],
    AS_HELP_STRING([--disable-desktop-integration],
                   [disable desktop manager integration (default: enable)]),
    desktop_integration=$enableval, desktop_integration="yes")
if test x"$desktop_integration" = "xyes"; then
  AC_DEFINE(DESKTOP_INTEGRATION, 1, [Whether to integrate desktop or not.])
fi
AM_CONDITIONAL(DESKTOP_INTEGRATION, test "$desktop_integration" = "yes")

AC_ARG_ENABLE(
    [video-thumbnails],
    AS_HELP_STRING([--disable-video-thumbnails],
                   [disable libffmpegthumbnailer video thumbnails (default: enable)]),
    video_thumbnails=$enableval, video_thumbnails="yes")
if test x"$video_thumbnails" = "xyes"; then
    PKG_CHECK_MODULES( FFMPEG, libffmpegthumbnailer, HAVE_FFMPEG=yes)
    AC_SUBST(FFMPEG_CFLAGS)
    AC_SUBST(FFMPEG_LIBS)
    AC_DEFINE(HAVE_FFMPEG, 1, [Whether to use video thumbnails or not.])
fi



dnl advanced compiler tweaking
CPPFLAGS="$CPPFLAGS -fstrict-aliasing -fmessage-length=0"

dnl data dirs
CPPFLAGS="$CPPFLAGS -DDATADIR=\\\"$datadir\\\" -DHTMLDIR=\\\"$htmldir\\\""

AC_CHECK_FUNC(euidaccess,[AC_DEFINE(HAVE_EUIDACCESS,[],[Define to 1 if euidaccess is available])])
AC_CHECK_FUNC(eaccess,[AC_DEFINE(HAVE_EACCESS,[],[Define to 1 if eaccess is available])])

AC_CHECK_FUNC(statvfs,[AC_DEFINE(HAVE_STATVFS,[],[Define to 1 if statvfs is available])])


# Gtk Builder
#AC_PATH_PROG([GTK_BUILDER_CONVERT],[gtk-builder-convert],[false])
#if test "$GTK_BUILDER_CONVERT" = "false"; then
#  AC_MSG_ERROR([gtk-builder-convert not found])
#fi

GETTEXT_PACKAGE=zzzfm
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package.])

dnl Add the languages which your application supports here.
ALL_LINGUAS="af ar da ca cs de es et eu fa fi fr gl he hr hu id it ja ko lt ml ms nb nl nn pl ps pt pt_BR ru sk sl sv tr uk ur ur_PK vi zh_CN zh_TW"
AM_GLIB_GNU_GETTEXT

AC_OUTPUT([
Makefile
etc/Makefile
src/Makefile
po/Makefile.in
data/Makefile
])

resolve_datadir="$(eval echo "$datadir")"
while [[ "${resolve_datadir:0:1}" == "$" ]]; do
    resolve_datadir="$(eval echo "$resolve_datadir")"
done

resolve_htmldir="$(eval echo "$htmldir")"
while [[ "${resolve_htmldir:0:1}" == "$" ]]; do
    resolve_htmldir="$(eval echo "$resolve_htmldir")"
done

echo
echo zzzFM...................................... : Version $VERSION
echo
echo '  -=- compile-time configuration options, per configure.ac :'
echo
echo Prefix....................................... : $prefix
echo Executable................................... : $prefix/bin/zzzfm
echo Configuration Dir............................ : "$sysconfdir/zzzfm/"

echo Icon Dir..................................... : "$resolve_datadir/pixmaps/"

echo Documentation Dir............................ : "$resolve_htmldir/"
if test ! -z "$bash_path"; then
echo Custom path to bash v4....................... : "$bash_path"
fi

echo Linux device support......................... : libudev

if test x"$use_inotify" = x"yes"; then
echo Linux file change monitor support............ : inotify
else
echo Linux file change monitor support............ : fam/gamin
fi
echo 64-bit Large file support.................... : $largefile

if test x"$with_gtk3" = x"yes"; then
echo 'GTK+ Version................................. : GTK 3'
else
echo 'GTK+ Version................................. : GTK 2'
fi
echo Desktop manager integration.................. : $desktop_integration
echo Video thumbnail support...................... : $video_thumbnails
echo
echo

if test ! -z "$bash_path"; then
echo "IMPORTANT: zzzFM is highly integrated with bash and uses features of"
echo "           bash v4.  Attempting to substitute another shell for bash WILL cause"
echo "           feature breakage, even if not immediately apparent.  Use genuine bash."
echo
fi
echo
