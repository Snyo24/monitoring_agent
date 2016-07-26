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
LIB			:= -L/usr/lib/x86_64-linux-gnu -lcurl -lmysqlclient -lpthread -lz -lm -lrt -ldl -lyaml -lzlog
INC			:= -I$(INCDIR) -I/usr/include/mysql

SOURCES		:= \
			   main.c \
			   aggregator.c \
			   metric.c \
			   mysql_agent.c \
			   util.c \
			   agent.c \
			   snyohash.c
OBJECTS		:= \
			   main.o \
			   aggregator.o \
			   metric.o \
			   mysql_agent.o \
			   util.o \
			   agent.o \
			   snyohash.o
_OBJECTS	:= $(addprefix $(OBJDIR)/,$(_OBJECTS))

vpath %.c $(SRCDIR)
vpath %.c $(SRCDIR)/agents
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

main.o: main.c
agent.o: agent.c agent.h
aggregator.o: aggregator.c aggregator.h
mysql_agent.o: mysql_agent.c mysql_agent.h
metric.o: metric.c metric.h
util.o: util.c util.h
snyohash.o: snyohash.c snyohash.h

.PHONY: all clean