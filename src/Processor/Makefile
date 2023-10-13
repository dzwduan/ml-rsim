include ../../bin/Makefile.defs

LIBRARY = libprocessor.a
OBJECT  = 
ifeq	($(MACHINE),linux)
	EXTRA_SRCS = linuxfp.c
else
	EXTRA_SRCS =
endif
SRCS    = userstat.cc active.cc branchpred.cc capconf.cc config.cc	\
	  except.cc exec.cc execfuncs.cc funcs.cc instrnames.cc 	\
	  mainsim.cc memprocess.cc procstate.cc startup.cc tlb.cc 	\
	  traps.cc memunit.cc funcunits.cc signalhandler.cc		\
	  mem_debug.cc pagetable.cc fsr.cc predecode_instr.cc		\
	  predecode_table.cc filedesc.cc multiprocessor.cc lock.s	\
	  $(EXTRA_SRCS)

include ../../bin/Makefile.rules
