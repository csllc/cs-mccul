# cs-mccul: Measurement Computing Universal Library

This package provides a Node.js interface for Measurement Computing's `uldaq` library. It uses native code through N-API and currently supports Linux only. It was developed and tested using Node.js 24 on Debian 13.

## Installation

Install dependencies. The module is compiled during installation unless a prebuilt binary for your platform is included in the package, so a compiler and the libusb development files are needed (see [Building](#building) for details):

```bash
sudo apt install build-essential python3 libusb-1.0-0-dev
```

Install this package:

```bash
npm install @csllc/cs-mccul
```

### Post-installation

Before this library can be used by non-root users, `udev` rules must be applied to grant permission to access the devices. If this module and its submodule are checked out locally, copy the rules file from the submodule:

```bash
sudo cp vendor/uldaq/rules/50-uldaq.rules /lib/udev/rules.d/
```

Otherwise, download it from the `uldaq` repository: [50-uldaq.rules](https://raw.githubusercontent.com/mccdaq/uldaq/refs/heads/master/rules/50-uldaq.rules).

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

Building from source is only needed for development, or on a platform without a prebuilt binary. The `uldaq` library is compiled from the `vendor/uldaq` submodule and linked statically into the module, so a system-wide `libuldaq` installation is not required (or used).

### Prerequisites

```bash
sudo apt install build-essential python3 git libusb-1.0-0-dev
```

Clone the repository with its submodule:

```bash
git clone --recurse-submodules https://github.com/csllc/cs-mccul.git
```

or, in an existing checkout:

```bash
git submodule update --init
```

### Building the module

`npm install` installs the dependencies and builds the module, unless a matching prebuilt binary is present in `prebuilds/`.

After that, use the build scripts:

| Command | Description |
| --- | --- |
| `npm run build` | Incremental build: recompiles only what has changed. Use this during development. |
| `npm run rebuild` | Clean build: deletes `build/` and recompiles everything, including all of `uldaq`. Use this after switching Node versions or architectures, or if the build gets into a bad state. |

The compiled module is written to `build/Release/cs_mccul.node`.

### Prebuilt binaries

Prebuilt binaries are created with [`prebuildify`](https://github.com/prebuild/prebuildify) and written to `prebuilds/<platform>-<arch>/`. Because the module uses N-API, one binary per architecture works with all supported Node versions. When the package is installed, [`node-gyp-build`](https://github.com/prebuild/node-gyp-build) loads a matching prebuilt binary if one exists, and only compiles from source otherwise.

| Command | Description |
| --- | --- |
| `npm run prebuildify:x64` | Build the `linux-x64` binary |
| `npm run prebuildify:arm64` | Cross-compile the `linux-arm64` binary |
| `npm run prebuildify` | Build both |

Cross-compiling for arm64 on an x64 Debian host requires the cross toolchain and the arm64 version of libusb:

```bash
sudo dpkg --add-architecture arm64
sudo apt update
sudo apt install g++-aarch64-linux-gnu libusb-1.0-0-dev:arm64
```

Systems that use a prebuilt binary only need the libusb runtime (`libusb-1.0-0`), not the `-dev` package or a compiler. They do still need the `udev` rules described above.

### Constants

The constants in `lib/mcculConst.js` (exposed as `ul.const`) are generated from the enums in `vendor/uldaq/src/uldaq.h`. To expose another enum, add its name to the `ENUMS` list in `tools/gen-const.js`, then regenerate the file:

```bash
node tools/gen-const.js
```

Regenerate it after updating the `uldaq` submodule as well, and review the changes.

### Editor support

`compile_flags.txt` provides include paths for `clangd`-based editors. The N-API headers come from the `node-api-headers` dev dependency, so they are available without a local Node source tree, including when editing on a different machine from the one used for building.

## Examples

The `example` directory contains JavaScript ports of example programs that ship with `uldaq`. Each one follows its C original step for step, using this module in place of the C API:

| Example | Ported from | Demonstrates |
| --- | --- | --- |
| `AIn.js` | `AIn.c` | Reading analog input channels with `aIn()` |
| `AInScan.js` | `AInScan.c` | Continuous hardware-paced analog input scan with `aInScan()` |
| `DIn.js` | `DIn.c` | Reading a digital port with `dIn()` |
| `DInScan.js` | `DInScan.c` | Continuous hardware-paced digital input scan with `dInScan()` |
| `CIn.js` | `CIn.c` | Clearing and reading a counter with `cClear()` and `cIn()` |
| `CInScan.js` | `CInScan.c` | Continuous hardware-paced counter input scan with `cInScan()` |
| `DaqInScan.js` | `DaqInScan.c` | Mixed analog, digital, and counter scan with `daqInScan()` |

Run an example from the package directory:

```bash
node example/AIn.js
```

Each example:

1. Lists the DAQ devices it finds on any interface, and asks which to use if there is more than one
2. Checks that the device supports the subsystem being demonstrated, and exits with a message if it doesn't
3. Connects to the device and shows the settings it will use
4. Waits for Enter, then displays live data until Enter is pressed again
5. Stops any running scan, then disconnects from and releases the device

Hardware capabilities vary by device, so some examples won't run on some devices. For example, a device without a hardware pacer for digital input can't run `DInScan.js`.

`example/utility.js` contains ports of the helper functions from the `uldaq` examples' `utility.h` that these examples use, such as `getAiInfoFirstSupportedInputMode()` and `convertScanOptionsToString()`. They are example conveniences built on this module's API, not part of the module itself.

## Usage

```js
const MccUl = require('@csllc/cs-mccul');

async function main() {
  const ul = new MccUl();
  const { DaqDeviceInterface, AiInputMode, Range } = ul.const;

  const devices = await ul.getDaqDeviceInventory(DaqDeviceInterface.USB_IFC);
  if (devices.length === 0) {
    throw new Error('No DAQ devices found');
  }

  const handle = await ul.createDaqDevice(devices[0]);

  try {
    await ul.connectDaqDevice(handle);

    const volts = await ul.aIn(handle, 0, AiInputMode.AI_SINGLE_ENDED, Range.BIP10VOLTS);
    console.log(`Channel 0: ${volts.toFixed(4)} V`);

    await ul.disconnectDaqDevice(handle);
  } finally {
    await ul.releaseDaqDevice(handle);
  }
}

main().catch(console.error);
```

### Conventions

The API follows the `uldaq` C API closely, so the [UL for Linux documentation](https://files.digilent.com/manuals/UL-Linux/c/index.html) applies to it directly. The method name is the C function name without the `ul` prefix, starting with a lower-case letter (`ulAInScan()` becomes `aInScan()`). The exceptions are `aiGetInfo()` and `dioGetInfo()`, for `ulAIGetInfo()` and `ulDIOGetInfo()`. The arguments are in the same order, except as noted below.

- **Constants**: all enum values are available on `ul.const` (or from `lib/mcculConst.js`), grouped by their C enum name, with the same names and values: for example `ul.const.Range.BIP10VOLTS` and `ul.const.ScanOption.SO_CONTINUOUS`. `ul.const.nameOf(enumObject, value)` returns the name of a value, e.g. `nameOf(Range, 5)` returns `'BIP10VOLTS'`.
- **Output parameters become return values**: values that the C API returns through a pointer are returned instead. For example, `aIn()` resolves with the reading, and the `*Scan()` methods resolve with the actual scan rate.
- **Promises and plain return values**: methods that communicate with the device return Promises. Methods that only query information (`*GetInfo()`, `*ScanStatus()`, `getDevInfoHasAi()`) return their result directly.
- **Device handles** are Numbers.
- **Errors**: a failed UL call throws (or rejects with) an `Error` whose `code` is the numeric `UlError` value as a string, and whose `message` names the UL function and includes the UL's description of the error. The exception is `createDaqDevice()`: `ulCreateDaqDevice()` reports failure without an error code, so its `Error` has no `code`. Invalid arguments throw (or reject with) a `TypeError` or `RangeError`.
- **64-bit values**: `dIn()` and `cIn()` return Numbers, or BigInts if the value exceeds `Number.MAX_SAFE_INTEGER`. Digital and counter scan data is always BigInt, since those scans use a `BigUint64Array` buffer.
- **Blocking**: calls currently run on Node's main thread and block it until the UL returns. Background scans run independently once started.

### Scans

A `*Scan()` method starts a scan that runs in the background and writes into a buffer you provide. With `ScanOption.SO_CONTINUOUS` the buffer is used as a circular buffer. The buffer must be a typed array of the right type, with room for at least (number of channels) × (samples per channel) elements:

| Scan | Buffer type |
| --- | --- |
| `aInScan()`, `daqInScan()` | `Float64Array` |
| `dInScan()`, `cInScan()` | `BigUint64Array` |

Poll a running scan with the matching `*ScanStatus()` method. It returns `{ status, currentScanCount, currentTotalCount, currentIndex }`, where `status` is a `ScanStatus` value and the most recent set of samples starts at `buffer[currentIndex]`. Stop the scan with the matching `*ScanStop()` method.

The module keeps the buffer alive while the scan runs, so it is safe to let your own reference to it go. It is released when the scan is stopped, when another scan of the same type is started on the device, or when the device is released.

```js
const { AiInputMode, Range, ScanOption, ScanStatus, AInScanFlag } = ul.const;

const lowChan = 0, highChan = 3, samplesPerChan = 1000;
const buffer = new Float64Array((highChan - lowChan + 1) * samplesPerChan);

const rate = await ul.aInScan(handle, lowChan, highChan, AiInputMode.AI_SINGLE_ENDED,
  Range.BIP10VOLTS, samplesPerChan, 100, ScanOption.SO_CONTINUOUS,
  AInScanFlag.AINSCAN_FF_DEFAULT, buffer);

const { status, currentIndex } = ul.aInScanStatus(handle);
if (status === ScanStatus.SS_RUNNING && currentIndex >= 0) {
  console.log('Latest channel 0 sample:', buffer[currentIndex]);
}

await ul.aInScanStop(handle);
```

### API

#### Devices

| Method | UL function | Returns |
| --- | --- | --- |
| `getDaqDeviceInventory(interfaceType)` | `ulGetDaqDeviceInventory()` | Promise of an array of device descriptors: `{ productName, productId, devInterface, devString, uniqueId }` |
| `createDaqDevice(devDescriptor)` | `ulCreateDaqDevice()` | Promise of a device handle |
| `connectDaqDevice(handle)` | `ulConnectDaqDevice()` | Promise |
| `disconnectDaqDevice(handle)` | `ulDisconnectDaqDevice()` | Promise |
| `releaseDaqDevice(handle)` | `ulReleaseDaqDevice()` | Promise |
| `devGetInfo(handle, infoItem, index = 0)` | `ulDevGetInfo()` | Number (`infoItem` is a `DevInfoItem`) |
| `getDevInfoHasAi(handle)` | `ulDevGetInfo(DEV_INFO_HAS_AI_DEV)` | Boolean |

#### Analog input

| Method | UL function | Returns |
| --- | --- | --- |
| `aiGetInfo(handle, infoItem, index = 0)` | `ulAIGetInfo()` | Number (`infoItem` is an `AiInfoItem`) |
| `aIn(handle, channel, inputMode, range, flags = AIN_FF_DEFAULT)` | `ulAIn()` | Promise of the reading |
| `aInScan(handle, lowChan, highChan, inputMode, range, samplesPerChan, rate, options, flags, buffer)` | `ulAInScan()` | Promise of the actual scan rate |
| `aInScanStatus(handle)` | `ulAInScanStatus()` | Scan status object |
| `aInScanStop(handle)` | `ulAInScanStop()` | Promise |

#### Digital I/O

| Method | UL function | Returns |
| --- | --- | --- |
| `dioGetInfo(handle, infoItem, index = 0)` | `ulDIOGetInfo()` | Number (`infoItem` is a `DioInfoItem`) |
| `dConfigPort(handle, portType, direction)` | `ulDConfigPort()` | Promise |
| `dIn(handle, portType)` | `ulDIn()` | Promise of the port value |
| `dInScan(handle, lowPort, highPort, samplesPerPort, rate, options, flags, buffer)` | `ulDInScan()` | Promise of the actual scan rate |
| `dInScanStatus(handle)` | `ulDInScanStatus()` | Scan status object |
| `dInScanStop(handle)` | `ulDInScanStop()` | Promise |

#### Counters

| Method | UL function | Returns |
| --- | --- | --- |
| `ctrGetInfo(handle, infoItem, index = 0)` | `ulCtrGetInfo()` | Number (`infoItem` is a `CtrInfoItem`) |
| `cClear(handle, counterNum)` | `ulCClear()` | Promise |
| `cIn(handle, counterNum)` | `ulCIn()` | Promise of the count |
| `cInScan(handle, lowCounterNum, highCounterNum, samplesPerCounter, rate, options, flags, buffer)` | `ulCInScan()` | Promise of the actual scan rate |
| `cInScanStatus(handle)` | `ulCInScanStatus()` | Scan status object |
| `cInScanStop(handle)` | `ulCInScanStop()` | Promise |

#### DAQ input (mixed analog, digital, and counter channels)

| Method | UL function | Returns |
| --- | --- | --- |
| `daqIGetInfo(handle, infoItem, index = 0)` | `ulDaqIGetInfo()` | Number (`infoItem` is a `DaqIInfoItem`) |
| `daqInScan(handle, chanDescriptors, samplesPerChan, rate, options, flags, buffer)` | `ulDaqInScan()` | Promise of the actual scan rate |
| `daqInScanStatus(handle)` | `ulDaqInScanStatus()` | Scan status object |
| `daqInScanStop(handle)` | `ulDaqInScanStop()` | Promise |

`daqInScan()` takes the channel list as an array of `{ channel, type, range }` objects, where `type` is a `DaqInChanType` value and `range` is only needed for analog channels. The C function's separate `numChans` argument is taken from the length of the array.

# Licensing and Dependencies

This project is licensed under the MIT license - see the [LICENSE](LICENSE) file for details.

It incorporates the following third-party components via Git submodules:
* [`uldaq`](https://github.com/mccdaq/uldaq) - Licensed under the MIT license by [Measurement Computing](https://github.com/mccdaq).
