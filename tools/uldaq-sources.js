'use strict';

// Print the uldaq library's C/C++ source files, one per line, for use by
// binding.gyp ("<!@(node tools/uldaq-sources.js)").
//
// The list is taken from libuldaq_la_SOURCES in the upstream
// vendor/uldaq/src/Makefile.am, so it follows the submodule automatically.
// Paths are printed relative to the package root (where binding.gyp lives).

const fs = require('fs');
const path = require('path');

const root = path.resolve(__dirname, '..');
const srcDir = 'vendor/uldaq/src';
const makefile = path.join(root, srcDir, 'Makefile.am');

let text;
try {
  text = fs.readFileSync(makefile, 'utf8');
} catch (err) {
  console.error(`uldaq-sources: cannot read ${makefile}`);
  console.error('Did you run "git submodule update --init"?');
  process.exit(1);
}

// Join backslash-continued lines, then find the SOURCES assignment
const joined = text.replace(/\\\r?\n/g, ' ');
const match = joined.match(/^\s*libuldaq_la_SOURCES\s*\+?=\s*(.*)$/m);

if (!match) {
  console.error(`uldaq-sources: libuldaq_la_SOURCES not found in ${makefile}`);
  process.exit(1);
}

const sources = match[1]
  .split(/\s+/)
  .filter((f) => /\.(c|cc|cpp)$/.test(f))
  .map((f) => `${srcDir}/${f}`);

console.log(sources.join('\n'));
