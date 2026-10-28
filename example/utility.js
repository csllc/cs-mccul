'use strict';

// Port of the helper functions from the uldaq examples' utility.h that are
// used by the ported examples. These are example conveniences, not part of
// the UL API.

const readline = require('readline');

const {
  AiInputMode,
  AiInfoItem,
  Range,
  DevInfoItem,
  DioInfoItem,
  DigitalPortType,
  DigitalPortIoType,
  CtrInfoItem,
  DaqIInfoItem,
  ScanOption,
  nameOf,
} = require('../lib/mcculConst');

// Shared line reader on stdin, used for device selection and Enter detection
const rl = readline.createInterface({ input: process.stdin });

// Prompt for a device index when more than one DAQ device was found
// (selectDAQDevice() in utility.h)
function selectDAQDevice(numDevs) {
  return new Promise((resolve) => {
    const ask = () => {
      process.stdout.write(`\nPlease select a DAQ device, enter a number between 0 and ${numDevs - 1}: `);
      rl.once('line', (line) => {
        const index = parseInt(line, 10);
        if (index >= 0 && index < numDevs) {
          resolve(index);
        } else {
          ask();
        }
      });
    };
    ask();
  });
}

// Resolve when the user presses Enter
function waitForEnter() {
  return new Promise((resolve) => rl.once('line', () => resolve()));
}

// Return a function that reports whether Enter has been pressed since this
// was called (enter_press() in utility.h)
function watchForEnter() {
  let pressed = false;
  rl.once('line', () => { pressed = true; });
  return () => pressed;
}

// Stop reading stdin so the process can exit
function closeInput() {
  rl.close();
}

// Move the cursor to the top-left of the terminal (resetCursor() in utility.h)
function resetCursor() {
  process.stdout.write('\x1b[1;1H');
}

// Clear the current terminal line (clearEOL() in utility.h)
function clearEOL() {
  process.stdout.write('\x1b[2K');
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

// Find the first supported analog input mode, preferring single-ended
// (getAiInfoFirstSupportedInputMode() in utility.h).
// Returns { inputMode, inputModeStr, numberOfChannels }.
function getAiInfoFirstSupportedInputMode(ul, devHandle) {
  for (const inputMode of [AiInputMode.AI_SINGLE_ENDED, AiInputMode.AI_DIFFERENTIAL]) {
    const numberOfChannels = ul.aiGetInfo(devHandle, AiInfoItem.AI_INFO_NUM_CHANS_BY_MODE, inputMode);

    if (numberOfChannels > 0) {
      return {
        inputMode,
        inputModeStr: nameOf(AiInputMode, inputMode),
        numberOfChannels,
      };
    }
  }

  throw new Error('The DAQ device has no single-ended or differential analog inputs');
}

// Get the first supported analog input range for the given input mode
// (getAiInfoFirstSupportedRange() in utility.h).
// Returns { range, rangeStr }.
function getAiInfoFirstSupportedRange(ul, devHandle, inputMode) {
  const infoItem = (inputMode === AiInputMode.AI_SINGLE_ENDED)
    ? AiInfoItem.AI_INFO_SE_RANGE
    : AiInfoItem.AI_INFO_DIFF_RANGE;

  const range = ul.aiGetInfo(devHandle, infoItem, 0);

  return { range, rangeStr: nameOf(Range, range) };
}

// Return true if the device has a digital I/O subsystem
// (getDevInfoHasDio() in utility.h)
function getDevInfoHasDio(ul, devHandle) {
  return ul.devGetInfo(devHandle, DevInfoItem.DEV_INFO_HAS_DIO_DEV, 0) !== 0;
}

// Get the type of the first digital port (AUXPORT0, FIRSTPORTA, ...)
// (getDioInfoFirstSupportedPortType() in utility.h).
// Returns { portType, portTypeStr }.
function getDioInfoFirstSupportedPortType(ul, devHandle) {
  const portType = ul.dioGetInfo(devHandle, DioInfoItem.DIO_INFO_PORT_TYPE, 0);

  return { portType, portTypeStr: nameOf(DigitalPortType, portType) };
}

// Get the I/O type of the first digital port
// (getDioInfoFirstSupportedPortIoType() in utility.h).
// Returns { portIoType, portIoTypeStr }.
function getDioInfoFirstSupportedPortIoType(ul, devHandle) {
  const portIoType = ul.dioGetInfo(devHandle, DioInfoItem.DIO_INFO_PORT_IO_TYPE, 0);

  return { portIoType, portIoTypeStr: nameOf(DigitalPortIoType, portIoType) };
}

// Return true if the device has a counter subsystem
// (getDevInfoHasCtr() in utility.h)
function getDevInfoHasCtr(ul, devHandle) {
  return ul.devGetInfo(devHandle, DevInfoItem.DEV_INFO_HAS_CTR_DEV, 0) !== 0;
}

// Return true if the device has a DAQ input subsystem
// (getDevInfoHasDaqi() in utility.h)
function getDevInfoHasDaqi(ul, devHandle) {
  return ul.devGetInfo(devHandle, DevInfoItem.DEV_INFO_HAS_DAQI_DEV, 0) !== 0;
}

// Return true if analog input supports hardware pacing
// (getAiInfoHasPacer() in utility.h)
function getAiInfoHasPacer(ul, devHandle) {
  return ul.aiGetInfo(devHandle, AiInfoItem.AI_INFO_HAS_PACER, 0) !== 0;
}

// Return true if digital I/O supports hardware pacing in the given
// const.DigitalDirection (getDioInfoHasPacer() in utility.h)
function getDioInfoHasPacer(ul, devHandle, direction) {
  return ul.dioGetInfo(devHandle, DioInfoItem.DIO_INFO_HAS_PACER, direction) !== 0;
}

// Return true if counter input supports hardware pacing
// (getCtrInfoHasPacer() in utility.h)
function getCtrInfoHasPacer(ul, devHandle) {
  return ul.ctrGetInfo(devHandle, CtrInfoItem.CTR_INFO_HAS_PACER, 0) !== 0;
}

// Return the number of counters (getCtrInfoNumberOfChannels() in utility.h)
function getCtrInfoNumberOfChannels(ul, devHandle) {
  return ul.ctrGetInfo(devHandle, CtrInfoItem.CTR_INFO_NUM_CTRS, 0);
}

// Return the mask of const.DaqInChanType values supported by DAQ input
// (getDaqiChannelTypes() in utility.h)
function getDaqiChannelTypes(ul, devHandle) {
  return ul.daqIGetInfo(devHandle, DaqIInfoItem.DAQI_INFO_CHAN_TYPES, 0);
}

// Describe a set of const.ScanOption flags, e.g. "SO_CONTINUOUS"
// (ConvertScanOptionsToString() in utility.h)
function convertScanOptionsToString(scanOptions) {
  if (scanOptions === ScanOption.SO_DEFAULTIO) {
    return 'Default';
  }

  return ['SO_SINGLEIO', 'SO_BLOCKIO', 'SO_BURSTIO', 'SO_CONTINUOUS', 'SO_EXTCLOCK',
    'SO_EXTTRIGGER', 'SO_RETRIGGER', 'SO_BURSTMODE', 'SO_PACEROUT']
    .filter((name) => scanOptions & ScanOption[name])
    .join(', ');
}

// Format a value like printf's "%+-10.6f"
function formatSigned(value) {
  return ((value < 0 ? '' : '+') + value.toFixed(6)).padEnd(10);
}

module.exports = {
  selectDAQDevice,
  waitForEnter,
  watchForEnter,
  closeInput,
  resetCursor,
  clearEOL,
  sleep,
  getAiInfoFirstSupportedInputMode,
  getAiInfoFirstSupportedRange,
  getDevInfoHasDio,
  getDioInfoFirstSupportedPortType,
  getDioInfoFirstSupportedPortIoType,
  getDevInfoHasCtr,
  getDevInfoHasDaqi,
  getAiInfoHasPacer,
  getDioInfoHasPacer,
  getCtrInfoHasPacer,
  getCtrInfoNumberOfChannels,
  getDaqiChannelTypes,
  convertScanOptionsToString,
  formatSigned,
};
