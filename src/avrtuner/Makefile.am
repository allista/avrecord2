INCLUDES = -I$(top_srcdir)/src/libavtools $(all_includes)
METASOURCES = AUTO
AM_CXXFLAGS = $(SDL_CFLAGS) -DDATA_PATH='"$(datadir)/$(PACKAGE)"'

bin_PROGRAMS = avrtuner
avrtuner_SOURCES  = avrtuner_main.cpp avrtunerwindow.cpp videomonitor.cpp
avrtuner_CPPFLAGS = $(GTKMM_CFLAGS) $(GTKSVMM_CFLAGS) $(GTHREAD_CFLAGS)
avrtuner_LDADD    = $(top_builddir)/src/libavtools/libavtools.la \
	$(GTHREAD_LIBS) $(GTKMM_LIBS) $(GTKSVMM_LIBS) $(SDL_LIBS)
noinst_HEADERS = avrtunerwindow.h videomonitor.h

confdir = $(datadir)/$(PACKAGE)
conf_DATA = avrtunerwindow.glade

# this is where the desktop file will go
desktopdir = $(datadir)/applications
desktop_DATA = avrtuner.desktop

iconsdir = $(datadir)/icons/hicolor/scalable/apps/
icons_DATA = hi64-avrtuner-icon.svg
