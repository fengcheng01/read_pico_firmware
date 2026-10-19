import assert from 'node:assert/strict';
import { readFile, readdir } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const dist = path.resolve(here, '..', 'dist');
const assets = path.join(dist, 'assets');
const files = await readdir(assets);
const scripts = files.filter((name) => name.endsWith('.js'));

assert.equal(
  scripts.length,
  1,
  `Production must emit one JavaScript bundle so flashing never fetches a chip helper after opening serial; found: ${scripts.join(', ')}`,
);

const html = await readFile(path.join(dist, 'index.html'), 'utf8');
assert.doesNotMatch(html, /esp32s3-[^"']+\.js/);

const bundle = await readFile(path.join(assets, scripts[0]), 'utf8');
assert.match(bundle, /ESP32-S3/);
assert.doesNotMatch(bundle, /import\(["']\.\/esp32s3-/);

console.log(`Production bundle: ${scripts[0]} contains ESP32-S3 helpers with no runtime chip chunk PASS`);
