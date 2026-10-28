/*
    Port of the uldaq example "CInScan.c" to the cs-mccul NodeJS module

    UL call demonstrated:             ulCInScan()

    Purpose:                          Performs a continuous scan of the
                                      specified encoder channel

    Demonstration:                    Displays the event counter data for the
                                      specified encoders

    Steps:
    1. Call getDaqDeviceInventory() to get the list of available DAQ devices
    2. Call createDaqDevice() to get a handle for the first DAQ device
    3. Verify the DAQ device has a counter input subsystem
    4. Verify the counter input subsystem has a hardware pacer
    5. Call connectDaqDevice() to establish a UL connection to the DAQ device
    6. Call cInScan() to start the scan
    7. Call cInScanStatus() to check the status of the background operation
    8. Display the data for each counter
    9. Call cInScanStop() to stop the background operation
    10. Call disconnectDaqDevice() and releaseDaqDevice() before exiting the process
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
  getCtrInfoHasPacer,
  getCtrInfoNumberOfChannels,
  convertScanOptionsToString,
} = require('./utility');

async function main() {
  const ul = new MccUl();
  const { DaqDeviceInterface, ScanOption, ScanStatus, CInScanFlag } = ul.const;

  const interfaceType = DaqDeviceInterface.ANY_IFC;

  // Set some variables that are used to acquire data
  const lowCtr = 0;
  let highCtr = 1;
  const samplesPerCounter = 10000;
  let rate = 1000;
  const scanOptions = ScanOption.SO_DEFAULTIO | ScanOption.SO_CONTINUOUS;
  const flags = CInScanFlag.CINSCAN_FF_DEFAULT;

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

    // Verify the specified DAQ device supports counter input
    if (!getDevInfoHasCtr(ul, daqDeviceHandle)) {
      console.log('\nThe specified DAQ device does not support counter input');
      return;
    }

    // Verify the specified DAQ device supports hardware pacing for counter
    // input
    if (!getCtrInfoHasPacer(ul, daqDeviceHandle)) {
      console.log('\nThe specified DAQ device does not support hardware paced counter input');
      return;
    }

    console.log(`\nConnecting to device ${devDescriptor.devString} - please wait ...`);

    // Establish a connection to the DAQ device
    await ul.connectDaqDevice(daqDeviceHandle);
    connected = true;

    // Get the number of supported counters
    const numberOfCounters = getCtrInfoNumberOfChannels(ul, daqDeviceHandle);

    if (highCtr >= numberOfCounters) {
      highCtr = numberOfCounters - 1;
    }

    const chanCount = highCtr - lowCtr + 1;

    // Allocate a buffer to receive the data
    const buffer = new BigUint64Array(chanCount * samplesPerCounter);

    console.log(`\n${devDescriptor.devString} ready`);
    console.log('    Function demonstrated: cInScan()');
    console.log(`    Counter: ${lowCtr} - ${highCtr}`);
    console.log(`    Samples per channel: ${samplesPerCounter}`);
    console.log(`    Rate: ${rate.toFixed(6)}`);
    console.log(`    Scan options: ${convertScanOptionsToString(scanOptions)}`);
    console.log('\nHit ENTER to continue');

    await waitForEnter();

    console.clear();

    // Start the acquisition
    rate = await ul.cInScan(daqDeviceHandle, lowCtr, highCtr, samplesPerCounter, rate,
      scanOptions, flags, buffer);
    scanRunning = true;

    // Get the initial status of the acquisition
    let { status } = ul.cInScanStatus(daqDeviceHandle);

    const enterPressed = watchForEnter();

    while (status === ScanStatus.SS_RUNNING && !enterPressed()) {
      // Get the current status of the acquisition
      const transferStatus = ul.cInScanStatus(daqDeviceHandle);
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
        for (let i = 0; i < chanCount; i++) {
          console.log(`Counter ${i + lowCtr} = ${String(buffer[index + i]).padEnd(20)}`);
        }
      }

      await sleep(100);
    }

    // Stop the acquisition if it is still running
    if (status === ScanStatus.SS_RUNNING) {
      await ul.cInScanStop(daqDeviceHandle);
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
      await ul.cInScanStop(daqDeviceHandle).catch(() => {});
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
