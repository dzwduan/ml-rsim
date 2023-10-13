############################################################################
# make remake    generate the binary using avaiable libraries
# make rebuild   regenerate every library and target file
# make bintree	 create necessary directories to store object/binary files
# make all       make depend + make rebuild
# make clean     delete only the binary file (lrsim)
# make clobber   delete the object files in all subdirectories
#
# use "make all" at the first time, 
# use "make rebuild" if you want to recompile everything
############################################################################

include Makefile.defs

BINDIR  = $(OSTYPE)
SUBDIRS = sim_main Processor Caches IO Bus Memory DRAM
LIBSAUX = libsim.a libcache.a libprocessor.a libio.a libbus.a libmemory.a libdram.a
LIBS    = $(addprefix $(OSTYPE)/,$(LIBSAUX))
DLIBS   = $(LIBS:.o=_d.o)
DLIBS  := $(DLIBS:.a=_d.a)
PROGRAM = mlrsim
TARGET  = $(OSTYPE)/$(PROGRAM)

default: links testbin
all: links bintree rebuild

links: $(PROGRAM) $(PROGRAM)_d ../lamix/machine

../lamix/machine:
	-ln -s ../lamix/arch/lamix_ws/lamix_ws ../lamix/machine

$(PROGRAM):
	-ln -s wrapper $(PROGRAM)

$(PROGRAM)_d:
	-ln -s wrapper $(PROGRAM)_d

bintree:
	-for i in ${SUBDIRS}; do (	\
	    cd ../src/$$i; 		\
	    -mkdir -p $(OBJDIR);	\
	    -touch $(OBJDIR)/depend; 	\
	    $(MAKE) depend); 		\
         done

rebuild: installlibs testbin

installlibs: 
	-for i in ${SUBDIRS}; do (cd ../src/$$i; $(MAKE) install); done
remake:
	-cd $(OSTYPE);
	$(C++) $(C++FLAGS) -o $(TARGET) $(LIBS) $(DEPLIBS)
	$(C++) $(C++DFLAGS) -o $(TARGET)_d $(DLIBS) $(DEPLIBS)

SRCDIR   = ../src
TTARGET  = $(OSTYPE)/$(PROGRAM)
DTARGET  = $(OSTYPE)/$(PROGRAM)_d
TESTLIBS = $(SRCDIR)/sim_main/$(OBJDIR)/libsim.a        \
           $(SRCDIR)/Caches/$(OBJDIR)/libcache.a        \
	   $(SRCDIR)/Processor/$(OBJDIR)/libprocessor.a \
	   $(SRCDIR)/IO/$(OBJDIR)/libio.a               \
	   $(SRCDIR)/Bus/$(OBJDIR)/libbus.a             \
	   $(SRCDIR)/Memory/$(OBJDIR)/libmemory.a       \
	   $(SRCDIR)/DRAM/$(OBJDIR)/libdram.a
DTLIBS   = $(TESTLIBS:.o=_d.o)
DTLIBS  := $(DTLIBS:.a=_d.a)

debug: $(DTARGET)
testbin: $(TTARGET) $(DTARGET)


$(TTARGET): $(TESTLIBS)
	-mkdir $(OSTYPE) > /dev/null
	$(LD) $(LDFLAGS) -o $@ $(TESTLIBS) $(TESTLIBS) $(SRCDIR)/Processor/$(OBJDIR)/libprocessor.a $(SRCDIR)/sim_main/$(OBJDIR)/libsim.a $(DEPLIBS)

$(DTARGET): $(DTLIBS)
	-mkdir $(OSTYPE) > /dev/null
	$(LD) $(LDFLAGS) -o $@ $(DTLIBS) $(DTLIBS) $(SRCDIR)/Processor/$(OBJDIR)/libprocessor_d.a $(SRCDIR)/sim_main/$(OBJDIR)/libsim_d.a $(DEPLIBS)


.PHONY: $(TESTLIBS) $(DTLIBS)
$(SRCDIR)/sim_main/$(OBJDIR)/libsim.a:
	cd ../src/sim_main; $(MAKE)
$(SRCDIR)/sim_main/$(OBJDIR)/libsim_d.a:
	cd ../src/sim_main; $(MAKE) debug
$(SRCDIR)/Caches/$(OBJDIR)/libcache.a:
	cd ../src/Caches; $(MAKE)
$(SRCDIR)/Caches/$(OBJDIR)/libcache_d.a:
	cd ../src/Caches; $(MAKE) debug
$(SRCDIR)/Processor/$(OBJDIR)/libprocessor.a:
	cd ../src/Processor; $(MAKE) 
$(SRCDIR)/Processor/$(OBJDIR)/libprocessor_d.a:
	cd ../src/Processor; $(MAKE) debug
$(SRCDIR)/IO/$(OBJDIR)/libio.a:
	cd ../src/IO; $(MAKE)
$(SRCDIR)/IO/$(OBJDIR)/libio_d.a:
	cd ../src/IO; $(MAKE) debug
$(SRCDIR)/Bus/$(OBJDIR)/libbus.a:
	cd ../src/Bus; $(MAKE) 
$(SRCDIR)/Bus/$(OBJDIR)/libbus_d.a:
	cd ../src/Bus; $(MAKE) debug
$(SRCDIR)/Memory/$(OBJDIR)/libmemory.a:
	cd ../src/Memory; $(MAKE)
$(SRCDIR)/Memory/$(OBJDIR)/libmemory_d.a:
	cd ../src/Memory; $(MAKE) debug
$(SRCDIR)/DRAM/$(OBJDIR)/libdram.a:
	cd ../src/DRAM; $(MAKE) 
$(SRCDIR)/DRAM/$(OBJDIR)/libdram_d.a:
	cd ../src/DRAM; $(MAKE) debug

clean:
	-rm -f $(TTARGET) $(DTARGET)

clobber: clean
	-for i in ${SUBDIRS}; do (cd ../src/$$i; $(MAKE) clobber); done
