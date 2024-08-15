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
Application]() - Link to be updated,
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
  HPLIP. They are packaged in the Rock as a compressed archive.

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

- The Rock of the HPLIP Printer Application takes HPLIP's source code
  from Debian's packaging repository instead of directly from HP, as
  Debian's package has ~80 patches fixing bugs which are reported to
  HP but the patch not adopted upstream. So with the Rock users should
  get the same experience in reliability and quality as with the
  Debian package.

- Support for downloading the proprietary plugin of HPLIP via an
  additional page in the web interface. This adds support for some
  laser printers which need their firmware loaded every time they are
  turned on or which use certain proprietary print data formats. This
  works both in the Rock and in the classic installation of the
  Printer Application (must run as root, otherwise only status check
  of the plugin).

## Install from Docker Hub
### Prerequisites

1. **Docker Installed**: Ensure Docker is installed on your system. You can download it from the [official Docker website](https://www.docker.com/get-started).

### Step-by-Step Guide

#### 1. Pull the hplip-printer-app docker image:

The first step is to pull the hplip-printer-app Docker image from Docker Hub.
```sh
sudo docker pull openprinting/hplip-printer-app
```

#### 2. Start the hplip-printer-app Container

##### Run the following Docker command to run the hplip-printer-app image:
```sh
sudo docker run --rm -d --name hplip-printer-app -p <port>:8000 \
    openprinting/hplip-printer-app:latest
```

## Setting Up and Running hplip-printer-app locally

### Prerequisites

1. **Docker Installed**: Ensure Docker is installed on your system. You can download it from the [official Docker website](https://www.docker.com/get-started).

2. **Rockcraft**: Rockcraft should be installed. You can install Rockcraft using the following command:
```sh
sudo snap install rockcraft --classic
```

3. **Skopeo**: Skopeo should be installed to compile `.rock` files into Docker images. <br>
**Note**: It comes bundled with Rockcraft.

### Step-by-Step Guide

#### 1. Build phplip-printer-app rock:

The first step is to build the Rock from the `rockcraft.yaml`. This image will contain all the configurations and dependencies required to run hplip-printer-app.

Open your terminal and navigate to the directory containing your `rockcraft.yaml`, then run the following command:

```sh
rockcraft pack -v
```

#### 2. Compile to Docker Image:

Once the rock is built, you need to compile docker image from it.

```sh
sudo rockcraft.skopeo --insecure-policy copy oci-archive:<rock_image> docker-daemon:hplip-printer-app:latest
```

#### Run the hplip-printer-app Docker Container:

```sh
sudo docker run --rm -d --name hplip-printer-app -p <port>:8000 \
    hplip-printer-app:latest
```

### Setting up

Enter the web interface

```sh
http://localhost:<port>/
```

Use the web interface to add a printer. Supply a name, select the
discovered printer, then select make and model. Also set the installed
accessories, loaded media and the option defaults. If the printer is a
PostScript printer, accessory configuration and option defaults can
also often get polled from the printer.