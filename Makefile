-include Config.mk

################ Source files ##########################################

exe	:= $O${name}
srcs	:= $(wildcard *.c)
objs	:= $(addprefix $O,$(srcs:.c=.o))
deps	:= ${objs:.o=.d}
confs	:= Config.mk config.h
oname   := $(notdir $(abspath $O))

################ Compilation ###########################################

.SUFFIXES:
.PHONY:	all run

all:	${exe}

run:	${exe}
	@${exe}

${exe}:	${objs}
	@echo "Linking $@ ..."
	@${CC} ${ldflags} -o $@ $^ ${libs}

$O%.o:	%.c
	@echo "    Compiling $< ..."
	@${CC} ${cflags} -MMD -MT "$(<:.c=.s) $@" -o $@ -c $<

%.s:	%.c
	@echo "    Compiling $< to assembly ..."
	@${CC} ${cflags} -S -o $@ -c $<

include man/Module.mk
include po/Module.mk

################ Installation ##########################################

ifdef bindir
.PHONY:	install installdirs uninstall uninstall-bin

exed	:= ${DESTDIR}${bindir}
exei	:= ${exed}/$(notdir ${exe})

${exed}:
	@echo "Creating $@ ..."
	@${INSTALL} -d $@
${exei}:	${exe} | ${exed}
	@echo "Installing $@ ..."
	@${INSTALL_PROGRAM} $< $@

installdirs:	${exed}
install:	${exei}
uninstall:	uninstall-bin
uninstall-bin:
	@if [ -f ${exei} ]; then\
	    echo "Removing ${exei} ...";\
	    rm -f ${exei};\
	fi
endif

################ Maintenance ###########################################

.PHONY:	clean distclean maintainer-clean

clean:
	@if [ -d ${builddir} ]; then\
	    rm -f ${exe} ${objs} ${deps} $O.d;\
	    rmdir ${builddir};\
	fi

distclean:	clean
	@rm -f ${oname} ${confs} config.status

maintainer-clean: distclean

$O.d:	${builddir}/.d
	@[ -h ${oname} ] || ln -sf ${builddir} ${oname}
$O%/.d:	$O.d
	@[ -d $(dir $@) ] || mkdir $(dir $@)
	@touch $@
${builddir}/.d:	Makefile
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	@touch $@

Config.mk:	Config.mk.in
config.h:	config.h.in | Config.mk
${objs}:	Makefile ${confs} | $O.d
${confs}:	configure
	@if [ -x config.status ]; then echo "Reconfiguring ...";\
	    ./config.status;\
	else echo "Running configure ...";\
	    ./configure;\
	fi

-include ${deps}
