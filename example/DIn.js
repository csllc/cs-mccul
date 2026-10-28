/*
    Port of the uldaq example "DIn.c" to the cs-mccul NodeJS module

    UL call demonstrated:             ulDIn()

    Purpose:                          Reads the value of the first supported
                                      digital port

    Demonstration:                    Displays the value of the first digital
                                      port

    Steps:
    1. Call getDaqDeviceInventory() to get the list of available DAQ devices
    2. Call createDaqDevice() to get a handle for the first DAQ device
    3. Verify the DAQ device has a digital input subsystem
    4. Call connectDaqDevice() to establish a UL connection to the DAQ device
    5. Get the first supported digital port
    6. Call dConfigPort() to configure the port for input
    7. Call dIn() to read a value from the digital port
    8. Display the data for the port
    9. Call disconnectDaqDevice() and releaseDaqDevice() before exiting the process.
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
  getDioInfoFirstSupportedPortType,
  getDioInfoFirstSupportedPortIoType,
} = require('./utility');

async function main() {
  const ul = new MccUl();
  const { DaqDeviceInterface, DigitalPortIoType, DigitalDirection } = ul.const;

  const interfaceType = DaqDeviceInterface.ANY_IFC;

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

    // Verify the device supports digital input
    if (!getDevInfoHasDio(ul, daqDeviceHandle)) {
      console.log('\nThe specified DAQ device does not support digital I/O');
      return;
    }

    console.log(`\nConnecting to device ${devDescriptor.devString} - please wait ...`);

    // Establish a connection to the DAQ device
    await ul.connectDaqDevice(daqDeviceHandle);
    connected = true;

    // Get the first port type (AUXPORT0, FIRSTPORTA, ...)
    const { portType, portTypeStr } = getDioInfoFirstSupportedPortType(ul, daqDeviceHandle);

    // Get the I/O type for the first port
    const { portIoType, portIoTypeStr } = getDioInfoFirstSupportedPortIoType(ul, daqDeviceHandle);

    if (portIoType === DigitalPortIoType.DPIOT_IO || portIoType === DigitalPortIoType.DPIOT_BITIO) {
      // Configure the first port for input
      await ul.dConfigPort(daqDeviceHandle, portType, DigitalDirection.DD_INPUT);
    }

    console.log(`\n${devDescriptor.devString} ready`);
    console.log('    Function demonstrated: dIn()');
    console.log(`    Port: ${portTypeStr}`);
    console.log(`    Port I/O type: ${portIoTypeStr}`);
    console.log('\nHit ENTER to continue');

    await waitForEnter();

    console.clear();

    const enterPressed = watchForEnter();

    while (!enterPressed()) {
      // Read the port
      const data = await ul.dIn(daqDeviceHandle, portType);

      resetCursor();
      console.log("Hit 'Enter' to terminate the process\n");
      console.log(`Active DAQ device: ${devDescriptor.productName} (${devDescriptor.uniqueId})\n`);

      clearEOL();
      console.log(`Data: ${data} (0x${data.toString(16)})`);

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
