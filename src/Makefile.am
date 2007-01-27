INCLUDES = -I$(top_srcdir)/src
METASOURCES = AUTO

noinst_HEADERS = avifile.h common.h fifo.h sndstream.h img_tools.h vidstream.h \
	utimer.h recorder.h configfile.h
noinst_LTLIBRARIES = libavtools.la
libavtools_la_SOURCES = avifile.cpp fifo.cpp sndstream.cpp img_tools.cpp \
	vidstream.cpp recorder.cpp configfile.cpp

bin_PROGRAMS = avrecord
avrecord_SOURCES = main.cpp
avrecord_LDADD = $(top_builddir)/src/libavtools.la

confdir = /etc/$(PACKAGE)
conf_DATA = avrecord.conf.sample
