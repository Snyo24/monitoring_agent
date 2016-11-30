#Compiler and Linker
CC          := gcc

#The Target Binary Program
TARGET      := agent

#The Directories, Source, Includes, Objects, Binary and Resources
BINDIR      := bin
SRCDIR      := src
INCDIR      := inc
LIBDIR      := lib
OBJDIR      := obj
LOGDIR      := log
DOCDIR      := html

ifeq ($(MAKECMDGOALS), v)
	V := -DVERBOSE
endif

#Flags, Libraries and Includes
CFLAGS      := -std=gnu99 -fms-extensions -Wall -O2 -g $(V)
LDLIBS      := -L/usr/lib/x86_64-linux-gnu -L/usr/local/lib -L$(LIBDIR) -Wl,-rpath=./lib -Wl,-rpath=./lib/plugins
LDFLAGS     := -lrt -ldl -lpthread -lcurl -ljson-c -lssl -lcrypto -lzlog -lmysqlclient
INC         := -I/usr/local/include/ -I$(INCDIR) -I$(INCDIR)/zlog

CORE        := $(wildcard $(SRCDIR)/*.c)
PLUGINS     := $(wildcard $(SRCDIR)/plugins/*.c)

OBJECTS     := $(CORE:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
LDS         := $(PLUGINS:$(SRCDIR)/plugins/%.c=$(LIBDIR)/plugins/lib%.so)

#Rules
all: dir $(LDS) $(BINDIR)/$(TARGET)

v: all

dir:
	@mkdir -p $(BINDIR)
	@mkdir -p $(LOGDIR)
	@mkdir -p $(OBJDIR)/plugins

$(BINDIR)/$(TARGET): $(OBJECTS)
	@echo
	@echo "[ Target ]"
	$(CC) -o $@ $(OBJECTS) $(INC) -Wl,--export-dynamic $(LDLIBS) $(LDFLAGS)

$(LIBDIR)/plugins/lib%.so: $(OBJDIR)/plugins/%.o
	@echo
	@echo "[ Plugin "$*" ]"
	$(CC) -shared -o $@ $< $(LDLIBS) $(LDFLAGS)

$(OBJDIR)/plugins/%.o: $(SRCDIR)/plugins/%.c $(INCDIR)/plugins/%.h
	$(CC) $(CFLAGS) $(INC) -fPIC -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(INCDIR)/%.h
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

clean:
	rm -rf $(BINDIR) $(OBJDIR) $(LOGDIR)/* $(LDS)

.PHONY: all clean
