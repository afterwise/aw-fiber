
PROGRAMS += FiberTest

FIBERTESTLIBS =

FiberTest.%: fibertest/libfibertest.%$(LIBSUF) $(patsubst %, extern/%$(EXESUF)$(LIBSUF), $(FIBERTESTLIBS))
	$(link)

