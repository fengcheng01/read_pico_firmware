// SPDX-License-Identifier: Apache-2.0
// 注入串口边界故障，运行真实控制器；不打开任何设备。/ Inject serial-boundary faults into the real controller without opening devices.
import assert from 'node:assert/strict';
import { registerHooks } from 'node:module';
import { picoRestartTransport } from '../src/utils/reset.ts';
let soft = 0, hard = 0;
const signals = [];
const loader = { async flashBegin(){ ++soft; }, async flashFinish(){} };
const transport = { async setDTR(state){ ++hard; signals.push(["DTR",state]); }, async setRTS(state){ ++hard; signals.push(["RTS",state]); } };
assert.equal(await picoRestartTransport(loader, transport, async(ms)=>{signals.push(['pause',ms]);}), 'hard');
assert.deepEqual(signals, [['DTR',false],['RTS',true],['pause',200],['RTS',false],['pause',200]]);
assert.equal(hard, 3, 'A soft ACK without a reboot must still send the mandatory board-reset pulse');
loader.flashBegin = async()=>{throw new Error('gone');};
assert.equal(await picoRestartTransport(loader, transport, async()=>{}), 'hard');
assert.equal(hard, 6);
transport.setDTR = async()=>{throw new Error('gone');};
assert.equal(await picoRestartTransport(loader, transport, async()=>{}), 'unconfirmed');
const mock = `export class Transport {
 constructor(){globalThis.serialTest.transports++;}
 async disconnect(){globalThis.serialTest.disconnects++;}
 async setDTR(){if(globalThis.serialTest.fail==='reset')throw new Error('USB gone');}
 async setRTS(state){if(globalThis.serialTest.fail==='reset')throw new Error('USB gone');if(state)globalThis.serialTest.asserted=true;else if(globalThis.serialTest.asserted)globalThis.serialTest.rebooted=true;}
}
export class ESPLoader {
 async main(){if(globalThis.serialTest.fail==='connect')throw new DOMException('An unknown system error has occurred.','NetworkError');return 'ESP32-S3';}
 async readFlash(){return globalThis.serialTest.deviceHead||new Uint8Array(256);}
 async writeFlash(options){globalThis.serialTest.writes++;globalThis.serialTest.plan=options.fileArray.map(f=>({address:f.address,size:f.data.length}));options.reportProgress(0,50,100);if(globalThis.serialTest.fail==='write')throw new Error('write failed');}
 async flashBegin(){if(globalThis.serialTest.fail==='reset')throw new Error('USB gone');}
 async flashFinish(){}
}`;
registerHooks({resolve(specifier, context, next){
 if(specifier==='esptool-js')return {url:'data:text/javascript,'+encodeURIComponent(mock),shortCircuit:true};
 if(specifier==='./reset'&&context.parentURL.endsWith('/flasher.ts'))return {url:new URL('../src/utils/reset.ts',import.meta.url).href,shortCircuit:true};
 if(specifier==='./firmware'&&context.parentURL.endsWith('/flasher.ts'))return {url:new URL('../src/utils/firmware.ts',import.meta.url).href,shortCircuit:true};
 return next(specifier,context);
}});
const { PicoFlasher } = await import('../src/utils/flasher.ts');
Object.defineProperty(globalThis,'navigator',{configurable:true,value:{serial:{async requestPort(){
 globalThis.serialTest.choices++;
 if(globalThis.serialTest.fail==='choose')throw new DOMException('cancelled','NotFoundError');
 return {};
}}}});
for(const fail of ['choose','connect','write','reset','none']){
 globalThis.serialTest={fail,choices:0,transports:0,disconnects:0,writes:0};
 const states=[],logs=[];let selected=0;
 const f=new PicoFlasher({onProgress:p=>states.push(p),onLog:l=>logs.push(l),onPortSelected:()=>selected++});
 const success=await f.flashFiles([{data:new Uint8Array(100),address:0x10000,name:'incremental.bin'}]);
 assert.equal(serialTest.choices,1,'Cancellation must not reopen the serial chooser');
 assert.equal(selected,fail==='choose'?0:1,'The selected-port callback fires once and never on chooser cancellation');
 assert.equal(success,fail==='reset'||fail==='none');
 const last=states.at(-1);
 if(fail==='choose')assert.match(last.error,/选择串口.*NotFoundError/);
 if(fail==='connect')assert.match(last.error,/握手.*NetworkError.*unknown system/);
 if(fail==='write'){assert.match(last.error,/应用写入.*write failed/);assert.equal(last.writtenBytes,50);assert.equal(serialTest.writes,1);}
 if(fail==='reset'){assert.equal(last.state,'done');assert.match(last.message,/未确认复位成功/);assert(!logs.some(line=>line==='软复位命令执行成功'));assert(logs.some(line=>line.includes('复位未确认')));}
 if(fail==='none'){assert.equal(last.state,'done');assert.match(last.message,/已执行硬件复位脉冲/);assert(serialTest.rebooted,'The reboot is modeled by RTS, never by the flash command ACK');}
}

// 完整包增量回退：识别为本固件→仅 0x10000 应用分区；未识别→整包 0x0。
// Full-image upgrade fallback: an identified device takes the app-only 0x10000 write; an unidentified one the whole 0x0 image.
{
 const head = new Uint8Array(256); head[0] = 0xe9; const dv = new DataView(head.buffer);
 dv.setUint32(32, 0xabcd5432, true);
 new TextEncoder().encodeInto('0.6.21\0', head.subarray(32 + 16));
 new TextEncoder().encodeInto('Read_Pico\0', head.subarray(32 + 48));
 for(const identified of [true, false]) {
  globalThis.serialTest = {fail:'none',choices:0,transports:0,disconnects:0,writes:0,deviceHead:identified?head:new Uint8Array(256)};
  const logs=[];
  const f = new PicoFlasher({onProgress:()=>{},onLog:l=>logs.push(l)});
  const fullImage = new Uint8Array(0x11000); fullImage[0] = 0xe9;
  const app = new Uint8Array(200); app[0] = 0xe9;
  const ok = await f.flashFiles([{data:fullImage,address:0,name:'full.bin'}], false, {app, name:'full.bin'});
  assert(ok, 'The fallback path must flash successfully');
  assert.equal(serialTest.writes, 1);
  assert.deepEqual(serialTest.plan, [{address:identified?0x10000:0,size:identified?200:fullImage.length}]);
  assert(logs.some(l=>l.includes(identified?'0x10000':'0x0')), 'The log must state the chosen write target');
 }
}
console.log('Upgrade fallback: identified device takes the app-only write, unidentified keeps the full image PASS');

// 用户明确选择保留数据时，设备识别失败必须停止，不能回退为完整写入。
// An explicit data-preserving choice must fail closed when device identification fails.
{
 globalThis.serialTest = {fail:'none',choices:0,transports:0,disconnects:0,writes:0,deviceHead:new Uint8Array(256)};
 const states=[];
 const f = new PicoFlasher({onProgress:p=>states.push(p),onLog:()=>{}});
 const ok = await f.flashFiles([{data:new Uint8Array(0x11000),address:0,name:'full.bin'}], false, {app:new Uint8Array(200),name:'full.bin',requireMatch:true});
 assert.equal(ok,false);
 assert.equal(serialTest.writes,0,'A failed device match must never write the full image');
 assert.match(states.at(-1).error,/已停止保留数据升级/);
}
console.log('Explicit data-preserving mode: unidentified devices stop before any write PASS');

console.log('Production web flasher: chooser cancellation, handshake/write stage errors, preserved progress, no write retries, reset fallback and completed-write status PASS');
