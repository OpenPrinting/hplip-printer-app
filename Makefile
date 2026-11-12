#
# Makefile for the HPLIP Printer Application
#
# Copyright © 2020-2021 by Till Kamppeter
# Copyright © 2020 by Michael R Sweet
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

# Version and
VERSION		=	1.0
prefix		=	/usr
sysconfdir	=	/etc
localstatedir	=	/var
includedir	=	$(prefix)/include
bindir		=	$(prefix)/bin
libdir		=	$(prefix)/lib
mandir		=	$(prefix)/share/man
ppddir		=	$(prefix)/share/ppd
statedir	=	$(localstatedir)/lib/hplip-printer-app
spooldir	=	$(localstatedir)/spool/hplip-printer-app
serverbin	=	$(prefix)/lib/hplip-printer-app
resourcedir	=	$(prefix)/share/hplip-printer-app
# CUPS detection: prioritize CUPS 3.x (cups3), then CUPS 2.5 (cups), then legacy cups-config
CUPS_PKG	:=	$(shell pkg-config --exists cups3 && echo cups3 || (pkg-config --exists cups && echo cups || echo ""))

ifneq ($(CUPS_PKG),)
CUPS_CFLAGS	:=	$(shell pkg-config --cflags $(CUPS_PKG))
CUPS_LDFLAGS	:=	
CUPS_LIBS	:=	$(shell pkg-config --libs $(CUPS_PKG))
cupsserverbin	:=	$(shell pkg-config --variable=prefix $(CUPS_PKG))/lib/cups
else
CUPSCONFIG	:=	$(shell which cups-config 2>/dev/null)
ifneq ($(CUPSCONFIG),)
CUPS_CFLAGS	:=	$(shell cups-config --cflags)
CUPS_LDFLAGS	:=	$(shell cups-config --ldflags)
CUPS_LIBS	:=	$(shell cups-config --image --libs 2>/dev/null || cups-config --libs)
cupsserverbin	:=	$(shell cups-config --serverbin)
else
cupsserverbin	:=	
endif
endif

# PAPPL detection: prioritize PAPPL 2.x (pappl2), fallback to PAPPL 1.x (pappl)
PAPPL_PKG	:=	$(shell pkg-config --exists pappl2 && echo pappl2 || echo pappl)

unitdir 	=	`pkg-config --variable=systemdsystemunitdir systemd 2>/dev/null || echo ""`
HPLIP_CONF_DIR  =       $(sysconfdir)/hp
HPLIP_PLUGIN_STATE_DIR = $(localstatedir)/lib/hp

# Compiler/linker options...
OPTIM		=	-Os -g
DIRS		=	-DHPLIP_CONF_DIR=\"$(HPLIP_CONF_DIR)\" -DHPLIP_PLUGIN_STATE_DIR=\"$(HPLIP_PLUGIN_STATE_DIR)\"
ifdef HPLIP_PLUGIN_ALT_DIR
DIRS		+=	-DHPLIP_PLUGIN_ALT_DIR=\"$(HPLIP_PLUGIN_ALT_DIR)\"
endif
CFLAGS		+=	`pkg-config --cflags $(PAPPL_PKG)` $(CUPS_CFLAGS) `pkg-config --cflags libppd` `pkg-config --cflags libcupsfilters` `pkg-config --cflags libpappl-retrofit` `pkg-config --cflags libcurl` `pkg-config --cflags libcrypto` $(DIRS) $(OPTIM)
ifdef VERSION
CFLAGS		+=	-DSYSTEM_VERSION_STR="\"$(VERSION)\""
ifndef MAJOR
MAJOR		=	`echo $(VERSION) | perl -p -e 's/^(\d+).*$$/\1/'`
endif
ifndef MINOR
MINOR		=	`echo $(VERSION) | perl -p -e 's/^\d+\D+(\d+).*$$/\1/'`
endif
ifndef PATCH
PATCH		=	`echo $(VERSION) | perl -p -e 's/^\d+\D+\d+\D+(\d+).*$$/\1/'`
endif
ifndef PACKAGE
PACKAGE		=	`echo $(VERSION) | perl -p -e 's/^\d+\D+\d+\D+\d+\D+(\d+).*$$/\1/'`
endif
endif
ifdef MAJOR
CFLAGS		+=	-DSYSTEM_VERSION_ARR_0=$(MAJOR)
endif
ifdef MINOR
CFLAGS		+=	-DSYSTEM_VERSION_ARR_1=$(MINOR)
endif
ifdef PATCH
CFLAGS		+=	-DSYSTEM_VERSION_ARR_2=$(PATCH)
endif
ifdef PACKAGE
CFLAGS		+=	-DSYSTEM_VERSION_ARR_3=$(PACKAGE)
endif
ifdef SNAP
CFLAGS		+=	-DSNAP=$(SNAP)
endif
LDFLAGS		+=	$(OPTIM) $(CUPS_LDFLAGS)
LIBS		+=	`pkg-config --libs $(PAPPL_PKG)` $(CUPS_LIBS) `pkg-config --libs libppd` `pkg-config --libs libcupsfilters` `pkg-config --libs libpappl-retrofit` `pkg-config --libs libcurl` `pkg-config --libs libcrypto`


# Targets...
OBJS		=	hplip-printer-app.o
TARGETS		=	hplip-printer-app


# General build rules...
.SUFFIXES:	.c .o
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<


# Targets...
all:		$(TARGETS)

clean:
	rm -f $(TARGETS) $(OBJS)

install:	$(TARGETS)
	mkdir -p $(DESTDIR)$(bindir)
	cp $(TARGETS) $(DESTDIR)$(bindir)
	mkdir -p $(DESTDIR)$(mandir)/man1
	cp hplip-printer-app.1 $(DESTDIR)$(mandir)/man1
	mkdir -p $(DESTDIR)$(ppddir)
	mkdir -p $(DESTDIR)$(statedir)/ppd
	mkdir -p $(DESTDIR)$(spooldir)
	mkdir -p $(DESTDIR)$(resourcedir)
	cp testpage.ps $(DESTDIR)$(resourcedir)
	if test "x$(cupsserverbin)" != x && [ -d $(cupsserverbin) ]; then \
	  mkdir -p $(DESTDIR)$(libdir); \
	  touch $(DESTDIR)$(serverbin) 2> /dev/null || :; \
	  if rm $(DESTDIR)$(serverbin) 2> /dev/null; then \
	    ln -s $(cupsserverbin) $(DESTDIR)$(serverbin); \
	  fi; \
	  mkdir -p $(DESTDIR)$(cupsserverbin)/backend; \
	  cp HP $(DESTDIR)$(cupsserverbin)/backend; \
	else \
	  mkdir -p $(DESTDIR)$(serverbin)/filter; \
	  mkdir -p $(DESTDIR)$(serverbin)/backend; \
	  cp HP $(DESTDIR)$(serverbin)/backend; \
	fi
	if test "x$(unitdir)" != x; then \
	  mkdir -p $(DESTDIR)$(unitdir); \
	  cp hplip-printer-app.service $(DESTDIR)$(unitdir); \
	fi

hplip-printer-app:	$(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

$(OBJS):	Makefile
