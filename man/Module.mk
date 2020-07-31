################ Source files ##########################################

man/lmans	:= $(wildcard man/snownews.*.1)
man/mans	:= $(filter-out ${man/lmans},$(wildcard man/*.1))
man/langs	:= $(subst man/snownews.,,$(man/lmans:.1=))

################ Installation ##########################################
ifdef man1dir
.PHONY:	man/install man/uninstall

mand		:= ${DESTDIR}${mandir}
man1d		:= ${DESTDIR}${man1dir}
lmand		:= $(foreach l,${man/langs},${mand}/${l}/man1)
man/emani	:= $(subst man/,${man1d}/,${man/mans})
man/lmani	:= $(foreach l,${man/langs},${mand}/${l}/man1/snownews.1)

${mand}:
	@echo "Creating $@ ..."
	@${INSTALL} -d $@
${man1d}: | ${mand}
	@echo "Creating $@ ..."
	@${INSTALL} -d $@
${lmand}: | ${mand}
	@echo "Creating $@ ..."
	@${INSTALL} -d $@
man/mani	:= ${man/emani}
${man/emani}:	${man1d}/%:	man/%	| ${man1d}
	@echo "Installing $@ ..."
	@${INSTALL_DATA} $< $@
ifdef localedir
man/mani	+= ${man/lmani}
${man/lmani}:	${mand}/%/man1/snownews.1:	man/snownews.%.1 | ${lmand}
	@echo "Installing $@ ..."
	@${INSTALL_DATA} $< $@
endif

installdirs:	${mand} ${man1d}
install:	man/install
man/install:	${man/mani}

uninstall:	man/uninstall
man/uninstall:
	@if [ -f ${man1d}/${name}.1 ]; then\
	    echo "Uninstalling ${name} man pages ...";\
	    rm -f ${man/mani};\
	fi
endif
