uhidctl
=======

uhidctl is utility to control USB HID power relays.

Supported USB relays
====================

This utility is designed to work for USB HID power relays which can be found on eBay or Amazon as:

`For Smart Home 5V USB Relay Programmable Computer Control`

<img src="https://bit.ly/3nHaRyD" width="300" align="center">

This utility supports such devices with 1, 2, 4, 8 ports (16 port devices will require patching to work).
Hardware that was tested to work has following characteristics (N is number of ports):

| Property       | Value             |
|:---------------|:------------------|
| Manufacturer   | `www.dcttech.com` |
| Product        | USBRelay{N}       |
| Vendor ID      | 0x16C0            |
| Product ID     | 0x05DF            |


Compiling
=========

This utility was tested to compile and work on Linux (Ubuntu/Debian, Redhat/Fedora/CentOS) and MacOS.
It should be possible to compile it for Windows as well - please report if you succeed in doing that.

First, you need to install library hidapi:

* Ubuntu: `sudo apt-get install libhidapi-dev`
* Redhat: `sudo yum install hidapi-devel`
* MacOS:  `brew install hidapi`
* Windows: [download hidapi](https://github.com/libusb/hidapi/releases)

To fetch `uhidctl` source and compile it:

    git clone https://github.com/mvp/uhidctl
    cd uhidctl
    make

This should generate `uhidctl` binary.

You can install it in your system using:

    sudo make install


Linux USB permissions
=====================

On Linux, you should configure `udev` USB permissions (otherwise you will have to run it as root using `sudo uhidctl`).
Simply add following line to file `/etc/udev/rules.d/52-usb.rules`:

    SUBSYSTEM=="usb", ATTR{idVendor}=="16c0", MODE="0666"

For your `udev` rule changes to take effect, reboot or run:

    sudo udevadm trigger --attr-match=subsystem=usb


Usage
=====

To list all compatible relays, run uhidctl without parameters:

    uhidctl

To control relay state:

    uhidctl -a 1 -p 2

This means operate on default USB relay, turn power off (`-a 0`, or `-a off`)
on port 2 (`-p 2`). Supported actions are `0`/`1`/`2` (or `off`/`on`/`cycle`).
`cycle` means turn power off, wait some delay (configurable with `-d`) and turn it back on.

On Linux, you may have to run it with `sudo`, or to configure `udev` USB permissions.

If you have more than one USB relay connected, you should choose
specific relay to control using option `-l`.


Copyright
=========

Copyright (C) 2017-2020 Vadim Mikhailov

This file can be distributed under the terms and conditions of the
GNU General Public License version 2.
