# cs-mccul

This package provides a NodeJS interface for Measurement Computing's `uldaq` library. It uses native code through N-API and currently supports Linux only. It was developed and tested using NodeJS 24 on Debian 13.

## Installation

Install dependencies:

```bash
sudo apt install libusb-1.0-0-dev
```

Install this package:

```bash
npm install @csllc/cs-mccul
```

### Post-installation

Before this library can be executed by non-root users, `udev` rules must be applied to grant permissions. If this module and its submodule is checked out locally, copy the file directly.

```bash
sudo cp vendor/uldaq/rules/50-uldaq.rules /lib/udev/rules.d/
```

Otherwise it can be obtained directly from the `uldaq` repository: [50-uldaq.rules](https://github.com/mccdaq/uldaq/blob/master/rules/50-uldaq.rules)

Then reload the rules and apply them:

```bash
sudo udevadm control --reload-rules
```

```bash
sudo udevadm trigger --subsystem-match=usb --attr-match=idVendor=09db
```

If these steps aren't performed, attempting to open a device will fail with an error like:

```text
libusb_open() failed: LIBUSB_ERROR_ACCESS
```

## Building

`npm run build` will invoke a full or partial build

## Examples

(Explain examples - a subset of them were ported from C)

## Usage

(list of supported API calls)

# Licensing and Dependencies

This project is licensed under the MIT license - see the [LICENSE](LICENSE) file for details.

It incorporates the following third-party components via Git submodules:
* [`uldaq`](https://github.com/mccdaq/uldaq) - Licensed under the MIT license by [Measurement Computing](https://github.com/mccdaq).
