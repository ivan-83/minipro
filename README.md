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

## Prerequisites

You'll need some sort of Linux or BSD machine.

## Compilation and Installation
```
sudo apt-get install build-essential git cmake fakeroot dpkg-dev libusb-1.0-0-dev
git clone --recursive https://github.com/rozhuk-im/minipro.git
cd minipro
mkdir build
cd build
cmake ..
make
```

