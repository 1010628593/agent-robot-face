const fs = require('fs');
const path = require('path');
const Module = require('module');

// Try requiring the asar directly
let asar;
try {
  asar = require('/Applications/WorkBuddy.app/Contents/Resources/app.asar');
} catch (e) {
  console.error('Cannot require asar directly:', e.message);
  process.exit(0);
}

const allKeys = Object.keys(asar);
console.log('TOTAL_KEYS:', allKeys.length);

const susp = allKeys.filter(k => /hook|emitter|bridge|mcp|event|claude|codex/i.test(k));
console.log('SUSPICIOUS_KEYS_COUNT:', susp.length);
console.log(susp.slice(0, 60));

// Try common metadata
for (const p of ['package.json', 'manifest.json', 'app/package.json']) {
  if (asar[p]) {
    const meta = asar[p];
    console.log('---', p, '---');
    console.log('name:', meta.name);
    console.log('version:', meta.version);
    console.log('main:', meta.main);
    if (meta.dependencies) {
      const dep = Object.keys(meta.dependencies).filter(d => /hook|emitter|mcp|claude|bridge/i.test(d));
      console.log('relevant deps:', dep);
    }
    if (meta.devDependencies) {
      const d = Object.keys(meta.devDependencies).filter(d => /hook|emitter|mcp|claude|bridge/i.test(d));
      console.log('relevant devDeps:', d);
    }
    break;
  }
}