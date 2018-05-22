################ Source files ##########################################

man/LMANS	:= $(wildcard man/snownews.*.1)
man/MANS	:= $(filter-out ${man/LMANS},$(wildcard man/*.1))
man/LANGS	:= $(subst man/snownews.,,$(man/LMANS:.1=))

################ Installation ##########################################
ifdef MANPATH
.PHONY:	man/install man/uninstall

man/IPREFIX	:= ${PKGDIR}${MANPATH}
man/MANSI	:= $(subst man/,${man/IPREFIX}/man1/,${man/MANS})
man/LMANSI	:= $(foreach l,${man/LANGS},${man/IPREFIX}/${l}/man1/snownews.1)

man/MANI	:= ${man/MANSI}
${man/MANSI}:	${man/IPREFIX}/man1/%:	man/%
	@echo "Installing $@ ..."
	@${INSTALLDATA} $< $@

ifdef LOCALEPATH
man/MANI	+= ${man/LMANSI}
${man/LMANSI}:	${man/IPREFIX}/%/man1/snownews.1:	man/snownews.%.1
	@echo "Installing $@ ..."
	@${INSTALLDATA} $< $@
endif

install:	man/install
man/install:	${man/MANI}

uninstall:	man/uninstall
man/uninstall:
	@if [ -f ${man/IPREFIX}/man1/${NAME}.1 ]; then\
	    echo "Uninstalling ${NAME} man pages ...";\
	    rm -f ${man/MANI};\
	fi
endif
