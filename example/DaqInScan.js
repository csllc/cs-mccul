/*
    Port of the uldaq example "DaqInScan.c" to the cs-mccul NodeJS module

    UL call demonstrated:             ulDaqInScan()

    Purpose:                          Performs a continuous scan of the available
                                      analog, digital, and/or counter input subsystems

    Demonstration:                    Displays the analog, digital, and counter
                                      input data on the specified channels

    Steps:
    1. Call getDaqDeviceInventory() to get the list of available DAQ devices
    2. Call createDaqDevice() to get a handle for the first DAQ device
    3. Verify the DAQ device has a DAQ input subsystem
    4. Get the channel types supported by the DAQ input subsystem
    5. Call connectDaqDevice() to establish a UL connection to the DAQ device
    6. Configure the available analog, digital, and counter channels
    7. Call daqInScan() to start the scan
    8. Call daqInScanStatus() to check the status of the background operation
    9. Display the data for each channel
    10. Call daqInScanStop() to stop the background operation
    11. Call disconnectDaqDevice() and releaseDaqDevice() before exiting the process
*/

'use strict';

const MccUl = require('..');
const {
  selectDAQDevice,
  waitForEnter,
  watchForEnter,
  closeInput,
  resetCursor,
  sleep,
  getDevInfoHasDaqi,
  getDaqiChannelTypes,
  getAiInfoFirstSupportedInputMode,
  getAiInfoFirstSupportedRange,
  getDioInfoFirstSupportedPortType,
  convertScanOptionsToString,
  formatSigned,
} = require('./utility');

const { DaqInChanType, DigitalDirection, Range, nameOf } = require('../lib/mcculConst');

// Add descriptors for the first numberOfChannels single-ended analog channels
// (ConfigureAnalogInputChannels() in DaqInScan.c)
function configureAnalogInputChannels(numberOfChannels, range, descriptors) {
  for (let i = 0; i < numberOfChannels; i++) {
    descriptors.push({ channel: i, type: DaqInChanType.DAQI_ANALOG_SE, range });
  }
}

// Configure the first digital port for input and add a descriptor for it
// (ConfigureDigitalInputChannel() in DaqInScan.c)
async function configureDigitalInputChannel(ul, devHandle, descriptors) {
  const { portType } = getDioInfoFirstSupportedPortType(ul, devHandle);

  // Configure the port for input
  await ul.dConfigPort(devHandle, portType, DigitalDirection.DD_INPUT);

  descriptors.push({ channel: portType, type: DaqInChanType.DAQI_DIGITAL });
}

// Add descriptors for the first numberOfChannels 32-bit counters
// (ConfigureCounterInputChannels() in DaqInScan.c)
function configureCounterInputChannels(numberOfChannels, descriptors) {
  for (let i = 0; i < numberOfChannels; i++) {
    descriptors.push({ channel: i, type: DaqInChanType.DAQI_CTR32 });
  }
}

function isAnalog(type) {
  return type === DaqInChanType.DAQI_ANALOG_SE || type === DaqInChanType.DAQI_ANALOG_DIFF;
}

async function main() {
  const ul = new MccUl();
  const { DaqDeviceInterface, ScanOption, ScanStatus, DaqInScanFlag } = ul.const;

  const interfaceType = DaqDeviceInterface.ANY_IFC;

  // Set some variables that are used to acquire data
  const samplesPerChannel = 10000;
  let rate = 1000;
  const scanOptions = ScanOption.SO_DEFAULTIO | ScanOption.SO_CONTINUOUS;
  const flags = DaqInScanFlag.DAQINSCAN_FF_DEFAULT;

  let daqDeviceHandle = 0;
  let connected = false;
  let scanRunning = false;

  try {
    // Get descriptors for all of the available DAQ devices
    const devDescriptors = await ul.getDaqDeviceInventory(interfaceType);
    const numDevs = devDescriptors.length;

    // Verify at least one DAQ device is detected
    if (numDevs === 0) {
      console.log('No DAQ device is detected');
      return;
    }

    console.log(`Found ${numDevs} DAQ device(s)`);
    devDescriptors.forEach((d, i) => console.log(`  [${i}] ${d.productName}: (${d.uniqueId})`));

    const descriptorIndex = (numDevs > 1) ? await selectDAQDevice(numDevs) : 0;
    const devDescriptor = devDescriptors[descriptorIndex];

    // Get a handle to the DAQ device associated with the selected descriptor
    daqDeviceHandle = await ul.createDaqDevice(devDescriptor);

    // Verify the specified device supports DAQ input
    if (!getDevInfoHasDaqi(ul, daqDeviceHandle)) {
      console.log('\nThe specified DAQ device does not support DAQ input');
      return;
    }

    console.log(`\nConnecting to device ${devDescriptor.devString} - please wait ...`);

    // Establish a connection to the DAQ device
    await ul.connectDaqDevice(daqDeviceHandle);
    connected = true;

    // Get the channel types supported by the DAQ input subsystem
    const chanTypesMask = getDaqiChannelTypes(ul, daqDeviceHandle);

    if (chanTypesMask === 0) {
      console.log('\nDaqInScan is not supported by the specified DAQ device');
      return;
    }

    const chanDescriptors = [];

    // Configure the analog channels
    if (chanTypesMask & DaqInChanType.DAQI_ANALOG_SE) {
      // Get the first supported analog input mode
      const { inputMode } = getAiInfoFirstSupportedInputMode(ul, daqDeviceHandle);

      // Get the first supported input range
      const { range } = getAiInfoFirstSupportedRange(ul, daqDeviceHandle, inputMode);

      configureAnalogInputChannels(2, range, chanDescriptors);
    }

    // Configure the digital channels
    if (chanTypesMask & DaqInChanType.DAQI_DIGITAL) {
      await configureDigitalInputChannel(ul, daqDeviceHandle, chanDescriptors);
    }

    // Configure the counter channels
    if (chanTypesMask & DaqInChanType.DAQI_CTR32) {
      configureCounterInputChannels(1, chanDescriptors);
    }

    const numberOfScanChannels = chanDescriptors.length;

    // Allocate a buffer to receive the data
    const buffer = new Float64Array(numberOfScanChannels * samplesPerChannel);

    console.log(`\n${devDescriptor.devString} ready`);
    console.log('    Function demonstrated: daqInScan()');
    console.log(`    Number of scan channels: ${numberOfScanChannels}`);
    chanDescriptors.forEach((d, i) => {
      const typeStr = nameOf(DaqInChanType, d.type);
      if (isAnalog(d.type)) {
        console.log(`        ScanChannel ${i}: type = ${typeStr}, channel = ${d.channel}, range = ${nameOf(Range, d.range)}`);
      } else {
        console.log(`        ScanChannel ${i}: type = ${typeStr}, channel = ${d.channel}`);
      }
    });
    console.log(`    Samples per channel: ${samplesPerChannel}`);
    console.log(`    Rate: ${rate.toFixed(6)}`);
    console.log(`    Scan options: ${convertScanOptionsToString(scanOptions)}`);
    console.log('\nHit ENTER to continue');

    await waitForEnter();

    console.clear();

    // Start the acquisition
    rate = await ul.daqInScan(daqDeviceHandle, chanDescriptors, samplesPerChannel, rate,
      scanOptions, flags, buffer);
    scanRunning = true;

    // Get the initial status of the acquisition
    let { status } = ul.daqInScanStatus(daqDeviceHandle);

    const enterPressed = watchForEnter();

    while (status === ScanStatus.SS_RUNNING && !enterPressed()) {
      // Get the current status of the acquisition
      const transferStatus = ul.daqInScanStatus(daqDeviceHandle);
      status = transferStatus.status;

      // Reset the cursor to the top of the display and show the termination
      // message
      resetCursor();
      console.log("Hit 'Enter' to terminate the process\n");
      console.log(`Active DAQ device: ${devDescriptor.productName} (${devDescriptor.uniqueId})\n`);
      console.log(`actual scan rate = ${rate.toFixed(6)}\n`);

      const index = transferStatus.currentIndex;
      console.log(`currentScanCount =  ${String(transferStatus.currentScanCount).padEnd(10)} `);
      console.log(`currentTotalCount = ${String(transferStatus.currentTotalCount).padEnd(10)} `);
      console.log(`currentIndex =      ${String(index).padEnd(10)} \n`);

      if (index >= 0) {
        // Display the data
        chanDescriptors.forEach((d, i) => {
          const value = buffer[index + i];
          if (isAnalog(d.type)) {
            console.log(`chan (Ai${d.channel}) = ${formatSigned(value)}`);
          } else {
            const prefix = (d.type === DaqInChanType.DAQI_DIGITAL) ? 'Di' : 'Ci';
            console.log(`chan (${prefix}${d.channel}) = ${String(Math.trunc(value)).padEnd(10)} `);
          }
        });
      }

      await sleep(100);
    }

    // Stop the acquisition if it is still running
    if (status === ScanStatus.SS_RUNNING) {
      await ul.daqInScanStop(daqDeviceHandle);
    }
    scanRunning = false;
  } catch (err) {
    // UL errors carry the numeric UlError value in err.code
    if (err.code !== undefined && /^\d+$/.test(err.code)) {
      console.log(`Error Code: ${err.code}`);
      console.log(`Error Message: ${err.message}`);
    } else {
      console.log(err);
    }
  } finally {
    // Make sure the scan is stopped if an error interrupted it
    if (scanRunning) {
      await ul.daqInScanStop(daqDeviceHandle).catch(() => {});
    }

    // Disconnect from the DAQ device
    if (connected) {
      await ul.disconnectDaqDevice(daqDeviceHandle);
    }

    // Release the handle to the DAQ device
    if (daqDeviceHandle) {
      await ul.releaseDaqDevice(daqDeviceHandle);
    }

    closeInput();
  }
}

main();
