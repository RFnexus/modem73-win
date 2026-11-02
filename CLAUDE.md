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

Single-binary C++17 project; the only compilation unit is `kiss_tnc.cc`, everything else is header-only. The file layout mirrors upstream modem73 (`kiss_tnc.cc` is `main()`, `kiss_tnc_impl.hh` holds `KISSTNC`, `tnc_ui_state.hh` the UI/TNC shared state, `tnc_ui.hh` the TUI, `attach_client.hh` the `--attach` client, `net_audio.hh` the `tcp:` audio link, `glyphs.hh` the ASCII/Unicode marker table, `probe.hh` the probe mode ladder) with these Windows replacements:

- Sockets are Winsock2 (`SOCKET`/`INVALID_SOCKET`, `closesocket`, `ioctlsocket` FIONBIO, `WSAPoll` or `select`, `WSAGetLastError`) in `kiss_tnc_impl.hh`, `control_port.hh`, `attach_client.hh`, `net_audio.hh`, `rigctl_ptt.hh` and `hamlib_ptt.cc`. `WSAStartup` runs at the top of `main()`. `SO_EXCLUSIVEADDRUSE` replaces `SO_REUSEADDR` so the port-in-use checks stay reliable.
- `serial_ptt.hh` drives DTR/RTS via `EscapeCommFunction` on `\\.\COMx` handles and enumerates COM ports for the TUI dialog.
- `cm108_ptt.hh` uses the vendored hidapi Windows backend (`deps/hidapi/hid.c`, links `-lsetupapi`). GPIO PTT stays in the sources under `WITH_GPIO_PTT`, which is never defined on Windows.
- `miniaudio_audio.hh`: `--audio-backend` accepts `auto`, `wasapi`, `dsound`, `winmm` (upstream: alsa/pulse/jack).
- `tnc_ui.hh` includes PDCurses `<curses.h>` with `PDC_NCMOUSE`; stderr is redirected to `NUL` while the TUI runs. `A_DIM` is remapped to colour pair 7 because wincon has no dim attribute, so never OR it with another `COLOR_PAIR`. `modal_getch()` and the main loop poll the console viewport (`PDC_get_rows/columns`) to catch resizes conhost does not report. Unicode markers are on when the console code page is 65001 or `WT_SESSION` is set (Windows Terminal), otherwise ASCII; the STYLE tab and `--ascii` override that. `PDC_WIDE`/`PDC_FORCE_UTF8` must be defined identically for the app and the PDCurses objects (the Makefile does this).
- Config/settings/presets/appearance/perf log live in `%APPDATA%\modem73\` (was `~/.config/modem73`).
- Restart (MISC, NETWORK tab, control port `restart`) relaunches the exe with `CreateProcess` using the original command line and waits for the child, since Windows has no in-place `exec`.
- Ctrl+C/close go through a console control handler that unkeys the radio; the Windows port also re-initialises the PTT driver live when PTT settings change (upstream requires a restart).
- `windows.h` defines `IN`/`OUT` macros that clash with aicodix template parameters — `kiss_tnc_impl.hh` `#undef`s them after the network includes; keep that ordering.

Parity with upstream is a three-way merge per file: base = upstream at the last parity commit, ours = this tree, theirs = upstream HEAD. Files with no Windows delta (`csma.hh`, `modem.hh`, `phy/*`, `rx_frame_info.hh`, `probe.hh`, `glyphs.hh`) are copied verbatim.

Key components, threading model, and the operating mode encoding are otherwise identical to upstream modem73.

Threads: main (accept/poll KISS clients), TX loop, RX loop, PTT watchdog, audio supervisor, control port.
