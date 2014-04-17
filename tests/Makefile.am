INCLUDES = -I$(top_srcdir)/src/libavtools -I$(top_srcdir)/tests $(all_includes)
METASOURCES = AUTO

noinst_PROGRAMS = gtkapp v4l2-teset
noinst_HEADERS = avrtunerwindow.h gtkapp.glade webcamserver.h


v4l2_teset_LDFLAGS = $(all_libraries)
v4l2_teset_SOURCES = v4l2-test.cpp webcamserver.cpp
v4l2_teset_LDADD = $(top_builddir)/src/libavtools/libavtools.la

gtkapp_SOURCES = avrtunerwindow.cpp gtkapp.cpp
gtkapp_CPPFLAGS = $(GTKMM_CFLAGS) $(GTKSVMM_CFLAGS)
gtkapp_LDADD = $(GTKMM_LIBS) $(GTKSVMM_LIBS)
