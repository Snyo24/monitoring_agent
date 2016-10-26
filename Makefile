#Compiler and Linker
CC			:= gcc

#The Target Binary Program
TARGET		:= agent

#The Directories, Source, Includes, Objects, Binary and Resources
SRCDIR		:= src
INCDIR		:= inc
OBJDIR		:= obj
BINDIR		:= bin
LOGDIR		:= log
DOCDIR		:= html

#Flags, Libraries and Includes
CFLAGS		:= -std=gnu99 -Wall -O2 -g
LDLIBS		:= -L/usr/lib/x86_64-linux-gnu -Lobj/plugins
LDFLAGS		:= -lrt -ldl -lpthread -lzlog -lcurl -ljson 
INC			:= -I$(INCDIR)

CORE		:= $(wildcard $(SRCDIR)/*.c)
PLUGINS		:= $(wildcard $(SRCDIR)/plugins/*.c)

OBJECTS		:= $(CORE:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
LDS			:= $(PLUGINS:$(SRCDIR)/plugins/%.c=$(OBJDIR)/plugins/lib%.so)

#Rules
all: directories $(BINDIR)/$(TARGET) $(LDS)
	$(shell sudo cp $(LDS) /usr/lib)

directories:
	@mkdir -p $(BINDIR)
	@mkdir -p $(OBJDIR)/plugins
	@mkdir -p $(LOGDIR)

$(BINDIR)/$(TARGET): $(OBJECTS)
	@echo
	@echo "[ Target ]"
	$(CC) $(OBJECTS) $(INC) -o $@ $(LDLIBS) $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(INCDIR)/%.h
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(OBJDIR)/plugins/lib%.so: $(OBJDIR)/plugins/%.o
	@echo
	@echo "[ Plugin "$*" ]"
	$(CC) -shared -o $@ $< 

$(OBJDIR)/plugins/%.o: $(SRCDIR)/plugins/%.c $(INCDIR)/plugins/%.h
	$(CC) $(CFLAGS) -fPIC $(INC) -c $< -o $@

clean:
	rm -rf *.o $(TARGET) $(OBJDIR) $(BINDIR) $(DOCDIR)

doc:
	@doxygen -s

.PHONY: all clean doc
