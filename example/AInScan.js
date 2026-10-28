/*
    Port of the uldaq example "AInScan.c" to the cs-mccul NodeJS module

    UL call demonstrated:             ulAInScan()

    Purpose:                          Performs a continuous scan of the range
                                      of A/D input channels

    Demonstration:                    Displays the analog input data for the
                                      range of user-specified channels using
                                      the first supported range and input mode

    Steps:
    1. Call getDaqDeviceInventory() to get the list of available DAQ devices
    2. Call createDaqDevice() to get a handle for the first DAQ device
    3. Verify the DAQ device has an analog input subsystem
    4. Verify the analog input subsystem has a hardware pacer
    5. Call connectDaqDevice() to establish a UL connection to the DAQ device
    6. Get the first supported analog range
    7. Call aInScan() to start the scan of A/D input channels
    8. Call aInScanStatus() to check the status of the background operation
    9. Display the data for each channel
    10. Call aInScanStop() to stop the background operation
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
  getAiInfoHasPacer,
  getAiInfoFirstSupportedInputMode,
  getAiInfoFirstSupportedRange,
  convertScanOptionsToString,
  formatSigned,
} = require('./utility');

async function main() {
  const ul = new MccUl();
  const { DaqDeviceInterface, ScanOption, ScanStatus, AInScanFlag } = ul.const;

  const interfaceType = DaqDeviceInterface.ANY_IFC;

  // Set some variables that are used to acquire data
  const lowChan = 0;
  let highChan = 3;
  const samplesPerChannel = 10000;
  let rate = 100;
  const scanOptions = ScanOption.SO_DEFAULTIO | ScanOption.SO_CONTINUOUS;
  const flags = AInScanFlag.AINSCAN_FF_DEFAULT;

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

    // Verify the specified device supports analog input
    if (!ul.getDevInfoHasAi(daqDeviceHandle)) {
      console.log('\nThe specified DAQ device does not support analog input');
      return;
    }

    // Verify the specified device supports hardware pacing for analog input
    if (!getAiInfoHasPacer(ul, daqDeviceHandle)) {
      console.log('\nThe specified DAQ device does not support hardware paced analog input');
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

    const chanCount = highChan - lowChan + 1;

    // Allocate a buffer to receive the data
    const buffer = new Float64Array(chanCount * samplesPerChannel);

    // Get the first supported analog input range
    const { range, rangeStr } = getAiInfoFirstSupportedRange(ul, daqDeviceHandle, inputMode);

    console.log(`\n${devDescriptor.devString} ready`);
    console.log('    Function demonstrated: aInScan()');
    console.log(`    Channels: ${lowChan} - ${highChan}`);
    console.log(`    Input mode: ${inputModeStr}`);
    console.log(`    Range: ${rangeStr}`);
    console.log(`    Samples per channel: ${samplesPerChannel}`);
    console.log(`    Rate: ${rate.toFixed(6)}`);
    console.log(`    Scan options: ${convertScanOptionsToString(scanOptions)}`);
    console.log('\nHit ENTER to continue');

    await waitForEnter();

    // Clear the display
    console.clear();

    // Start the acquisition
    rate = await ul.aInScan(daqDeviceHandle, lowChan, highChan, inputMode, range,
      samplesPerChannel, rate, scanOptions, flags, buffer);
    scanRunning = true;

    // Get the initial status of the acquisition
    let { status } = ul.aInScanStatus(daqDeviceHandle);

    const enterPressed = watchForEnter();

    while (status === ScanStatus.SS_RUNNING && !enterPressed()) {
      // Get the current status of the acquisition
      const transferStatus = ul.aInScanStatus(daqDeviceHandle);
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
          console.log(`chan ${i + lowChan} = ${formatSigned(buffer[index + i])}`);
        }
      }

      await sleep(100);
    }

    // Stop the acquisition if it is still running
    if (status === ScanStatus.SS_RUNNING) {
      await ul.aInScanStop(daqDeviceHandle);
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
      await ul.aInScanStop(daqDeviceHandle).catch(() => {});
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
