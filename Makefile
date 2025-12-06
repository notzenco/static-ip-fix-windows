# Makefile for dnsconfig_safe
# Supports MSVC (nmake) and MinGW-w64 (make)

# Directories
SRCDIR = src
OBJDIR = obj
BINDIR = bin

# Output binary name
TARGET = $(BINDIR)/dnsconfig_safe.exe
SRC = $(SRCDIR)/main.c

# Detect compiler
ifdef MSVC
# MSVC build (use with: nmake MSVC=1)
CC = cl
CFLAGS = /W4 /DUNICODE /D_UNICODE /O2 /nologo
LDFLAGS = /link iphlpapi.lib advapi32.lib
OBJ = $(OBJDIR)/main.obj

$(TARGET): $(OBJ) | $(BINDIR)
	$(CC) $(CFLAGS) $(OBJ) /Fe:$(TARGET) $(LDFLAGS)

$(OBJ): $(SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) /c $(SRC) /Fo:$(OBJ)

$(OBJDIR):
	-mkdir $(OBJDIR)

$(BINDIR):
	-mkdir $(BINDIR)

clean:
	-del /Q $(OBJDIR)\*.obj 2>nul
	-del /Q $(BINDIR)\*.exe $(BINDIR)\*.pdb $(BINDIR)\*.ilk 2>nul
	-rmdir $(OBJDIR) 2>nul
	-rmdir $(BINDIR) 2>nul

else
# MinGW-w64 build (default for make)
CC = gcc
CFLAGS = -Wall -Wextra -DUNICODE -D_UNICODE -O2 -municode
LDFLAGS = -liphlpapi -ladvapi32
OBJ = $(OBJDIR)/main.o

$(TARGET): $(OBJ) | $(BINDIR)
	$(CC) -municode $(OBJ) -o $(TARGET) $(LDFLAGS)

$(OBJ): $(SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $(SRC) -o $(OBJ)

$(OBJDIR):
	-mkdir $(OBJDIR) 2>nul || true

$(BINDIR):
	-mkdir $(BINDIR) 2>nul || true

clean:
	-rmdir /S /Q $(OBJDIR) 2>nul || true
	-rmdir /S /Q $(BINDIR) 2>nul || true

endif

.PHONY: clean all

all: $(TARGET)
