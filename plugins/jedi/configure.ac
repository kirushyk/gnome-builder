# --enable-jedi-plugin=yes/no
AC_ARG_ENABLE([jedi-plugin],
              [AS_HELP_STRING([--enable-jedi-plugin=@<:@yes/no@:>@],
                              [Build with support for Python autocompletion using Jedi.])],
              [enable_jedi_plugin=$enableval],
              [enable_jedi_plugin=yes])

# for if ENABLE_JEDI_PLUGIN in Makefile.am
AM_CONDITIONAL(ENABLE_JEDI_PLUGIN, test x$enable_python_scripting = xyes && test x$enable_jedi_plugin = xyes)

# Ensure our makefile is generated by autoconf
AC_CONFIG_FILES([plugins/jedi/Makefile])
