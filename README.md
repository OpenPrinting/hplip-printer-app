# HPLIP Printer Application

## INTRODUCTION

This repository contains a Printer Application for printing on HP and
Apollo printers supported by HP's HPLIP driver suite.

It uses [PAPPL](https://www.msweet.org/pappl) to support IPP
printing from multiple operating systems. In addition, it uses the
resources of [cups-filters
2.x](https://github.com/OpenPrinting/cups-filters) (filter functions
in libcupsfilters, libppd) and
[pappl-retrofit](https://github.com/OpenPrinting/pappl-retrofit)
(encapsulating classic CUPS drivers in Printer Applications). This
work (or now the code of pappl-retrofit) is derived from the
[hp-printer-app](https://github.com/michaelrsweet/hp-printer-app).

The printer driver itself and the software to communicate with the
printer hardware is taken from the [HPLIP (HP Linux Imaging and
Printing) driver
suite](https://developers.hp.com/hp-linux-imaging-and-printing/), also
the Information about supported printer models and their capabilities.

Your contributions are welcome. Please post [issues and pull
requests](https://github.com/OpenPrinting/hplip-printer-app).

**Note: HPLIP is actively maintained by HP, they are continuously
adding the newest printer models and adapting the software to new
environments/Linux distributions. Therefore it would also be the
correct way if HP turns HPLIP into a Printer Application or at least
offers this as an alternative to the classic CUPS/SANE
driver. Especially they should create a native Printer Application,
meaning that it does not use PPDs, CUPS filters, and CUPS backends
internally. Also their utilities need to be made independent of
CUPS.**

For PostScript printers you can also use the [PostScript Printer
Application](https://github.com/OpenPrinting/ps-printer-app),
especially if you have it already installed for some non-HP PostScript
printer.

Also check whether your printer is a driverless IPP printer (AirPrint,
Mopria, IPP Everywhere, Wi-Fi Direct Print, prints from phones) as in
this case you do not need any Printer Application at all. Most modern
HP printers, even the cheapest models, are driverless IPP
printers. Even USB-only printers can be driverless IPP, and you can
generally use driverless IPP via USB, try
[ipp-usb](https://github.com/OpenPrinting/ipp-usb) for these cases
first.


### Properties

- A Printer Application providing the `hpcups` printer driver and all
  printer's PPDs of HPLIP, supporting printing on most printers from
  HP and Apollo. This allows easy printing in high quality, including
  photos on photo paper. The `hpps` CUPS filter for PIN-protected
  printing on PostScript printers is also included.

- The printers are discovered with HPLIP, too. For USB printers the
  `hp` CUPS backend is used and for network printers the `hp-probe`
  utility (encapsulated in a script to behave as a CUPS backend).

- The communication with the printers is done by the `hp` CUPS backend
  and so (at least in case of USB) the IEEE-1284.4 packet protocol
  (protocol 7/1/3 on USB) is used and not a simple stream protocol
  (like the standard CUPS and PAPPL backends use). This way one should
  be able to print and scan simultaneously, or at least check printer
  status while printing. Not all printers support this protocol, if
  not, a standard streaming protocol is used. Also any other special
  functionality which requires the `hp` backend is supported. On the
  "Add Printer" web interface page under "Devices" select the "HPLIP
  (HP)" entries.

- Note that the `hp` backend does not allow bi-directional access to
  the printer. If you have a PostScript printer and prefer support for
  remote querying of the printer's accessory configuration instead of
  simultaneous printing and scanning, CUPS' standard backends for USB
  and network printers are also available.

- If you have an unusal system configuration or a personal firewall
  HP's backends will perhaps not discover your printer. Also in this
  situation the standard backends, including the fully manual "Network
  Printer" entry in combination with the hostname/IP field can be
  helpful.

- Use of CUPS' instead of PAPPL's standard backends makes quirk
  workarounds for USB printers with compatibility problems being used
  (and are editable) and the output can get sent to the printer via
  IPP, IPPS (encrypted!), and LPD in addition to socket (usually port
  9100). The SNMP backend can get configured (community, address
  scope).

- PWG Raster, Apple Raster or image input data to be printed on a
  non-PostScript printer does not get converted to PostScript or PDF,
  it is only converted/scaled to the required color space and
  resolution and then fed into the `hpcups` driver.

- For printing on non-PostScript printers PDF and PostScript input
  data is rendered into raster data using Ghostscript. Ghostscript is
  also used to convert PDF into PostScript for PostScript printers.

- The information about which printer models are supported and which
  are their capabilities is based on the PPD files included in
  HPLIP. They are packaged in the Snap as a compressed archive.

- Standard job IPP attributes are mapped to the driver's option
  settings best fitting to them so that users can print from any type
  of client (like for example a phone or IoT device) which only
  supports standard IPP attributes and cannot retrive the PPD
  options. Trays, media sizes, media types, and duplex can get mapped
  easily, but when it comes to color and quality it gets more complex,
  as relevant options differ a lot in the PPD files. Here we use an
  algorithm which automatically (who wants hand-edit ~3000 PPD files
  for the assignments) finds the right set of option settings for each
  combination of `print-color-mode` (`color`/`monochrome`),
  `print-quality` (`draft`/`normal`/`high`), and
  `print-content-optimize`
  (`auto`/`photo`/`graphics`/`text`/`text-and-graphics`) in the PPD of
  the current printer. So you have easy access to the full quality or
  speed of your printer without needing to deal with printer-specific
  option settings (the original options are still accessible via web
  admin interface).

- The Snap of the HPLIP Printer Application takes HPLIP's source code
  from Debian's packaging repository instead of directly from HP, as
  Debian's package has ~80 patches fixing bugs which are reported to
  HP but the patch not adopted upstream. So with the Snap users should
  get the same experience in reliability and quality as with the
  Debian package.

- Support for downloading the proprietary plugin of HPLIP via an
  additional page in the web interface. This adds support for some
  laser printers which need their firmware loaded every time they are
  turned on or which use certain proprietary print data formats. This
  works both in the Snap and in the classic installation of the
  Printer Application (must run as root, otherwise only status check
  of the plugin).

### To Do

- Support for scanning on HP's multi-function printers. this requires
  scanning support in PAPPL (which made [good progress in GSoC
  2021](https://github.com/michaelrsweet/pappl/commits/scanning)).

- PDF test page, for example generated with the bannertopdf filter.

- Human-readable strings for vendor options (Needs support by PAPPL:
  [Issue #58: Localization
  support](https://github.com/michaelrsweet/pappl/issues/58))

- Internationalization/Localization (Needs support by PAPPL: [Issue
  #58: Localization
  support](https://github.com/michaelrsweet/pappl/issues/58))

- SNMP Ink level check via ps_status() function (Needs support by PAPPL:
  [Issue #83: CUPS does IPP and SNMP ink level polls via backends,
  PAPPL should have functions for
  this](https://github.com/michaelrsweet/pappl/issues/83))

- Build options for cups-filters, to build without libqpdf and/or
  without libppd, the former will allow to create the Snap of this
  Printer Application without downloading and building QPDF


## THE SNAP

### Installing and building

To just run and use this Printer Application, simply install it from
the Snap Store:

```
sudo snap install --edge hplip-printer-app
```

Then follow the instructions below for setting it up.

To build the Snap by yourself, in the main directory of this
repository run

```
snapcraft snap
```

This will download all needed packages and build the HPLIP Printer
Application. Note that PAPPL (upcoming 1.0) and cups-filters (upcoming
2.0) are pulled directly from their GIT repositories, as there are no
appropriate releases yet. This can also lead to the fact that this
Printer Application will suddenly not build any more.

To install the resulting Snap run

```
sudo snap install --dangerous hplip-printer-app_1.0_amd64.snap
```


### Setting up

The Printer Application will automatically be started as a server daemon.

Enter the web interface

```
http://localhost:8000/
```

Use the web interface to add a printer. Supply a name, select the
discovered printer, then select make and model. Also set the installed
accessories, loaded media and the option defaults. If the printer is a
PostScript printer, accessory configuration and option defaults can
also often get polled from the printer.

If the entry of your printer in the web interface has the remark
"requires proprietary plugin", you need to install HP's plugin. For
this, click on the "Plugin" button in this printer entry or on the
"Install Proprietary Plugin" button under "Other Settings" on the
front page of the web interface and follow the instructions on the
screen.

Then print PDF, PostScript, JPEG, Apple Raster, or PWG Raster files
with

```
hplip-printer-app FILE
```

or print with CUPS, CUPS (and also cups-browsed) discover and treat
the printers set up with this Printer Application as driverless IPP
printers (IPP Everywhere and AirPrint).

See

```
hplip-printer-app --help
```

for more options.

Use the "-o log-level=debug" argument for verbose logging in your
terminal window.

You can add files to `/var/snap/hplip-printer-app/common/usb/` for
additional USB quirk rules. Edit the existing files only for quick
tests, as they get replaced at every update of the Snap (to introduce
new rules).

You can edit the `/var/snap/hplip-printer-app/common/cups/snmp.conf`
file for configuring SNMP network printer discovery.

## THE ROCK (OCI CONTAINER IMAGE)

### Install from Docker Hub
#### Prerequisites

1. **Docker Installed**: Ensure Docker is installed on your system. You can download it from the [official Docker website](https://www.docker.com/get-started).

#### Step-by-Step Guide

You can pull the `hplip-printer-app` Docker image from either the GitHub Container Registry or Docker Hub.

**From GitHub Container Registry** <br>
To pull the image from the GitHub Container Registry, run the following command:
```sh
  sudo docker pull ghcr.io/openprinting/hplip-printer-app:latest
```

To run the container after pulling the image from the GitHub Container Registry, use:
```sh
  sudo docker run -d \
      --name hplip-printer-app \
      --network host \
      -e PORT=<port> \
      ghcr.io/openprinting/hplip-printer-app:latest
```

**From Docker Hub** <br>
Alternatively, you can pull the image from Docker Hub, by running:
```sh
  sudo docker pull openprinting/hplip-printer-app
```

To run the container after pulling the image from Docker Hub, use:
```sh
  sudo docker run -d \
      --name hplip-printer-app \
      --network host \
      -e PORT=<port> \
      openprinting/hplip-printer-app:latest
```

- `PORT` is an optional environment variable used to start the printer-app on a specified port. If not provided, it will start on the default port 8000 or, if port 8000 is busy, on 8001 and so on.
- **The container must be started in `--network host` mode** to allow the Printer-Application instance inside the container to access and discover printers available in the local network where the host system is in.
- Alternatively using the internal network of the Docker instance (`-p <port>:8000` instead of `--network host -e PORT=<port>`) only gives access to local printers running on the host system itself.

### Setting Up and Running hplip-printer-app locally

#### Prerequisites

**Docker Installed**: Ensure Docker is installed on your system. You can download it from the [official Docker website](https://www.docker.com/get-started) or from the Snap Store:
```sh
  sudo snap install docker
```

**Rockcraft**: Rockcraft should be installed. You can install Rockcraft using the following command:
```sh
  sudo snap install rockcraft --classic
```

**Skopeo**: Skopeo should be installed to compile `*.rock` files into Docker images. It comes bundled with Rockcraft, so no separate installation is required.

#### Step-by-Step Guide

**Build hplip-printer-app rock**

The first step is to build the Rock from the `rockcraft.yaml`. This image will contain all the configurations and dependencies required to run hplip-printer-app.

Open your terminal and navigate to the directory containing your `rockcraft.yaml`, then run the following command:

```sh
  rockcraft pack -v
```

**Compile to Docker Image**

Once the rock is built, you need to compile docker image from it.

```sh
  sudo rockcraft.skopeo --insecure-policy copy oci-archive:<rock_image> docker-daemon:hplip-printer-app:latest
```

**Run the hplip-printer-app Docker Container**

```sh
  sudo docker run -d \
      --name hplip-printer-app \
      --network host \
      -e PORT=<port> \
      hplip-printer-app:latest
```
- `PORT` is an optional environment variable used to start the printer-app on a specified port. If not provided, it will start on the default port 8000 or, if port 8000 is busy, on 8001 and so on.
- **The container must be started in `--network host` mode** to allow the Printer-Application instance inside the container to access and discover printers available in the local network where the host system is in.
- Alternatively using the internal network of the Docker instance (`-p <port>:8000` instead of `--network host -e PORT=<port>`) only gives access to local printers running on the host system itself.

#### Setting up

Enter the web interface

```sh
http://localhost:<port>/
```

Use the web interface to add a printer. Supply a name, select the
discovered printer, then select make and model. Also set the installed
accessories, loaded media and the option defaults. If the printer is a
PostScript printer, accessory configuration and option defaults can
also often get polled from the printer.

<!-- Begin Included Components -->
## Included Components
  - pappl v1.4.8
  - qpdf v11.10.1
  - ghostscript ghostpdl-10.05.0-test-base-001
  - cups v2.4.11
  - libcupsfilters 2.1.1
  - libppd 2.1.1
  - pyppd release-1-1-0
  - hplip debian/3.22.10+dfsg0-8
<!-- End Included Components -->

## BUILDING WITHOUT PACKAGING OR INSTALLATION

You can also do a "quick-and-dirty" build without snapping and without
needing to install [PAPPL](https://www.msweet.org/pappl),
[cups-filters 2.x](https://github.com/OpenPrinting/cups-filters), and
[pappl-retrofit](https://github.com/OpenPrinting/pappl-retrofit) into
your system. You need a directory with the latest GIT snapshot of
PAPPL, the latest GIT snapshot of cups-filters, and the latest GIT
snapshot of pappl-retrofit (master branches of each). They all need to
be compiled (`./autogen.sh; ./configure; make`), installing not
needed. Also install the header files of all needed libraries
(installing "libcups2-dev" should do it).

In the directory with hplip-printer-app.c run the command line

```
gcc -o hplip-printer-app hplip-printer-app.c $PAPPL_SRC/pappl/libpappl.a $CUPS_FILTERS_SRC/.libs/libppd.a $CUPS_FILTERS_SRC/.libs/libcupsfilters.a $PAPPL_RETROFIT_SRC/.libs/libpappl-retrofit.a -ldl -lpthread  -lppd -lcups -lavahi-common -lavahi-client -lgnutls -ljpeg -lpng16 -ltiff -lz -lm -lusb-1.0 -lpam -lqpdf -lstdc++ -I. -I$PAPPL_SRC/pappl -I$CUPS_FILTERS_SRC/ppd -I$CUPS_FILTERS_SRC/cupsfilters -I$PAPPL_RETROFIT_SRC/pappl/retrofit -L$CUPS_FILTERS_SRC/.libs/ -L$PAPPL_RETROFIT_SRC/.libs/
```

There is also a Makefile, but this needs PAPPL, cups-filters 2.x, and
pappl-retrofit to be installed into your system.

Run

```
./hplip-printer-app --help
```

When running the non-snapped version, by default, PPD files are
searched for in

```
/usr/share/ppd/
/usr/lib/cups/driver/
/var/lib/hplip-printer-app/ppd/
```

You can set the `PPD_PATHS` environment variable to search other
places instead:

```
PPD_PATHS=/path/to/my/ppds:/my/second/place ./hplip-printer-app server
```

Simply put a colon-separated list of any amount of paths into the
variable. Creating a wrapper script is recommended.

Note that only PPD files for the `hpcups` driver of HPLIP are
considred, other PPD files are ignored.

Printers are only discovered via the `hp` backend of HPLIP (USB) or
the `hp-probe` utility of HPLIP (network). For the latter a wrapper
script named `HP` is included which makes the utility be used like a
CUPS backend (discovery mode only). This especially makes only HP and
Apollo printers being discovered. Printers from other manufacturers
are not supported.

Jobs are filtered through `hpcups` and send to the printer via the
`hp` backend (both USB and network).

The standard (not HPLIP's) backends provided as alternative in this
Printer Application are CUPS' backends and not PAPPL's, meaning that
for USB printers CUPS' USB quirk workarounds for compatibility
problems are used, network printers can also be used with IPP, IPPS,
and LPD protocols, and SNMP printer discovery is configurable.

USB Quirk rules in `/usr/share/cups/usb` and the `/etc/cups/snmp.conf`
file can get edited if needed.

Make sure you have HPLIP installed and if you want to use standard
backends, CUPS (at least its backends).

You also need Ghostscript to print PDF or PostScript jobs.

For access to the test page `testpage.ps` use the TESTPAGE_DIR
environment variable:

```
TESTPAGE_DIR=`pwd` PPD_PATHS=/path/to/my/ppds:/my/second/place ./hplip-printer-app server
```

or for your own creation of a test page (PostScript, PDF, PNG, JPEG,
Apple Raster, PWG Raster):

```
TESTPAGE=/path/to/my/testpage/my_testpage.ps PPD_PATHS=/path/to/my/ppds:/my/second/place ./hplip-printer-app server
```


## LEGAL STUFF

The HPLIP Printer Application is Copyright © 2020 by Till Kamppeter.

It is derived from the HP PCL Printer Application, a first working model of
a raster Printer Application using PAPPL. It is available here:

https://github.com/michaelrsweet/hp-printer-app

The HP PCL Printer Application is Copyright © 2019-2020 by Michael R Sweet.

This software is licensed under the Apache License Version 2.0 with an exception
to allow linking against GPL2/LGPL2 software (like older versions of CUPS).  See
the files "LICENSE" and "NOTICE" for more information.
