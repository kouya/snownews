################ Source files ##########################################

po/SRCS	:= $(filter-out po/messages.po,$(wildcard po/*.po))
po/OBJS	:= $(addprefix $O,$(po/SRCS:.po=.mo))
LOCALES	:= $(notdir $(po/SRCS:.po=))

################ Installation ##########################################
ifdef LOCALEPATH
.PHONY:	po/install po/uninstall po/clean

po/IPREFIX	:= ${PKGDIR}${LOCALEPATH}/
po/ISUFFIX	:= /LC_MESSAGES/${NAME}.mo
po/OBJSI	:= $(foreach l,${LOCALES},${po/IPREFIX}${l}${po/ISUFFIX})

${po/OBJSI}:	${po/IPREFIX}%${po/ISUFFIX}:	po/%.po
	@echo "Installing $@ ..."
	@${MSGFMT} -o $Opo/$*.mo $<
	@${INSTALLDATA} $Opo/$*.mo $@

install:	po/install
po/install:	${po/OBJSI}

uninstall:	po/uninstall
po/uninstall:
	@if [ -f ${po/IPREFIX}de${po/ISUFFIX} ]; then\
	    echo "Uninstalling ${NAME} locales ...";\
	    rm -f ${po/OBJSI};\
	fi
endif
################ Maintenance ###########################################

clean:	po/clean
po/clean:
	@if [ -d $Opo ]; then\
	    rm -f ${po/OBJS} $Opo/.d;\
	    rmdir ${BUILDDIR}/po;\
	fi

${po/OBJSI}: |Makefile po/Module.mk ${CONFS} $Opo/.d
