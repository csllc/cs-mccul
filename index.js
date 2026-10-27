'use strict';

const binary = require('node-pre-gyp');
const path = require('path');

const { Duplex } = require('stream');

const mcculConst = require('./lib/mcculConst');

const mccul_path = binary.find(path.resolve(path.join(__dirname,'./package.json')));
const mccul = require(mccul_path);

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
