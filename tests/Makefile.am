INCLUDES = -I$(top_srcdir)/src/libavtools $(all_includes)
METASOURCES = AUTO

noinst_PROGRAMS = test
test_LDFLAGS = $(KDE_RPATH) $(all_libraries)
test_SOURCES = test.cpp
test_LDADD = $(top_builddir)/src/libavtools/libavtools.la $(LIB_KDEUI)
