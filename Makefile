#Compiler and Linker
CC			:= gcc

#The Target Binary Program
TARGET		:= test

#The Directories, Source, Includes, Objects, Binary and Resources
SRCDIR		:= src
INCDIR		:= inc
OBJDIR		:= obj
TARGETDIR	:= bin
LOGDIR		:= log
DOCDIR		:= html

#Flags, Libraries and Includes
CFLAGS		:= -Wall -O2 -g
LIB			:= -L/usr/lib/x86_64-linux-gnu -lpthread -lz -lm -lrt -ldl -lyaml -lzlog -lcurl -lmysqlclient
INC			:= -I$(INCDIR) -I/usr/include/mysql

CORE		:= $(wildcard $(SRCDIR)/*.c)
AGENTS		:= $(wildcard $(SRCDIR)/agents/*.c)

SOURCES		:= $(CORE) $(AGENTS)
OBJECTS		:= $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

all: directories $(TARGET)

directories:
	@mkdir -p $(TARGETDIR)
	@mkdir -p $(OBJDIR)/agents
	@mkdir -p $(LOGDIR)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LIB)

$(OBJDIR)/%.o : $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(INC) -c $< -o $@  

clean:
	rm -rf *.o $(TARGET) $(OBJDIR) $(TARGETDIR) $(DOCDIR) $(LOGDIR)

doc:
	@doxygen -s

.PHONY: all clean doc