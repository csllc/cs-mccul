/*
    Port of the uldaq example "DInScan.c" to the cs-mccul NodeJS module

    UL call demonstrated:             ulDInScan()

    Purpose:                          Performs a continuous scan of the first
                                      digital port

    Demonstration:                    Displays the digital input data on the
                                      first digital port

    Steps:
    1. Call getDaqDeviceInventory() to get the list of available DAQ devices
    2. Call createDaqDevice() to get a handle for the first DAQ device
    3. Verify the DAQ device has a digital input subsystem
    4. Verify the digital input subsystem has a hardware pacer
    5. Call connectDaqDevice() to establish a UL connection to the DAQ device
    6. Get the first supported digital port
    7. Call dConfigPort() to configure the port for input
    8. Call dInScan() to start the scan
    9. Call dInScanStatus() to check the status of the background operation
    10. Display the data for the port
    11. Call dInScanStop() to stop the background operation
    12. Call disconnectDaqDevice() and releaseDaqDevice() before exiting the process
*/

'use strict';

const MccUl = require('..');
const {
  selectDAQDevice,
  waitForEnter,
  watchForEnter,
  closeInput,
  resetCursor,
  clearEOL,
  sleep,
  getDevInfoHasDio,
  getDioInfoHasPacer,
  getDioInfoFirstSupportedPortType,
  convertScanOptionsToString,
} = require('./utility');

async function main() {
  const ul = new MccUl();
  const { DaqDeviceInterface, DigitalDirection, ScanOption, ScanStatus, DInScanFlag } = ul.const;

  const interfaceType = DaqDeviceInterface.ANY_IFC;

  // Set some variables that are used to acquire data
  const samplesPerPort = 1000;
  let rate = 100;
  const scanOptions = ScanOption.SO_DEFAULTIO | ScanOption.SO_CONTINUOUS;
  const flags = DInScanFlag.DINSCAN_FF_DEFAULT;
  const chanCount = 1;

  let daqDeviceHandle = 0;
  let connected = false;
  let scanStarted = false;

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

    // Verify the specified DAQ device supports digital input
    if (!getDevInfoHasDio(ul, daqDeviceHandle)) {
      console.log('\nThe specified DAQ device does not support digital I/O');
      return;
    }

    // Verify the device supports hardware pacing
    if (!getDioInfoHasPacer(ul, daqDeviceHandle, DigitalDirection.DD_INPUT)) {
      console.log('\nThe specified DAQ device does not support hardware paced digital input');
      return;
    }

    console.log(`\nConnecting to device ${devDescriptor.devString} - please wait ...`);

    // Establish a connection to the DAQ device
    await ul.connectDaqDevice(daqDeviceHandle);
    connected = true;

    const { portType, portTypeStr } = getDioInfoFirstSupportedPortType(ul, daqDeviceHandle);
    const lowPort = portType;
    const highPort = portType;

    await ul.dConfigPort(daqDeviceHandle, portType, DigitalDirection.DD_INPUT);

    // Allocate a buffer to receive the data
    const buffer = new BigUint64Array(chanCount * samplesPerPort);

    console.log(`\n${devDescriptor.devString} ready`);
    console.log('    Function demonstrated: dInScan()');
    console.log(`    Port: ${portTypeStr}`);
    console.log(`    Samples per port: ${samplesPerPort}`);
    console.log(`    Rate: ${rate.toFixed(6)}`);
    console.log(`    Scan options: ${convertScanOptionsToString(scanOptions)}`);
    console.log('\nHit ENTER to continue');

    await waitForEnter();

    console.clear();

    rate = await ul.dInScan(daqDeviceHandle, lowPort, highPort, samplesPerPort, rate,
      scanOptions, flags, buffer);
    scanStarted = true;

    let { status } = ul.dInScanStatus(daqDeviceHandle);

    const enterPressed = watchForEnter();

    while (status === ScanStatus.SS_RUNNING && !enterPressed()) {
      const transferStatus = ul.dInScanStatus(daqDeviceHandle);
      status = transferStatus.status;

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
          clearEOL();
          console.log(`chan ${i + lowPort} = 0x${buffer[index + i].toString(16)}`);
        }
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
    // Stop the scan (DInScan.c always does this)
    if (scanStarted) {
      await ul.dInScanStop(daqDeviceHandle).catch(() => {});
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
