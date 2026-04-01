include Makefile.default.mak

ifeq ($(OS),Windows_NT)

$(error Source distributions are not intended for use on Windows)

endif

##########################################################################
##########################################################################

.PHONY:configure
configure:
	$(_V)$(MAKE) init_parallel

##########################################################################
##########################################################################

.PHONY:build
build:
	$(if $(WITH_DEBUGGER),$(_V)cd build/r.$(OS) && ninja)
	$(if $(WITH_DEBUGGER),$(if $(RUN_TESTS),$(_V)cd build/r.$(OS) && ctest -j))
	$(if $(WITHOUT_DEBUGGER),$(_V)cd build/f.$(OS) && ninja)
	$(if $(WITHOUT_DEBUGGER),$(if $(RUN_TESTS),$(_V)cd build/f.$(OS) && ctest -j))

##########################################################################
##########################################################################

ifneq ($(UNAME),Darwin)

# TODOOO - test for evidence of any builds, and barf if none. Copy any
# available, then the assets too.

.PHONY:install
install:
	echo hellooo

endif

# install:PREFIX=$(error Must specify PREFIX)
# install:_
# install:
# 	test -f "build/r.$(OS)
# 	mkdir -p "$(PREFIX)/bin"
# 	mkdir -p "$(PREFIX)/share/b2"
# 	cp -Rv "build/f.$(OS)/src/b2/assets/*" "$(PREFIX)/share/b2/"
