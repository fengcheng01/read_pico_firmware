// SPDX-License-Identifier: Apache-2.0
// 真实镜像计划回归，不访问串口。/ Real-image planning regressions without serial access.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { inspectFirmware, prepareFirmware } from '../src/utils/firmware.ts';

const root = resolve(import.meta.dirname, '../..');
const version = readFileSync(resolve(root, 'version.txt'), 'utf8').trim();
const stem = `build/firmware/PageRest-Read_Pico-${version}`;
const full = new Uint8Array(readFileSync(resolve(root, stem + '-full.bin')));
const app = new Uint8Array(readFileSync(resolve(root, stem + '-incremental.bin')));
assert.equal(inspectFirmware(full).kind, 'full');
assert.equal(inspectFirmware(app).kind, 'app');
assert.equal(inspectFirmware(app).version, version);
assert.deepEqual(prepareFirmware(full, 'full.bin', true).data, app);
assert.deepEqual(prepareFirmware(app, 'app.bin', true).data, app);
assert.equal(prepareFirmware(full, 'full.bin', false).address, 0);
assert.equal(prepareFirmware(app, 'app.bin', true).address, 0x10000);
assert.throws(() => prepareFirmware(app, 'app.bin', false), /0x10000/);
assert.throws(() => inspectFirmware(app.slice(0, -1)), /长度|截断/);
const corrupted = app.slice(); corrupted[200] ^= 1;
assert.throws(() => inspectFirmware(corrupted), /校验/);
const wrongChip = app.slice(); wrongChip[12] = 0;
assert.throws(() => inspectFirmware(wrongChip), /ESP32-S3/);
const extra = new Uint8Array(app.length + 4); extra.set(app);
assert.throws(() => inspectFirmware(extra), /额外/);
assert.throws(() => inspectFirmware(new Uint8Array(64)), /大小/);
console.log('Firmware import: actual raw/merged images, same upgrade payload, offsets, wrong-mode rejection, corruption and truncation PASS');
const { planFirmware } = await import('../src/utils/firmware.ts');
assert.equal(planFirmware(full,'misleading-incremental.bin').file.address,0);
assert.equal(planFirmware(app,'misleading-full.bin').file.address,0x10000);
assert.deepEqual(planFirmware(full,'full.bin').file.data,full,'Full plan retains boot/partitions/NVS padding');
assert.equal(planFirmware(full,'full.bin').eraseAll,false);
assert.equal(planFirmware(full,'full.bin',true).eraseAll,true);
assert.throws(()=>planFirmware(app,'app.bin',true),/全片擦除/);
console.log('Unified plan: content overrides name, full/app offsets, whole-full payload and app erase rejection PASS');
