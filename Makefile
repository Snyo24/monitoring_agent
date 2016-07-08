.SUFFIXES: .c .o

#Compiler and Linker
CC			:= gcc

#The Target Binary Program
TARGET		:= test

#The Directories, Source, Includes, Objects, Binary and Resources
SRCDIR		:= src
INCDIR		:= inc
OBJDIR		:= obj
TARGETDIR	:= bin

#Flags, Libraries and Includes
CFLAGS		:= -Wall -O2 -g
LIB			:= -L/usr/lib/x86_64-linux-gnu -ljson -lcurl -lmysqlclient -lpthread -lz -lm -lrt -ldl
INC			:= -I$(INCDIR) -I/usr/include

SOURCES		:= \
			   agent.c \
			   collector.c \
			   mysql.c \
			   util.c
OBJECTS		:= \
			   agent.o \
			   collector.o \
			   mysql.o \
			   util.o
_OBJECTS	:= $(addprefix $(OBJDIR)/,$(_OBJECTS))

vpath %.c $(SRCDIR)
vpath %.c $(SRCDIR)/subCollector
vpath %.h $(INCDIR)

all: directories $(TARGET)

directories:
	@mkdir -p $(OBJDIR)
	@mkdir -p $(TARGETDIR)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LIB)

.c.o:
	$(CC) $(CFLAGS) $(INC) -c -o $@ $< 

clean:
	rm -rf *.o $(TARGET) $(OBJDIR) $(TARGETDIR)

agent.o: agent.c collector.h collector.c
collector.o: collector.c collector.h
mysql.o: mysql.c mysql.h
util.o: util.c util.h

.PHONY: all clean