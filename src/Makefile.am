# set the include path for X, qt and KDE
INCLUDES = -I$(top_srcdir)/src/libavtools $(all_includes)

# let automoc handle all of the meta source files (moc)
METASOURCES = AUTO

messages: rc.cpp
	$(EXTRACTRC) `find . -name \*.ui -o -name \*.rc` > rc.cpp
	$(XGETTEXT) *.cpp -o $(podir)/avrecord.pot

KDE_ICON = AUTO

#########################################################################
# APPLICATION SECTION
#########################################################################
# this is the program that gets installed.  it's name is used for all
# of the other Makefile.am variables

# the application source, library search path, and link libraries


# this is where the desktop file will go
shelldesktopdir = $(kde_appsdir)/Utilities


# this is where the shell's XML-GUI resource file goes
shellrcdir = $(kde_datadir)/avrecord


confdir = /etc/$(PACKAGE)

servicedir = /etc/init.d/
service_SCRIPTS = avrecd


SUBDIRS = libavtools avrtuner

syssbindir       = /usr/sbin/
syssbin_PROGRAMS = avrecord
avrecord_SOURCES = main.cpp
avrecord_LDFLAGS = $(all_libraries)
avrecord_LDADD   = $(top_builddir)/src/libavtools/libavtools.la
conf_DATA = avrecord.cfg.template
