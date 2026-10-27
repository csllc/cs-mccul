// Port of "AIn.c" from examples

const MccUl = require("..");


let ul = new MccUl();

ul.getDaqDeviceInventory(ul.const.DaqDeviceInterface.USB_IFC)
.then((devDescriptors) => {
  console.log(devDescriptors);
});
