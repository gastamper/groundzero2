CC              = gcc
PROF            =
NOCRYPT         =
C_FLAGS         = -O -g -Wall -std=gnu99 $(PROF) $(NOCRYPT)
L_FLAGS         = -lz -lm -lcares
L_FLAG_CRYPT    = -lcrypt
EXECUTABLE_NAME = ground0

OBJDIR  = .obj

SRC     = act_comm.c act_obj.c db.c interp.c social-edit.c \
	  act_info.c act_team.c act_wiz.c fight.c memory.c \
	  update.c act_move.c comm.c handler.c save.c string.c \
	  mccp.c net.c utils.c poll.c rank.c mob_vehicle.c \
	  act_use.c act_combat.c reboot.c telnet.c inline.c dns.c \
	  top.c behave.c ban.c act_profile.c graph.c pfile.c imp_table.c
O_FILES = $(foreach f,$(SRC:.c=.o),$(OBJDIR)/$(f))
D_FILES = $(foreach f,$(SRC:.c=.d),$(OBJDIR)/$(f))

default: ground0
-include $(D_FILES)

osx: $(O_FILES)
	@echo Creating executable $(EXECUTABLE_NAME) \(without -lcrypt\) ...
	@$(CC) -o $(EXECUTABLE_NAME) $(O_FILES) $(L_FLAGS)
	@chmod g=rwx $(EXECUTABLE_NAME)
	@chmod -R g=rw $(OBJDIR) *.[ch]
	@echo "DONE."

ground0: $(O_FILES)
	@echo Creating executable $(EXECUTABLE_NAME) ...
	@$(CC) -o $(EXECUTABLE_NAME) $(O_FILES) $(L_FLAGS) $(L_FLAG_CRYPT)
	@chmod g=rwx $(EXECUTABLE_NAME)
	@chmod -R g=rw $(OBJDIR) *.[ch]
	@echo "DONE."

clean:
	rm -f ground0
	rm -f $(OBJDIR)/*
	rm -f *~

$(OBJDIR)/%.d: %.c
	@$(CC) -MM -MT '$$(OBJDIR)/$(*F).o' $< > $@
$(OBJDIR)/%.o: %.c
	@echo Compiling $< ...
	@$(CC) -c $(C_FLAGS) $< -o $@
