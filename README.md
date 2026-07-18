
<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://i.ibb.co/LDNR23jg/MODEM73-white.png">
    <source media="(prefers-color-scheme: light)" srcset="https://i.ibb.co/wZKznzrF/MODEM73-blk.png">
    <img alt="MODEM73" src="https://i.ibb.co/wZKznzrF/MODEM73-blk.png">
  </picture>
</p>


MODEM73 is an open source software modem that works with any HF, VHF, or UHF radio capable of 2400 Hz of bandwidth. All you need is a sound card and audio cable for your radio.


### NOTE:

This fork of the [modem73](https://github.com/RFnexus/modem73) upstream uses LLM assisted tooling for porting modem73 to Win32 exclusive APIs and PDcurses.  As such this repo is  experimental. For Windows users it's recommended to use the upstream modem73 with [WSL](https://learn.microsoft.com/en-us/windows/wsl/install) for stability, or switch to an operating system that [respects your privacy and autonomy as a human](https://www.youtube.com/watch?v=n8vmXvoVjZw). 

Did you know modem73 can run comfortably on a old laptop flashed with Linux Mint or something like a Raspberry Pi 4 at 30% CPU?

![Screenshot](https://i.ibb.co/4ZhhvcQs/Peek-2026-01-01-10-41.gif)
<p>
<img width="546" height="423" alt="image" src="https://github.com/user-attachments/assets/7180ab80-4386-4ee1-8029-42ca5300ef13" />
<img width="276.5" height="199" alt="image" src="https://github.com/user-attachments/assets/5ac2a8bd-75a1-48a4-8264-74a851a06767" />
</p>



SSB, AM, and FM are all supported. It's plug and play compatible with any KISS application and works with rigctl, CM108 sound devices, and serial PTT out of the box.

There are three modem families each suited for covering any possible RF setup from clean line of sight FM links to poor HF band conditions. The receiver decodes all of them at the same time, so one station can hear anything another station sends without switching modes.

**OFDM**, based on the open source [COFDMTV modem](https://github.com/aicodix/modem) developed by Ahmet Inan / [aicodix GmbH](https://www.aicodix.de/). Modulations from BPSK to QAM4096 with code rates from 1/4 to 5/6 and payloads from 256 to 6144 bytes per frame. Rates run from about 790 bits per second to over 13 kilobits per second in the same 2400 Hz. 

**ROBUST**, a set of slow modes built for fading HF paths such as 40 and 80 meter NVIS.  Five modes from 1150 bps down to 149 bps in either 2400 Hz or a 600 Hz narrow variant

**MFSK**, a non-coherent mode for weak signal propagation and backup links. 

### Features

- KISS over TCP so it works with anything that speaks KISS: APRS clients, HamIRC, packet BBS software, Reticulum, custom applications
- JSON control port API for status and configuration for writing your own progra,ms
- lightweight UI that runs straight from the terminal and headless mode for embedded use 

## Installation

Want to try out a demo of modem73 directly from your browser? Try it out here:
https://rfnexus.github.io/modem73-wasm-demo/
Hit "Start microphone" to begin decoding

## Building from source

Everything is vendored under `deps/` (aicodix DSP, PDCurses, hidapi, miniaudio, cJSON) and the exe is statically linked, so no external libraries are needed at build or run time. CM108 USB PTT is always enabled.

### Native on Windows (MSYS2)

Install [MSYS2](https://www.msys2.org/), then in a MinGW64 shell:
```
pacman -S --needed make mingw-w64-x86_64-gcc
make CXX=g++ CC=gcc
```

### Cross-compile from Linux

```
# Debian/Ubuntu
sudo apt install make g++-mingw-w64-x86-64-posix
make
```

and run `modem73.exe`

## Running & Operations

By default, MODEM73 will listen on port 8001

All of the modes provided by the OFDM modem require a bandwidth of 2400 Hz and work over both FM and SSB. 

There are currently five PTT options:
- NONE (speaker/mic over the air)
- Rigctl
- VOX
- Serial
- CM108


```
# Start in UI mode
modem73.exe

# Start in headless mode
modem73.exe --headless

# See all options with:
modem73.exe --help
```

### PTT options 

```
# Connect to rigctld for PTT control
modem73.exe --rigctl localhost:4532
```

while running `rigctld`


```
modem73.exe --ptt vox --vox-freq 1200 --vox-lead 500 --vox-tail 150
# 500ms vox lead and 150ms vox tail
```


```
modem73.exe --ptt com --com-port COM4 --com-line rts
```

```
# CM108 USB audio interface PTT (GPIO3 is the default)
modem73.exe --ptt cm108 --cm108-gpio 3
```

### Control port

A control port for modem73 will automatically start on port `8073` by default. View `CONTROL_PORT.md` for the full JSON spec

## Usage
<img width="1092" height="847" alt="image" src="https://github.com/user-attachments/assets/7180ab80-4386-4ee1-8029-42ca5300ef13" />

### All In One Audio Cable (AIOC)
modem73 supports the [AIOC](https://github.com/skuep/AIOC) out of the box. To use the All In One Audio cable, set PTT to COM, specify your COM port, and set PTT line to `BOTH` and Invert to `INVERT RTS`. Check Device Manager for the COM number (e.g. COM5). Note that it may change after a device restart, plugging it back in, etc.

### rigctl
modem73 supports Hamlib and rigctl for any rigctl supported radio for PTT. Set rigctl to your options and run `rigctld -m (your model) -s (serial baud rate) -r COMx`  The `d` at the end of `rigctl` tells rigctl to run in network mode, which is what modem73 will connect to.

### Reticulum
Want to use modem73 with Reticulum? Check out the modem73interface
https://github.com/RFnexus/modem73interface

Drop it into `%USERPROFILE%\.reticulum\interfaces\` and in your Reticulum config, add something like:
```
[[MODEM73]]
type = Modem73Interface
enabled = yes
target_host = 127.0.0.1
target_port = 8001
control_host = 127.0.0.1
control_port = 8073
```

## Settings

Settings, presets, and the performance log are stored in `%APPDATA%\modem73\`.
