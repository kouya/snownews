################ Source files ##########################################

po/srcs	:= $(filter-out po/messages.po,$(wildcard po/*.po))
po/objs	:= $(addprefix $O,$(po/srcs:.po=.mo))
locales	:= $(notdir $(po/srcs:.po=))

################ Installation ##########################################

ifdef localedir
.PHONY:	po/all po/install po/uninstall po/clean

all:	po/all
po/all:	${po/objs}

pod		:= ${DESTDIR}${localedir}
polds		:= $(foreach l,${locales},${pod}/${l}/LC_MESSAGES)
po/objsi	:= $(foreach l,${polds},${l}/${name}.mo)

${pod}:
	@echo "Creating $@ ..."
	@${INSTALL} -d $@
${polds}: | ${pod}
	@echo "Creating $@ ..."
	@${INSTALL} -d $@
${po/objsi}:	${pod}/%/LC_MESSAGES/${name}.mo:	$Opo/%.mo | ${polds}
	@echo "Installing $@ ..."
	@${INSTALL_DATA} $< $@

installdirs:	${polds}
install:	po/install
po/install:	${po/objsi}

uninstall:	po/uninstall
po/uninstall:
	@if [ -f ${po/pod}/de/${po/suffix} ]; then\
	    echo "Uninstalling ${name} locales ...";\
	    rm -f ${po/objsi};\
	fi

$O%.mo:	%.po
	@echo "    Compiling $< ..."
	@${MSGFMT} -o $@ $<
endif

################ Maintenance ###########################################

clean:	po/clean
po/clean:
	@if [ -d ${builddir}/po ]; then\
	    rm -f ${po/objs} $Opo/.d;\
	    rmdir ${builddir}/po;\
	fi

${po/objs}: Makefile po/Module.mk ${confs} | $Opo/.d
