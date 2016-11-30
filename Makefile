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
LDLIBS		:= -L/usr/lib/x86_64-linux-gnu -L/usr/local/lib -Lobj/third-party -Wl,-rpath=./obj/plugins -Wl,-rpath=./obj/third-party
LDFLAGS		:= -lrt -ldl -lpthread -lcurl -ljson-c -lssl -lcrypto -lzlog -lmysqlclient
INC			:= -I$(INCDIR) -I/usr/local/include/ -I$(INCDIR)/zlog  -I$(INCDIR)/json

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
	$(CC) -o $@ $(OBJECTS) $(INC) -Wl,--export-dynamic $(LDLIBS) $(LDFLAGS)

$(OBJDIR)/plugins/lib%.so: $(OBJDIR)/plugins/%.o
	@echo
	@echo "[ Plugin "$*" ]"
	$(CC) -shared -o $@ $< $(LDLIBS) $(LDFLAGS)

$(OBJDIR)/plugins/%.o: $(SRCDIR)/plugins/%.c $(INCDIR)/plugins/%.h
	$(CC) $(CFLAGS) $(INC) -fPIC -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(INCDIR)/%.h
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

clean:
	rm -rf $(BINDIR) $(OBJDIR)/*.o $(OBJDIR)/plugins

.PHONY: all clean
