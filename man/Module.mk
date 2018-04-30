################ Source files ##########################################

man/MANS	:= $(wildcard man/man1/*.1)
man/LANGS	:= $(notdir $(filter-out man/Module.mk man/man1,$(wildcard man/*)))
man/LMANS	:= $(foreach l,${man/LANGS},$(wildcard man/${l}/man1/*.1))

################ Installation ##########################################
ifdef MANPATH

.PHONY:	man/install man/uninstall

man/IPREFIX	:= ${PKGDIR}${MANPATH}
man/MANSI	:= $(subst man/,${man/IPREFIX}/,${man/MANS})
man/LMANSI	:= $(foreach l,${man/LANGS},$(subst man/,${man/IPREFIX}/,$(wildcard man/${l}/man1/*.1)))
man/MANI	:= ${man/MANSI} ${man/LMANSI}

${man/MANI}:	${man/IPREFIX}/%:	man/%
	@echo "Installing $@ ..."
	@${INSTALLDATA} $< $@

install:	man/install
man/install:	${man/MANI}

uninstall:	man/uninstall
man/uninstall:
	@if [ -f ${man/IPREFIX}/man1/${NAME}.1 ]; then\
	    echo "Uninstalling ${NAME} man pages ...";\
	    rm -f ${man/MANI};\
	fi
endif
