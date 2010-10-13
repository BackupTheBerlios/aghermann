SUBDIRS = core

AM_CFLAGS = -Wall -std=c99 $(AGH_CFLAGS) $(GSL_CFLAGS)

INCLUDES = -I.

bin_PROGRAMS = aghermann

aghermann_SOURCES = \
	main.c \
	constants.h \
	iface.h \
	core/libaghcore.la \
	ui/misc.h \
	ui/misc.c \
	ui/ui.c \
	ui/loadsave.c \
	ui/settings.c \
	ui/measurements.c \
	ui/scoring-facility.c \
	ui/modelrun-facility.c \
	ui/simulations.c \
	ui/tunables.c \
	ui/statusbar.c \
	ui/ui.h

aghermann_LDADD = $(AGH_LIBS) $(GSL_LIBS) core/libaghcore.la core/libaghcore-glib.la