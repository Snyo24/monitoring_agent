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
CFLAGS		:= -std=gnu99 -fms-extensions -Wall -O2 -g $(V)
LDLIBS		:= -L/usr/lib/x86_64-linux-gnu -L/usr/local/lib
LDFLAGS		:= -lrt -ldl -lpthread -lzlog -lcurl -ljson -lssl -lcrypto -lmysqlclient
INC			:= -I$(INCDIR) -I/usr/local/include/

CORE		:= $(wildcard $(SRCDIR)/*.c)
PLUGINS		:= $(wildcard $(SRCDIR)/plugins/*.c)

OBJECTS		:= $(CORE:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
LDS			:= $(PLUGINS:$(SRCDIR)/plugins/%.c=$(OBJDIR)/plugins/lib%.so)

#Rules
all: directories $(BINDIR)/$(TARGET) $(LDS)

directories:
	@mkdir -p $(BINDIR)
	@mkdir -p $(OBJDIR)/plugins
	@mkdir -p $(LOGDIR)

$(BINDIR)/$(TARGET): $(OBJECTS)
	@echo
	@echo "[ Target ]"
	$(CC) -o $@ $(OBJECTS) $(INC) -Wl,--export-dynamic -Wl,-rpath,$(shell pwd)/obj/plugins $(LDLIBS) $(LDFLAGS)

$(OBJDIR)/plugins/lib%.so: $(OBJDIR)/plugins/%.o
	@echo
	@echo "[ Plugin "$*" ]"
	$(CC) -shared -o $@ $< $(LDFLAGS)

$(OBJDIR)/plugins/%.o: $(SRCDIR)/plugins/%.c $(INCDIR)/plugins/%.h
	$(CC) $(CFLAGS) $(INC) -fPIC -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(INCDIR)/%.h
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

clean:
	rm -rf $(TARGET) $(OBJDIR) $(BINDIR) $(DOCDIR)

doc:
	@doxygen -s

.PHONY: all clean doc
