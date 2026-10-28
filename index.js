'use strict';

const { Duplex } = require('stream');

const mcculConst = require('./lib/mcculConst');

// Load the native addon: prebuilds/<platform>-<arch>/ if a matching prebuild
// is present, otherwise build/Release (or build/Debug)
const mccul = require('node-gyp-build')(__dirname);

module.exports = class MccUl extends Duplex {
  constructor(options) {

    super({ objectMode: true });

    let me = this;
  }

  getDaqDeviceInventory(interfaceType) {
    return new Promise((resolve, reject) => {

      let devices = mccul.GetDaqDeviceInventory(interfaceType);

      resolve(devices);
      // will resolve with 'devDescriptors', the length of which provides
      // 'numDevs'
    });
  }

  const = mcculConst

};
