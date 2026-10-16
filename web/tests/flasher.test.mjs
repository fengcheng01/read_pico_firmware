// SPDX-License-Identifier: Apache-2.0
// 注入串口边界故障，运行真实控制器；不打开任何设备。/ Inject serial-boundary faults into the real controller without opening devices.
import assert from 'node:assert/strict';
import { registerHooks } from 'node:module';
import { picoRestartTransport } from '../src/utils/reset.ts';
let soft = 0, hard = 0;
const loader = { async flashBegin(){ ++soft; }, async flashFinish(){} };
const transport = { async setDTR(){ ++hard; }, async setRTS(){ ++hard; } };
assert.equal(await picoRestartTransport(loader, transport, async()=>{}), 'soft');
assert.equal(hard, 0, 'Successful soft reset must not signal a stale USB port');
loader.flashBegin = async()=>{throw new Error('gone');};
assert.equal(await picoRestartTransport(loader, transport, async()=>{}), 'hard');
assert.equal(hard, 3);
transport.setDTR = async()=>{throw new Error('gone');};
assert.equal(await picoRestartTransport(loader, transport, async()=>{}), 'unconfirmed');
const mock = `export class Transport {
 constructor(){globalThis.serialTest.transports++;}
 async disconnect(){globalThis.serialTest.disconnects++;}
 async setDTR(){throw new Error('USB gone');}
 async setRTS(){throw new Error('USB gone');}
}
export class ESPLoader {
 async main(){if(globalThis.serialTest.fail==='connect')throw new DOMException('An unknown system error has occurred.','NetworkError');return 'ESP32-S3';}
 async writeFlash(options){globalThis.serialTest.writes++;options.reportProgress(0,50,100);if(globalThis.serialTest.fail==='write')throw new Error('write failed');}
 async flashBegin(){if(globalThis.serialTest.fail==='reset')throw new Error('USB gone');}
 async flashFinish(){}
}`;
registerHooks({resolve(specifier, context, next){
 if(specifier==='esptool-js')return {url:'data:text/javascript,'+encodeURIComponent(mock),shortCircuit:true};
 if(specifier==='./reset'&&context.parentURL.endsWith('/flasher.ts'))return {url:new URL('../src/utils/reset.ts',import.meta.url).href,shortCircuit:true};
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
 const states=[],logs=[];
 const f=new PicoFlasher({onProgress:p=>states.push(p),onLog:l=>logs.push(l)});
 const success=await f.flashFiles([{data:new Uint8Array(100),address:0x10000,name:'incremental.bin'}]);
 assert.equal(serialTest.choices,1,'Cancellation must not reopen the serial chooser');
 assert.equal(success,fail==='reset'||fail==='none');
 const last=states.at(-1);
 if(fail==='choose')assert.match(last.error,/选择串口.*NotFoundError/);
 if(fail==='connect')assert.match(last.error,/握手.*NetworkError.*unknown system/);
 if(fail==='write'){assert.match(last.error,/应用写入.*write failed/);assert.equal(last.writtenBytes,50);assert.equal(serialTest.writes,1);}
 if(fail==='reset'){assert.equal(last.state,'done');assert.match(last.message,/未确认复位成功/);assert(!logs.some(line=>line==='软复位命令执行成功'));assert(logs.some(line=>line.includes('复位未确认')));}
 if(fail==='none'){assert.equal(last.state,'done');assert.match(last.message,/已发送重启指令/);}
}
console.log('Production web flasher: chooser cancellation, handshake/write stage errors, preserved progress, no write retries, reset fallback and completed-write status PASS');
