'use strict';

const { Duplex } = require('stream');

const mcculConst = require('./lib/mcculConst');

// Load the native addon: prebuilds/<platform>-<arch>/ if a matching prebuild
// is present, otherwise build/Release (or build/Debug)
const mccul = require('node-gyp-build')(__dirname);

// Call a native function and return a Promise of its result; a thrown error
// becomes a rejection
function call(fn, ...args) {
  return new Promise((resolve) => resolve(fn(...args)));
}

module.exports = class MccUl extends Duplex {
  constructor(options) {

    super({ objectMode: true });

    let me = this;
  }

  getDaqDeviceInventory(interfaceType) {
    return new Promise((resolve) => {
      let devices = mccul.GetDaqDeviceInventory(interfaceType);
      resolve(devices);
      // will resolve with 'devDescriptors', the length of which provides
      // 'numDevs'
    });
  }

  createDaqDevice(devDescriptor) {
    return new Promise((resolve) => {
      let devHandle = mccul.CreateDaqDevice(devDescriptor);
      resolve(devHandle);
    });
  }

  getDevInfoHasAi(devHandle) {
    return mccul.GetDevInfoHasAi(devHandle);
  }

  connectDaqDevice(devHandle) {
    return new Promise((resolve) => {
      mccul.ConnectDaqDevice(devHandle);
      resolve();
    });
  }

  disconnectDaqDevice(devHandle) {
    return new Promise((resolve) => {
      mccul.DisconnectDaqDevice(devHandle);
      resolve();
    });
  }

  releaseDaqDevice(devHandle) {
    return new Promise((resolve) => {
      mccul.ReleaseDaqDevice(devHandle);
      resolve();
    });
  }

  // Analog input information; infoItem is a const.AiInfoItem value
  aiGetInfo(devHandle, infoItem, index = 0) {
    return mccul.AIGetInfo(devHandle, infoItem, index);
  }

  // Read one analog input channel; resolves with the value (in engineering
  // units unless const.AInFlag.AIN_FF_NOSCALEDATA is set)
  aIn(devHandle, channel, inputMode, range, flags = mcculConst.AInFlag.AIN_FF_DEFAULT) {
    return new Promise((resolve) => {
      resolve(mccul.AIn(devHandle, channel, inputMode, range, flags));
    });
  }

  // Device information; infoItem is a const.DevInfoItem value
  devGetInfo(devHandle, infoItem, index = 0) {
    return mccul.DevGetInfo(devHandle, infoItem, index);
  }

  // Digital I/O information; infoItem is a const.DioInfoItem value, and for
  // per-port items index is the port index (0 = first port)
  dioGetInfo(devHandle, infoItem, index = 0) {
    return mccul.DIOGetInfo(devHandle, infoItem, index);
  }

  // Configure a digital port; portType is a const.DigitalPortType value and
  // direction a const.DigitalDirection value
  dConfigPort(devHandle, portType, direction) {
    return new Promise((resolve) => {
      mccul.DConfigPort(devHandle, portType, direction);
      resolve();
    });
  }

  // Read a digital port; resolves with the port value
  dIn(devHandle, portType) {
    return new Promise((resolve) => {
      resolve(mccul.DIn(devHandle, portType));
    });
  }

  // --- Scans
  //
  // Each *Scan() method starts a background scan that writes into the typed
  // array passed as `buffer` (used as a circular buffer with
  // const.ScanOption.SO_CONTINUOUS) and resolves with the actual scan rate.
  // Poll progress with the matching *ScanStatus() method, which returns
  // { status, currentScanCount, currentTotalCount, currentIndex }; the most
  // recent sample set starts at buffer[currentIndex]. The module keeps the
  // buffer alive until the scan is stopped, another scan of the same type is
  // started, or the device is released.

  // Analog input scan; buffer is a Float64Array with at least
  // (highChan - lowChan + 1) * samplesPerChan elements
  aInScan(devHandle, lowChan, highChan, inputMode, range, samplesPerChan, rate, options,
    flags, buffer) {
    return call(mccul.AInScan, devHandle, lowChan, highChan, inputMode, range, samplesPerChan,
      rate, options, flags, buffer);
  }

  aInScanStatus(devHandle) {
    return mccul.AInScanStatus(devHandle);
  }

  aInScanStop(devHandle) {
    return call(mccul.AInScanStop, devHandle);
  }

  // Digital input scan; buffer is a BigUint64Array with at least
  // (number of ports) * samplesPerPort elements
  dInScan(devHandle, lowPort, highPort, samplesPerPort, rate, options,
    flags, buffer) {
    return call(mccul.DInScan, devHandle, lowPort, highPort, samplesPerPort, rate, options,
      flags, buffer);
  }

  dInScanStatus(devHandle) {
    return mccul.DInScanStatus(devHandle);
  }

  dInScanStop(devHandle) {
    return call(mccul.DInScanStop, devHandle);
  }

  // --- Counters

  // Counter information; infoItem is a const.CtrInfoItem value
  ctrGetInfo(devHandle, infoItem, index = 0) {
    return mccul.CtrGetInfo(devHandle, infoItem, index);
  }

  // Read a counter; resolves with the count
  cIn(devHandle, counterNum) {
    return call(mccul.CIn, devHandle, counterNum);
  }

  // Clear a counter to zero
  cClear(devHandle, counterNum) {
    return call(mccul.CClear, devHandle, counterNum);
  }

  // Counter input scan; buffer is a BigUint64Array with at least
  // (highCounterNum - lowCounterNum + 1) * samplesPerCounter elements
  cInScan(devHandle, lowCounterNum, highCounterNum, samplesPerCounter, rate, options,
    flags, buffer) {
    return call(mccul.CInScan, devHandle, lowCounterNum, highCounterNum, samplesPerCounter,
      rate, options, flags, buffer);
  }

  cInScanStatus(devHandle) {
    return mccul.CInScanStatus(devHandle);
  }

  cInScanStop(devHandle) {
    return call(mccul.CInScanStop, devHandle);
  }

  // --- DAQ input (mixed analog, digital, and counter channels)

  // DAQ input information; infoItem is a const.DaqIInfoItem value
  daqIGetInfo(devHandle, infoItem, index = 0) {
    return mccul.DaqIGetInfo(devHandle, infoItem, index);
  }

  // DAQ input scan; chanDescriptors is an array of { channel, type, range }
  // (type is a const.DaqInChanType value; range is only needed for analog
  // channels), and buffer is a Float64Array with at least
  // chanDescriptors.length * samplesPerChan elements
  daqInScan(devHandle, chanDescriptors, samplesPerChan, rate, options,
    flags, buffer) {
    return call(mccul.DaqInScan, devHandle, chanDescriptors, samplesPerChan, rate, options,
      flags, buffer);
  }

  daqInScanStatus(devHandle) {
    return mccul.DaqInScanStatus(devHandle);
  }

  daqInScanStop(devHandle) {
    return call(mccul.DaqInScanStop, devHandle);
  }

  const = mcculConst

};
