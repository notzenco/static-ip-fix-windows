# Makefile for static-ip-fix
# Supports:
#   - MinGW-w64 + gcc + GNU make  (default)
#   - MSVC + cl via GNU make       (invoke with: make MSVC=1)

# -------------------------------
# Directories
# -------------------------------
SRCDIR  = src
OBJDIR  = obj
BINDIR  = bin

# -------------------------------
# Target
# -------------------------------
TARGET  = $(BINDIR)/static-ip-fix.exe

# -------------------------------
# Auto-detect source files
# -------------------------------
SRCS    = $(wildcard $(SRCDIR)/*.c)

.PHONY: all clean

all: $(TARGET)

# ============================================================
#               MSVC BUILD (make MSVC=1)
# ============================================================
ifdef MSVC

CC      = cl
CFLAGS  = /W4 /DUNICODE /D_UNICODE /O2 /nologo /I$(SRCDIR)
LDFLAGS = /link iphlpapi.lib advapi32.lib ws2_32.lib

OBJS    = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.obj,$(SRCS))

$(TARGET): $(OBJS) | $(BINDIR)
	$(CC) $(CFLAGS) $(OBJS) /Fe:$(TARGET) $(LDFLAGS)

$(OBJDIR)/%.obj: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) /c $< /Fo:$@

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
#               MINGW + GCC (DEFAULT)
# ============================================================

CC      = gcc
CFLAGS  = -Wall -Wextra -DUNICODE -D_UNICODE -O2 -municode -I$(SRCDIR) -MMD -MP
LDFLAGS = -municode -liphlpapi -ladvapi32 -lws2_32

OBJS    = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
DEPS    = $(OBJS:.o=.d)

$(TARGET): $(OBJS) | $(BINDIR)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	@mkdir -p $(OBJDIR) 2>/dev/null || mkdir $(OBJDIR) 2>nul || true

$(BINDIR):
	@mkdir -p $(BINDIR) 2>/dev/null || mkdir $(BINDIR) 2>nul || true

clean:
	rm -rf $(OBJDIR) $(BINDIR) 2>/dev/null || (rmdir /S /Q $(OBJDIR) 2>nul & rmdir /S /Q $(BINDIR) 2>nul) || true

# Include auto-generated dependencies
-include $(DEPS)

endif
