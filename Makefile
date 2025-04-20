ECHO	= echo
DIRS	= protocols sim
EXE	= sim_trace
OBJS	= 
OBJLIBS	= lib/libprotocols.a lib/libsim.a 
LIBS	= -Llib/ -lsim -lprotocols
TARBALL = $(if $(USER),$(USER),gburdell3)-proj3.tar.gz

all : $(EXE)

$(EXE) : $(OBJLIBS)
	g++ -o $(EXE) $(OBJS) $(LIBS)

lib/libprotocols.a : force_look
	cd protocols; $(MAKE) $(MFLAGS)

lib/libsim.a : force_look
	cd sim; $(MAKE) $(MFLAGS)

clean :
	$(ECHO) cleaning up in .
	-$(RM) -f $(EXE) $(OBJS) $(OBJLIBS)
	-for d in $(DIRS); do (cd $$d; $(MAKE) clean ); done

force_look :
	true

validate: $(EXE)
	@bash validate.sh

submit: clean
	tar --exclude=Project3_v1_0_1.pdf -czhvf $(TARBALL) run.sh Makefile protocols sim $(wildcard *.pdf) 
	@echo
	@echo 'submission tarball written to' $(TARBALL)
	@echo 'please decompress it yourself and make sure it looks right!'