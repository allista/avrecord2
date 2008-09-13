INCLUDES = -I$(top_srcdir)/src/libavtools $(all_includes)
METASOURCES = AUTO

bin_PROGRAMS = avrtuner
avrtuner_LDADD = $(top_builddir)/src/libavtools/libavtools.la $(LIB_KDEUI)
avrtuner_LDFLAGS = $(KDE_RPATH) $(all_libraries)
avrtuner_SOURCES = avrtuner_main.cpp tunerformbase.ui tunerform.cpp \
	monitorthread.cpp kdetvview.cpp
noinst_HEADERS = tunerform.h kdetvview.h monitorthread.h

KDE_ICON = AUTO

# this is where the desktop file will go
shelldesktopdir   = $(kde_appsdir)/Utilities
shelldesktop_DATA = avrtuner.desktop

# this is where the shell's XML-GUI resource file goes
shellrcdir = $(kde_datadir)/avrecord
shellrc_DATA = avrtunerui.rc
