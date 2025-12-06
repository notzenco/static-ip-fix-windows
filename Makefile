# Makefile for static-ip-fix
# Supports:
#   - MinGW-w64 + gcc + GNU make  (default)
#   - MSVC + cl via GNU make       (invoke with: make MSVC=1)

# -------------------------------
# Directories and shared vars
# -------------------------------
SRCDIR  = src
OBJDIR  = obj
BINDIR  = bin

# Binary name
TARGET  = $(BINDIR)/static-ip-fix.exe

SRC     = $(SRCDIR)/main.c

.PHONY: all clean

all: $(TARGET)

# ============================================================
#               MSVC BUILD (must run: make MSVC=1)
# ============================================================
ifdef MSVC
CC      = cl
CFLAGS  = /W4 /DUNICODE /D_UNICODE /O2 /nologo
LDFLAGS = /link iphlpapi.lib advapi32.lib ws2_32.lib

OBJ     = $(OBJDIR)/main.obj

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
# ============================================================
#                 MINGW + GCC (DEFAULT)
# ============================================================
CC      = gcc
CFLAGS  = -Wall -Wextra -DUNICODE -D_UNICODE -O2 -municode
LDFLAGS = -municode -liphlpapi -ladvapi32 -lws2_32

OBJ     = $(OBJDIR)/main.o

$(TARGET): $(OBJ) | $(BINDIR)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

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
