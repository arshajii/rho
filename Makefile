TARGET   := rho
CC       := gcc
WARNINGS := -Wall -Wextra -Werror
CFLAGS   := -std=c11 -g -pedantic -pedantic-errors -fstrict-aliasing -pthread $(WARNINGS)
LDLIBS   := -lm -ldl -lpthread
LDFLAGS  :=

SRCDIR := src
OBJDIR := obj

.PHONY: default all clean
.PRECIOUS: $(TARGET) $(OBJECTS)

default: $(TARGET)
all: default

SOURCES := $(wildcard $(SRCDIR)/*.c) \
           $(wildcard $(SRCDIR)/compiler/*.c) \
           $(wildcard $(SRCDIR)/modules/*.c) \
           $(wildcard $(SRCDIR)/runtime/*.c) \
           $(wildcard $(SRCDIR)/types/*.c) \
           $(wildcard $(SRCDIR)/util/*.c)

OBJECTS := $(patsubst %.c, $(OBJDIR)/%.o, $(notdir $(SOURCES)))

HEADERS := $(wildcard $(SRCDIR)/*.h) \
           $(wildcard $(SRCDIR)/compiler/*.h) \
           $(wildcard $(SRCDIR)/modules/*.h) \
           $(wildcard $(SRCDIR)/runtime/*.h) \
           $(wildcard $(SRCDIR)/types/*.h) \
           $(wildcard $(SRCDIR)/util/*.h) \
           $(wildcard $(SRCDIR)/include/*.h)

INCLUDES := -I$(SRCDIR) -I$(SRCDIR)/compiler -I$(SRCDIR)/modules -I$(SRCDIR)/runtime \
            -I$(SRCDIR)/types -I$(SRCDIR)/util -I$(SRCDIR)/include

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/compiler/%.c $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/modules/%.c $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/runtime/%.c $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/types/%.c $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/util/%.c $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) $(LDLIBS) -o $@

clean:
	-rm -f $(OBJDIR)/*.o
