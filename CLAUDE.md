# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What is MODEM73-WIN

Windows port of MODEM73, a KISS TNC frontend for the aicodix OFDM modem. It turns a soundcard into a packet radio modem, accepting KISS frames over TCP (default port 8001) and encoding/decoding them as OFDM audio via miniaudio. Builds a single statically linked `modem73.exe`; the TUI uses PDCurses (wincon, wide/UTF-8).

## Build

Everything is vendored under `deps/` (aicodix dsp/code/modem, PDCurses 3.9, hidapi 0.10.1, miniaudio, cJSON) — no external dependencies.

```bash
# Cross-compile from Linux (needs g++-mingw-w64-x86-64-posix or llvm-mingw on PATH)
make

# Native on Windows in an MSYS2 MinGW64 shell
make CXX=g++ CC=gcc

# Debug build
make debug

# Clean
make clean
```

There is no test suite or linter at present.

## Architecture

Single-binary C++17 project; the only compilation unit is `kiss_tnc.cc`, everything else is header-only. Same layout as upstream modem73 with these Windows replacements:

- Sockets are Winsock2 (`SOCKET`/`INVALID_SOCKET`, `closesocket`, `ioctlsocket` FIONBIO, `WSAPoll`, `WSAGetLastError`). `WSAStartup` runs at the top of `main()`. `SO_EXCLUSIVEADDRUSE` replaces `SO_REUSEADDR` so the port-in-use checks stay reliable.
- `serial_ptt.hh` drives DTR/RTS via `EscapeCommFunction` on `\\.\COMx` handles.
- `cm108_ptt.hh` uses the vendored hidapi Windows backend (`deps/hidapi/hid.c`, links `-lsetupapi`).
- `tnc_ui.hh` includes PDCurses `<curses.h>` with `PDC_NCMOUSE`; stderr is redirected to `NUL` while the TUI runs. `PDC_WIDE`/`PDC_FORCE_UTF8` must be defined identically for the app and the PDCurses objects (the Makefile does this).
- Config/settings/perf log live in `%APPDATA%\modem73\` (was `~/.config/modem73`).
- `windows.h` defines `IN`/`OUT` macros that clash with aicodix template parameters — `kiss_tnc.cc` `#undef`s them after the network includes; keep that ordering.

Key components, threading model, and the operating mode encoding are otherwise identical to upstream modem73:

- `kiss_tnc.cc`/`kiss_tnc.hh` — `KISSTNC` (TCP server, KISS handling, TX/RX loops, CSMA), `KISSParser`, `PacketQueue`, `Fragmenter`/`Reassembler`, length-prefix framing.
- `modem.hh` + `phy/` — OFDM/MFSK/ROBUST encoders and decoders at 48kHz.
- `miniaudio_audio.hh` — audio I/O (WASAPI via miniaudio), level measurement for CSMA.
- `tnc_ui.hh` — PDCurses TUI (`WITH_UI`, default on; `--headless` disables at runtime).
- `control_port.hh` — JSON control port on TCP 8073 (length-prefixed, cJSON).
- PTT drivers: rigctl (TCP), serial DTR/RTS, CM108 HID GPIO (`WITH_CM108`, always on), VOX tone, dummy.

Threads: main (accept/poll KISS clients), TX loop, RX loop, PTT watchdog, control port.
