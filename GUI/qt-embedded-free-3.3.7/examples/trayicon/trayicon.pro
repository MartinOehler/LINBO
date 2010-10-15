TEMPLATE	= app
TARGET		= trayicon

CONFIG		+= qt warn_on release

REQUIRES	= large-config

HEADERS		= trayicon.h
SOURCES		= main.cpp \
		  trayicon.cpp
INTERFACES	=

win32 {
   SOURCES  	+= trayicon_win.cpp
} else:embedded {
  SOURCES	+=trayicon_qws.cpp
}
