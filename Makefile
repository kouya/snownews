include Config.mk

################ Source files ##########################################

EXE	:= $O${NAME}
SRCS	:= $(wildcard *.c)
OBJS	:= $(addprefix $O,$(SRCS:.c=.o))
DEPS	:= ${OBJS:.o=.d}
CONFS	:= Config.mk config.h
ONAME   := $(notdir $(abspath $O))

################ Compilation ###########################################

.PHONY:	all run

all:	${EXE}

run:	${EXE}
	${EXE}

${EXE}:	${OBJS}
	@echo "Linking $@ ..."
	@${CC} -o $@ $^ ${LDFLAGS}

${EXE}-static: ${SRCS}
	${CC} -s -static -o $@ ${CFLAGS} ${SRCS} ${LDFLAGS}

$O%.o:	%.c
	@echo "    Compiling $< ..."
	@${CC} ${CFLAGS} -MMD -MT "$(<:.c=.s) $@" -o $@ -c $<

%.s:	%.c
	@echo "    Compiling $< to assembly ..."
	@${CC} ${CFLAGS} -S -o $@ -c $<

include man/Module.mk
include po/Module.mk

################ Installation ##########################################
ifdef BINDIR

.PHONY:	install uninstall install-bin uninstall-bin

EXEI	:= ${BINDIR}/$(notdir ${EXE})
O2SI	:= ${BINDIR}/opml2snow
S2OI	:= ${BINDIR}/snow2opml

install:	install-bin
install-bin:	${EXEI} ${O2SI} ${S2OI}

${EXEI}:	${EXE}
	@echo "Installing $@ ..."
	@${INSTALLEXE} $< $@
${O2SI}:	opml2snow
	@echo "Installing $@ ..."
	@${INSTALLSCR} $< $@
${S2OI}:	${O2SI}
	@echo "Installing $@ ..."
	@(cd ${BINDIR}; rm -f $@; ln -s $(notdir $<) $(notdir $@))

uninstall:	uninstall-bin
uninstall-bin:
	@if [ -f ${EXEI} ]; then\
	    echo "Uninstalling ${EXEI} ...";\
	    rm -f ${EXEI} ${O2SI} ${S2OI};\
	fi

endif
################ Maintenance ###########################################

.PHONY:	clean distclean maintainer-clean dist dist-binary

clean:
	@if [ -h ${ONAME} ]; then\
	    rm -f ${EXE} ${EXE}-static ${OBJS} ${DEPS} $O.d ${ONAME};\
	    rmdir ${BUILDDIR};\
	fi

distclean:	clean
	@rm -f ${CONFS} config.status

maintainer-clean: distclean

$O.d:	${BUILDDIR}/.d
	@[ -h ${ONAME} ] || ln -sf ${BUILDDIR} ${ONAME}
$O%/.d:	$O.d
	@[ -d $(dir $@) ] || mkdir $(dir $@)
	@touch $@
${BUILDDIR}/.d:	Makefile
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	@touch $@

Config.mk:	Config.mk.in
config.h:	config.h.in
${OBJS}:	Makefile ${CONFS} $O.d
${CONFS}:	configure
	@if [ -x config.status ]; then echo "Reconfiguring ...";\
	    ./config.status;\
	else echo "Running configure ...";\
	    ./configure;\
	fi

-include ${DEPS}

################ Dist ##################################################

.PHONY:	dist dist-binary

DISTDIR		:= ${NAME}-${VERSION}
DISTFILES	:= AUTHOR COPYING CREDITS Changelog README README.de README.patching opml2snow \
	Makefile configure doc po scripts ${SRCS} $(wildcard *.h) config.h

dist: clean
	./configure
	mkdir ${DISTDIR}
	cp -R ${DISTFILES} ${DISTDIR}
	tar -czf ${DISTDIR}.tar.gz ${DISTDIR}
	rm -rf ${DISTDIR}

dist-binary: clean ${EXE}-static
	DISTDIR=${NAME}-i386-${VERSION}
	mkdir ${DISTDIR}
	mkdir ${DISTDIR}/man
	mkdir ${DISTDIR}/man/de
	mkdir ${DISTDIR}/man/nl
	mkdir ${DISTDIR}/man/fr
	mkdir ${DISTDIR}/man/it
	mkdir ${DISTDIR}/man/ru_RU.KOI8-R
	mkdir ${DISTDIR}/po
	cp AUTHOR COPYING CREDITS Changelog README README.de scripts/INSTALL.binary opml2snow ${DISTDIR}
	cp ${EXE}-static ${DISTDIR}/${NAME}
	cp doc/man/de/${NAME}.1 ${DISTDIR}/man/de
	cp doc/man/nl/${NAME}.1 ${DISTDIR}/man/nl
	cp doc/man/fr/${NAME}.1 ${DISTDIR}/man/fr
	cp doc/man/it/${NAME}.1 ${DISTDIR}/man/it
	cp doc/man/ru_RU.KOI8-R/${NAME}.1 ${DISTDIR}/man/ru_RU.KOI8-R
	cp doc/man/${NAME}.1 ${DISTDIR}/man
	cp po/*.mo ${DISTDIR}/po
	cp scripts/install.sh ${DISTDIR}
	tar -cjf ${DISTDIR}.i386.tar.bz2 ${DISTDIR}
	rm -rf ${DISTDIR}
