# Cross-compile from Linux with mingw-w64 (default), or build natively
# on Windows under MSYS2 with: make CXX=g++ CC=gcc
CXX = x86_64-w64-mingw32-g++
CC = x86_64-w64-mingw32-gcc
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra
LDFLAGS = -static -lws2_32 -lsetupapi

# dependencies
AICODIX_DSP ?= deps/aicodix/dsp
AICODIX_CODE ?= deps/aicodix/code
MODEM_SRC ?= deps/aicodix/modem
PDCURSES ?= deps/pdcurses

INCLUDES = -I$(AICODIX_DSP) -I$(AICODIX_CODE) -I$(MODEM_SRC) -I$(PDCURSES)

TARGET = modem73.exe

SRCS = kiss_tnc.cc
HDRS = kiss_tnc.hh csma.hh miniaudio_audio.hh rigctl_ptt.hh serial_ptt.hh cm108_ptt.hh modem.hh phy/mfsk_modem.hh phy/robust_modem.hh phy/common.hh tnc_ui.hh control_port.hh

PDC_FLAGS = -DPDC_WIDE -DPDC_FORCE_UTF8
PDC_SRCS = $(wildcard $(PDCURSES)/pdcurses/*.c) $(wildcard $(PDCURSES)/wincon/*.c)
PDC_OBJS = $(PDC_SRCS:.c=.o)

OBJS = deps/miniaudio.o deps/cJSON.o deps/hidapi/hid.o $(PDC_OBJS)

# defualt to build with UI, headless operations through --headless
UI_FLAGS = -DWITH_UI

# CM108 PTT via vendored hidapi (always available on Windows)
CM108_FLAGS = -DWITH_CM108

.PHONY: all clean debug help

all: $(TARGET)

deps/miniaudio.o: deps/miniaudio.c deps/miniaudio.h
	$(CC) -c -O2 -o $@ deps/miniaudio.c

deps/cJSON.o: deps/cJSON.c deps/cJSON.h
	$(CC) -c -O2 -o $@ deps/cJSON.c

deps/hidapi/hid.o: deps/hidapi/hid.c deps/hidapi/hidapi.h
	$(CC) -c -O2 -Ideps/hidapi -o $@ deps/hidapi/hid.c

$(PDCURSES)/%.o: $(PDCURSES)/%.c
	$(CC) -c -O2 $(PDC_FLAGS) -I$(PDCURSES) -o $@ $<

$(TARGET): $(SRCS) $(HDRS) $(OBJS)
	$(CXX) $(CXXFLAGS) $(UI_FLAGS) $(CM108_FLAGS) $(PDC_FLAGS) $(INCLUDES) -o $@ $(SRCS) $(OBJS) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)

# Debug build
debug: CXXFLAGS = -std=c++17 -g -O0 -Wall -Wextra -DDEBUG
debug: $(TARGET)

# Help
help:
	@echo "MODEM73 makefile (Windows port)"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build modem73.exe"
	@echo "  clean    - Remove build"
	@echo "  debug    - Build with debug symbols"
	@echo ""
	@echo "Variables:"
	@echo "  CXX/CC       - Toolchain (default: x86_64-w64-mingw32-g++/gcc)"
	@echo "  AICODIX_DSP  - Path to aicodix/dsp (default: deps/aicodix/dsp)"
	@echo "  AICODIX_CODE - Path to aicodix/code (default: deps/aicodix/code)"
	@echo "  MODEM_SRC    - Path to modem source (default: deps/aicodix/modem)"
	@echo ""
	@echo "Native build in an MSYS2 MinGW64 shell:"
	@echo "  make CXX=g++ CC=gcc"
	@echo ""
	@echo "Runtime options:"
	@echo "  modem73.exe            # Run with UI"
	@echo "  modem73.exe -h         # Run headless"
	@echo "  modem73.exe --headless"
