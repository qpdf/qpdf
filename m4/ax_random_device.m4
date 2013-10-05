dnl @synopsis AX_RANDOM_DEVICE
dnl
dnl This macro will check for a random device, allowing the user to explicitly
dnl set the path. The user uses '--with-random=FILE' as an argument to
dnl configure.
dnl
dnl If A random device is found then HAVE_RANDOM_DEVICE is set to 1 and
dnl RANDOM_DEVICE contains the path.
dnl
dnl @category Miscellaneous
dnl @author Martin Ebourne
dnl @version 2005/07/01
dnl @license AllPermissive

AC_DEFUN([AX_RANDOM_DEVICE], [
  AC_ARG_WITH([random],
    [AC_HELP_STRING([--with-random=FILE], [Use FILE as random number seed [auto-detected]])],
    [RANDOM_DEVICE="$withval"],
    [AC_CHECK_FILE("/dev/urandom", [RANDOM_DEVICE="/dev/urandom";],
       [AC_CHECK_FILE("/dev/arandom", [RANDOM_DEVICE="/dev/arandom";],
         [AC_CHECK_FILE("/dev/random", [RANDOM_DEVICE="/dev/random";])]
       )])
    ])
  if test "x$RANDOM_DEVICE" != "x" ; then
    AC_DEFINE([HAVE_RANDOM_DEVICE], 1,
              [Define to 1 (and set RANDOM_DEVICE) if a random device is available])
    AC_SUBST([RANDOM_DEVICE])
    AC_DEFINE_UNQUOTED([RANDOM_DEVICE], ["$RANDOM_DEVICE"],
                       [Define to the filename of the random device (and set HAVE_RANDOM_DEVICE)])
  fi
])dnl
