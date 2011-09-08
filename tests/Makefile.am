INCLUDES = -I$(top_srcdir)/src/libavtools -I$(top_srcdir)/tests $(all_includes)
METASOURCES = AUTO

noinst_PROGRAMS = test v4l2-teset
test_LDFLAGS = $(KDE_RPATH) $(all_libraries)
test_SOURCES = test.cpp
test_LDADD = $(top_builddir)/src/libavtools/libavtools.la $(LIB_KDEUI)

v4l2_teset_LDFLAGS = $(all_libraries)
v4l2_teset_SOURCES = v4l2-test.cpp
v4l2_teset_LDADD = $(top_builddir)/src/libavtools/libavtools.la
