INCLUDES = -I$(top_srcdir)/src/libavtools $(all_includes)
METASOURCES = AUTO

bin_PROGRAMS = avrtuner avrtuner_kde3
avrtuner_kde3_LDADD = $(top_builddir)/src/libavtools/libavtools.la $(LIB_KDEUI) \
	$(SDL_LIBS)
avrtuner_kde3_LDFLAGS = $(KDE_RPATH) $(all_libraries)
avrtuner_kde3_SOURCES = avrtuner_main_kde3.cpp monitorthread.cpp tunerform.cpp tunerformbase.ui

avrtuner_SOURCES = avrtuner_main.cpp avrtunerwindow.cpp videomonitor.cpp
avrtuner_CPPFLAGS = $(GTKMM_CFLAGS) $(GTKSVMM_CFLAGS)
avrtuner_LDADD = $(top_builddir)/src/libavtools/libavtools.la $(GTKMM_LIBS) \
	$(GTKSVMM_LIBS) $(SDL_LIBS)
noinst_HEADERS = avrtunerwindow.glade avrtunerwindow.h monitorthread.h \
	tunerform.h videomonitor.h

KDE_ICON = AUTO

# this is where the desktop file will go
shelldesktopdir = $(kde_appsdir)/Utilities
shelldesktop_DATA = avrtuner.desktop

# this is where the shell's XML-GUI resource file goes
shellrcdir = $(kde_datadir)/avrecord
shellrc_DATA = avrtunerui.rc

AM_CFLAGS = $(SDL_CFLAGS)
AM_CXXFLAGS = $(SDL_CFLAGS)
