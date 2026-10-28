/*
    Port of the uldaq example "AIn.c" to the cs-mccul NodeJS module

    UL call demonstrated:             ulAIn()

    Purpose:                          Reads the user-specified A/D input channels

    Demonstration:                    Displays the analog input data for each of
                                      the user-specified channels using the first
                                      supported range and input mode

    Steps:
    1. Call getDaqDeviceInventory() to get the list of available DAQ devices
    2. Call createDaqDevice() to get a handle for the first DAQ device
    3. Verify the DAQ device has an analog input subsystem
    4. Call connectDaqDevice() to establish a UL connection to the DAQ device
    5. Get the first supported analog range
    6. Call aIn() for each channel specified to read a value from the A/D input channel
    7. Display the data for each channel
    8. Call disconnectDaqDevice() and releaseDaqDevice() before exiting the process.
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
  getAiInfoFirstSupportedInputMode,
  getAiInfoFirstSupportedRange,
} = require('./utility');

async function main() {
  const ul = new MccUl();
  const { DaqDeviceInterface, AInFlag } = ul.const;

  // Set some variables used to acquire data
  const interfaceType = DaqDeviceInterface.ANY_IFC;
  const lowChan = 0;
  let highChan = 3;
  const flags = AInFlag.AIN_FF_DEFAULT;

  let daqDeviceHandle = 0;
  let connected = false;

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

    // Verify the specified DAQ device supports analog input
    if (!ul.getDevInfoHasAi(daqDeviceHandle)) {
      console.log('\nThe specified DAQ device does not support analog input');
      return;
    }

    console.log(`\nConnecting to device ${devDescriptor.devString} - please wait ...`);

    // Establish a connection to the DAQ device
    await ul.connectDaqDevice(daqDeviceHandle);
    connected = true;

    // Get the first supported analog input mode
    const { inputMode, inputModeStr, numberOfChannels } =
      getAiInfoFirstSupportedInputMode(ul, daqDeviceHandle);

    if (highChan >= numberOfChannels) {
      highChan = numberOfChannels - 1;
    }

    // Get the first supported analog input range
    const { range, rangeStr } = getAiInfoFirstSupportedRange(ul, daqDeviceHandle, inputMode);

    console.log(`\n${devDescriptor.devString} ready`);
    console.log('    Function demonstrated: aIn()');
    console.log(`    Channels: ${lowChan} - ${highChan}`);
    console.log(`    Input mode: ${inputModeStr}`);
    console.log(`    Range: ${rangeStr}`);
    console.log('\nHit ENTER to continue');

    await waitForEnter();

    console.clear();

    const enterPressed = watchForEnter();

    while (!enterPressed()) {
      // Reset the cursor to the top of the display and show the termination
      // message
      resetCursor();
      console.log("Hit 'Enter' to terminate the process\n");
      console.log(`Active DAQ device: ${devDescriptor.productName} (${devDescriptor.uniqueId})\n`);

      // Display data for the first 4 analog input channels
      for (let chan = lowChan; chan <= highChan; chan++) {
        const data = await ul.aIn(daqDeviceHandle, chan, inputMode, range, flags);
        const value = (data < 0 ? '' : '+') + data.toFixed(6);
        console.log(`Channel(${chan}) Data: ${value.padEnd(10)}`);
      }

      await sleep(100);
    }
  } catch (err) {
    // UL errors carry the numeric UlError value in err.code
    if (err.code !== undefined && /^\d+$/.test(err.code)) {
      console.log(`Error Code: ${err.code}`);
      console.log(`Error Message: ${err.message}`);
    } else {
      console.log(err);
    }
  } finally {
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
