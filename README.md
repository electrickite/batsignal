batsignal - Simple battery monitor
==================================
batsignal is a lightweight battery daemon written in C that notifies the user
about various battery states. It is intended for minimal window managers, but
can be used in any environment that supports desktop notifications via
libnotify.

Features
--------
Customizable messages via libnotify on three configurable battery levels. A
fourth configurable battery level will execute an arbitrary system command. In
order to use as few system resources as possible, fewer battery checks are
performed while the battery is discharging and the level of charge is not near a
warning level.

By default, batsignal will attempt to detect the correct battery to monitor.

Requirements
------------
Batsignal requires the following software to build:

  * C compiler (GCC)
  * libnotify
  * make
  * pkg-config

Installation
------------
Run the following command to build and install batsignal (as root if necessary):

    $ make clean install

Usage
-----
See `man batsignal` for details.

Authors
-------
batsignal is written by Corey Hinshaw. It was orignally forked from juiced by
Aaron Marcher.

License and Copyright
---------------------
Copyright 2018-2020 Corey Hinshaw
Copyright 2016-2017 Aaron Marcher

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
