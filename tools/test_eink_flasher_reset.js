/* SPDX-License-Identifier: Apache-2.0
 * 中文：验证软复位优先、脉冲兜底与失败传播。/ English: Verify soft reset first, pulse fallback and error propagation.
 */
const assert = require('node:assert/strict');
const { picoRestartTransport } = require('./eink_flasher_reset.js');
(async () => {
  const steps = [];
  const loader = {
    flashBegin: async o => steps.push(['flashBegin', o]),
    flashFinish: async r => steps.push(['flashFinish', r]),
  };
  const transport = {
    setDTR: async x => steps.push(['DTR', x]),
    setRTS: async x => steps.push(['RTS', x]),
  };
  const soft = await picoRestartTransport(loader, transport, async ms => steps.push(['wait', ms]));
  assert.equal(soft, true);
  assert.deepEqual(steps, [
    ['flashBegin', 0], ['flashFinish', true],
    ['DTR', false], ['RTS', true], ['wait', 200], ['RTS', false], ['wait', 200],
  ]);
  // 软复位失败仍走脉冲并返回 false。/ Soft failure still pulses and returns false.
  const recoveringLoader = { flashBegin: async () => { throw Error('rom busy'); }, flashFinish: async () => {} };
  const recovered = await picoRestartTransport(recoveringLoader, transport, async () => {});
  assert.equal(recovered, false);
  // 两条路都失败才向上抛。/ Only when both paths fail does it reject.
  const failingLoader = { flashBegin: async () => { throw Error('rom lost'); } };
  const failingTransport = { setDTR: async () => { throw Error('port lost'); } };
  await assert.rejects(picoRestartTransport(failingLoader, failingTransport), /port lost/);
  console.log('flasher soft reset + hard pulse and failure propagation: PASS');
})().catch(error => {console.error(error);process.exit(1);});
