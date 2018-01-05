CC=         gcc
MAKE=       make
INSTALL=    install
LOCALEPATH= $(PREFIX)/share/locale
MANPATH=    $(PREFIX)/share/man

### Compiler/linker flags   ###
### Generated via configure ###
include platform_settings

### Object files ###
OBJFILES= main.o netio.o interface.o xmlparse.o updatecheck.o conversions.o dialog.o ui-support.o categories.o about.o cookies.o setup.o net-support.o digcalc.o filters.o io-internal.o os-support.o zlib_interface.o support.o

VERSION= `grep VERSION version.h | sed s/\"//g | sed s/\#define\ VERSION\ //`
DISTDIR= snownews-$(VERSION)
DISTFILES = AUTHOR COPYING CREDITS Changelog README README.de README.patching INSTALL opml2snow \
	Makefile configure \
	doc po scripts \
	main.c interface.c netio.c xmlparse.c updatecheck.c os-support.c conversions.c dialog.c ui-support.c categories.c about.c cookies.c setup.c net-support.c digcalc.c filters.c io-internal.c zlib_interface.c support.c \
	config.h version.h main.h interface.h netio.h xmlparse.h updatecheck.h os-support.h conversions.h dialog.h ui-support.h categories.h about.h cookies.h setup.h net-support.h digcalc.h filters.h io-internal.h zlib_interface.h support.h

### Translations ###
LOCALES= de es fr it nl ru sl se zh_TW zh_CN pt_BR pl ja be@latin uk_UA
LOC=     po

### Manpages ##
LANGS= de fr it nl ru_RU.KOI8-R
MAN=   doc/man

### Compile ###

all: snownews manpages locales

snownews: $(OBJFILES)
	$(CC) $(OBJFILES) -o snownews $(LDFLAGS)

snownews-static: manpages locales
	$(CC) -s -static -o snownews main.c netio.c interface.c xmlparse.c updatecheck.c conversions.c dialog.c ui-support.c categories.c about.c cookies.c setup.c net-support.c digcalc.c filters.c io-internal.c zlib_interface.c os-support.c support.c $(CFLAGS) $(LDFLAGS) 

locales:
	for L in $(LOCALES); do \
		msgfmt -o $(LOC)/$$L.mo $(LOC)/$$L; \
	done

manpages:
	cat $(MAN)/snownews.1.in | sed s#PREFIX#$(PREFIX)# | \
	sed s/VERSION/$(VERSION)/ > $(MAN)/snownews.1
	
	for L in $(LANGS); do \
		cat $(MAN)/$$L/snownews.1.$$L.in | sed s#PREFIX#$(PREFIX)# | \
		sed s/VERSION/$(VERSION)/ > $(MAN)/$$L/snownews.1; \
	done

### Install ###

install: install-bin install-locales install-man
	@echo ""

install-bin: snownews
	if [ ! -d "$(DESTDIR)$(PREFIX)/bin" ]; then \
		mkdir -p $(DESTDIR)$(PREFIX)/bin; \
	fi
	$(INSTALL) -s snownews $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) opml2snow $(DESTDIR)$(PREFIX)/bin
	if [ ! -f "$(DESTDIR)$(PREFIX)/bin/snow2opml" ]; then \
		(cd $(DESTDIR)$(PREFIX)/bin && \
		 ln -sf opml2snow snow2opml ); \
	fi;

install-locales: locales
	for L in $(LOCALES); do \
		if [ ! -d "$(DESTDIR)$(LOCALEPATH)/$$L/LC_MESSAGES" ]; then \
			mkdir -p $(DESTDIR)$(LOCALEPATH)/$$L/LC_MESSAGES; \
		fi; \
		$(INSTALL) -m 0644 $(LOC)/$$L.mo $(DESTDIR)$(LOCALEPATH)/$$L/LC_MESSAGES/snownews.mo; \
	done

install-man: manpages
	if [ ! -d "$(DESTDIR)$(MANPATH)/man1" ]; then \
		mkdir -p $(DESTDIR)$(MANPATH)/man1; \
	fi
	$(INSTALL) -m 0644 $(MAN)/snownews.1 $(DESTDIR)$(MANPATH)/man1
	$(INSTALL) -m 0644 $(MAN)/opml2snow.1 $(DESTDIR)$(MANPATH)/man1
	
	for L in $(LANGS); do \
		if [ ! -d "$(DESTDIR)$(MANPATH)/$$L/man1" ]; then \
			mkdir -p $(DESTDIR)$(MANPATH)/$$L/man1; \
		fi; \
		$(INSTALL) -m 0644 $(MAN)/$$L/snownews.1 $(DESTDIR)$(MANPATH)/$$L/man1; \
	done

### Cleanup ###

clean: clean-bin clean-locales clean-man
	rm -f platform_settings
	@echo ""
	@echo "Run ./configure before building again!"

clean-bin:
	rm -f snownews *.o

clean-locales:
	rm -f $(LOC)/*.mo

clean-man:
	rm -f $(MAN)/snownews.1
	
	for L in $(LANGS); do \
		rm -f $(MAN)/$$L/snownews.1; \
	done

### Dist ###

dist: clean
	./configure
	mkdir $(DISTDIR)
	cp -R $(DISTFILES) $(DISTDIR)
	tar -czf $(DISTDIR).tar.gz $(DISTDIR)
	rm -rf $(DISTDIR)

dist-binary: clean snownews-static
	DISTDIR=snownews-i386-$(VERSION)
	mkdir $(DISTDIR)
	mkdir $(DISTDIR)/man
	mkdir $(DISTDIR)/man/de
	mkdir $(DISTDIR)/man/nl
	mkdir $(DISTDIR)/man/fr
	mkdir $(DISTDIR)/man/it
	mkdir $(DISTDIR)/man/ru_RU.KOI8-R
	mkdir $(DISTDIR)/po
	cp AUTHOR COPYING CREDITS Changelog README README.de INSTALL.binary snownews opml2snow $(DISTDIR)
	cp doc/man/de/snownews.1 $(DISTDIR)/man/de
	cp doc/man/nl/snownews.1 $(DISTDIR)/man/nl
	cp doc/man/fr/snownews.1 $(DISTDIR)/man/fr
	cp doc/man/it/snownews.1 $(DISTDIR)/man/it
	cp doc/man/ru_RU.KOI8-R/snownews.1 $(DISTDIR)/man/ru_RU.KOI8-R
	cp doc/man/snownews.1 $(DISTDIR)/man
	cp po/*.mo $(DISTDIR)/po
	cp scripts/install.sh $(DISTDIR)
	tar -cjf $(DISTDIR).i386.tar.bz2 $(DISTDIR)
	rm -rf $(DISTDIR)
