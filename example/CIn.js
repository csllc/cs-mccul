/*
    Port of the uldaq example "CIn.c" to the cs-mccul NodeJS module

    UL call demonstrated:             ulCIn()

    Purpose:                          Reads a counter input channel

    Demonstration:                    Displays the event counter input data
                                      for a specified encoder

    Steps:
    1. Call getDaqDeviceInventory() to get the list of available DAQ devices
    2. Call createDaqDevice() to get a handle for the first DAQ device
    3. Verify the DAQ device has a counter input subsystem
    4. Call connectDaqDevice() to establish a UL connection to the DAQ device
    5. Call cClear() to clear the counter (set it to 0)
    6. Call cIn() to read a value from a counter channel
    7. Display the data for the counter channel
    8. Call disconnectDaqDevice() and releaseDaqDevice() before exiting the process
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
  getDevInfoHasCtr,
} = require('./utility');

async function main() {
  const ul = new MccUl();
  const { DaqDeviceInterface } = ul.const;

  const interfaceType = DaqDeviceInterface.ANY_IFC;
  const ctrNumber = 0;

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

    // Verify the specified DAQ device supports counter input
    if (!getDevInfoHasCtr(ul, daqDeviceHandle)) {
      console.log('\nThe specified DAQ device does not support counter input');
      return;
    }

    console.log(`\nConnecting to device ${devDescriptor.devString} - please wait ...`);

    // Establish a connection to the DAQ device
    await ul.connectDaqDevice(daqDeviceHandle);
    connected = true;

    console.log(`\n${devDescriptor.devString} ready`);
    console.log('    Function demonstrated: cIn()');
    console.log(`    Counter: ${ctrNumber}`);
    console.log('\nHit ENTER to continue');

    await waitForEnter();

    console.clear();

    // Clear the counter
    await ul.cClear(daqDeviceHandle, ctrNumber);

    const enterPressed = watchForEnter();

    while (!enterPressed()) {
      // Reset the cursor to the top of the display and show the termination
      // message
      resetCursor();
      console.log("Hit 'Enter' to terminate the process\n");
      console.log(`Active DAQ device: ${devDescriptor.productName} (${devDescriptor.uniqueId})\n`);

      const data = await ul.cIn(daqDeviceHandle, ctrNumber);

      // Display data for the counter channel
      console.log(`Counter(${ctrNumber}) Data: ${data}`);

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
