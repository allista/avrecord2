INCLUDES = -I$(top_srcdir)/src/libavtools $(all_includes)
SUBDIRS = libavtools avrtuner
METASOURCES = AUTO

servicedir = /etc/init.d/
confdir = /etc/$(PACKAGE)

bin_PROGRAMS = avrecord
avrecord_SOURCES = main.cpp
avrecord_LDFLAGS = $(all_libraries)
avrecord_LDADD   = $(top_builddir)/src/libavtools/libavtools.la

conf_DATA = avrecord.cfg.template
service_SCRIPTS = avrecd
