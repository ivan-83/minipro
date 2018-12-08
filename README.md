minipro
========
An open source program for controlling the MiniPRO TL866xx series of chip programmers 

## Features
* Compatibility with Minipro TL866CS and Minipro TL866A from 
Autoelectric (http://www.autoelectric.cn/)
* More than 13000 target devices (including AVRs, PICs, various BIOSes and EEPROMs)
* ZIF40 socket and ISP support
* Vendor-specific MCU configuration bits
* Chip ID verification
* Overcurrent protection
* System testing

## This fork features
* Advanced error handling
* More options for partial flash read/write
* Chips DB in text file, so user can add/edit chips without recompilation
* BSD systems support (tested on FreeBSD)
* Modern CMake build system
* Less code, more clean code, fixed code style
* All futures after fork is backported
* Tool to extract chips database from vendor InfoIC.dll library

## Prerequisites

You'll need some sort of Linux or BSD machine.

## Compilation and Installation
```
sudo apt-get install build-essential git cmake pkgconf fakeroot dpkg-dev libusb-1.0-0-dev
git clone --recursive https://github.com/rozhuk-im/minipro.git
cd minipro
mkdir build
cd build
cmake ..
make -j
```

