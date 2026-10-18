// SPDX-License-Identifier: Apache-2.0
// 中文：运行真实组件逻辑，仅替换React调度、文件读取与串口边界。/ English: Run component logic with hook scheduling, file I/O and serial boundaries mocked.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { registerHooks } from 'node:module';
import { transformWithOxc } from 'vite';
const componentURL=new URL('../src/components/FlasherSection.tsx',import.meta.url).href;
const compiled=await transformWithOxc(readFileSync(new URL(componentURL),'utf8'),'FlasherSection.tsx',{jsx:{runtime:'automatic'}});
let hooks=[],cursor=0,effects=[],pendingEffects=[],tree;
globalThis.uiHooks={
 useState(initial){const i=cursor++;if(!(i in hooks))hooks[i]=initial;return[hooks[i],v=>{hooks[i]=typeof v==='function'?v(hooks[i]):v;}];},
 useRef(initial){const i=cursor++;if(!(i in hooks))hooks[i]={current:initial};return hooks[i];},
 useEffect(fn,deps){const i=cursor++;const prev=effects[i];if(!prev||deps.some((v,j)=>v!==prev.deps[j]))pendingEffects.push(()=>{prev?.cleanup?.();effects[i]={deps,cleanup:fn()};});}
};
const asModule=source=>'data:text/javascript,'+encodeURIComponent(source);
const icons=['AlertCircle','ArrowRight','Check','CheckCircle2','FileCheck2','LoaderCircle','Terminal','Upload','Usb','X'];
let calls=[];globalThis.flashBoundary=async(files,erase,upgrade,opts)=>{calls.push({files,erase,upgrade});opts.onProgress({state:'writing',percent:30,writtenBytes:20,totalBytes:100,message:'test writing'});await new Promise(resolve=>globalThis.finishFlash=resolve);opts.onProgress({state:'done',percent:100,writtenBytes:100,totalBytes:100,message:'test done'});};
registerHooks({resolve(id,context,next){
 if(id==='react')return{url:asModule('export const {useState,useRef,useEffect}=globalThis.uiHooks;'),shortCircuit:true};
 if(id==='react/jsx-runtime')return{url:asModule('export const jsx=(type,props)=>({type,props});export const jsxs=jsx;'),shortCircuit:true};
 if(id==='lucide-react')return{url:asModule(icons.map(n=>`export const ${n}='svg';`).join('')),shortCircuit:true};
 if(context.parentURL===componentURL){
  if(id.endsWith('/flasher'))return{url:asModule('export class PicoFlasher{constructor(opts){this.opts=opts;}flashFiles(files,erase,upgrade){return globalThis.flashBoundary(files,erase,upgrade,this.opts);}}'),shortCircuit:true};
  if(id.endsWith('/firmware'))return{url:new URL('../src/utils/firmware.ts',import.meta.url).href,shortCircuit:true};
 }
 return next(id,context);
},load(url,context,next){if(url===componentURL)return{format:'module',source:compiled.code,shortCircuit:true};return next(url,context);}});
const { FlasherSection }=await import(componentURL);
Object.defineProperty(globalThis,'navigator',{value:{serial:{}},configurable:true});globalThis.window={isSecureContext:true};
const readers=[];globalThis.FileReader=class{
 constructor(){readers.push(this);}readAsArrayBuffer(file){this.file=file;}abort(){this.onabort?.();}
 finish(bytes){this.result=bytes.buffer.slice(bytes.byteOffset,bytes.byteOffset+bytes.byteLength);this.onload();}
 fail(){this.error=Error('An unknown system error has occurred.');this.onerror();}
};
const nodes=(node)=>Array.isArray(node)?node.flatMap(n=>nodes(n)):node&&typeof node==='object'?[node,...nodes(node.props?.children)]:[];
const text=node=>typeof node==='string'||typeof node==='number'?String(node):Array.isArray(node)?node.map(text).join(''):text(node?.props?.children||'');
const find=(type,test=()=>true)=>{const n=nodes(tree).find(n=>n.type===type&&test(n));assert(n,`${type} not found`);return n;};
function render(){cursor=0;tree=FlasherSection({lang:'zh'});for(const n of nodes(tree))if(n.props.ref&&!n.props.ref.current)n.props.ref.current={value:'',open:false,focus(){},showModal(){this.open=true;},close(){this.open=false;}};while(pendingEffects.length)pendingEffects.shift()();return tree;}
const input=()=>find('input',n=>n.props.type==='file');
const start=()=>find('button',n=>n.props.className?.includes('flash-start'));
const choose=(files)=>{input().props.onChange({target:{files}});render();};
const version=readFileSync(new URL('../../version.txt',import.meta.url),'utf8').trim();
const full=new Uint8Array(readFileSync(new URL(`../../build/firmware/PageRest-Read_Pico-${version}-full.bin`,import.meta.url)));
const app=new Uint8Array(readFileSync(new URL(`../../build/firmware/PageRest-Read_Pico-${version}-incremental.bin`,import.meta.url)));
const file=(name,bytes)=>({name,size:bytes.length});
render();render();assert(start().props.disabled);assert.equal(nodes(tree).filter(n=>n.type==='input'&&n.props.type==='file').length,1);
choose([file('spoof-full.bin',app)]);readers.at(-1).finish(app);render();assert(!start().props.disabled);assert.match(text(tree),/增量升级/);assert.match(text(tree),/0x10000/);assert.equal(calls.length,0);
start().props.onClick();render();assert(find('dialog').props.ref.current.open);find('button',n=>text(n)==='返回检查').props.onClick();render();assert.equal(calls.length,0,'Cancel never accesses serial');
choose([file('actual-full.bin',full)]);const stale=readers.at(-1);choose([file('actual-app.bin',app)]);readers.at(-1).finish(app);stale.finish(full);render();assert.match(text(tree),/actual-app.bin/);assert.doesNotMatch(text(tree),/连接后先识别设备/);
choose([file('actual-full.bin',full)]);readers.at(-1).finish(full);render();assert.match(text(tree),/识别失败会停止/);assert.match(text(tree),/写入地址0x10000/);assert.equal(nodes(tree).filter(n=>n.type==='input'&&n.props.type==='checkbox').length,0);
start().props.onClick();render();const upgradePromise=find('button',n=>text(n)==='确认并选择串口').props.onClick();render();assert.equal(calls.length,1);assert.equal(calls[0].upgrade.requireMatch,true);assert.equal(calls[0].erase,false);globalThis.finishFlash();await upgradePromise;render();
choose([file('actual-full.bin',full)]);readers.at(-1).finish(full);render();find('input',n=>n.props.type==='radio'&&n.props.value==='fresh').props.onChange();render();assert.match(text(tree),/写入地址0x0/);assert.equal(find('input',n=>n.props.type==='checkbox').props.checked,false);
find('input',n=>n.props.type==='checkbox').props.onChange({target:{checked:true}});render();start().props.onClick();render();assert.match(text(find('dialog')),/内置文件和所有设置将被删除/);
const promise=find('button',n=>text(n)==='确认并选择串口').props.onClick();render();assert.equal(calls.length,2);assert.equal(calls[1].files[0].address,0);assert.equal(calls[1].erase,true);assert.equal(calls[1].upgrade,undefined);assert.deepEqual(calls[1].files[0].data,full);assert(start().props.disabled);
const oldReaders=readers.length;choose([file('cannot-change.bin',app)]);assert.equal(readers.length,oldReaders,'Busy file changes ignored');await find('button',n=>text(n)==='确认并选择串口').props.onClick();assert.equal(calls.length,2,'Double start ignored');globalThis.finishFlash();await promise;render();
choose([file('actual-app.bin',app)]);readers.at(-1).finish(app);render();assert.equal(nodes(tree).filter(n=>n.type==='input'&&n.props.type==='checkbox').length,0,'Erase UI absent for app');
start().props.onClick();render();const appPromise=find('button',n=>text(n)==='确认并选择串口').props.onClick();assert.equal(calls.at(-1).files[0].address,0x10000);assert.equal(calls.at(-1).erase,false);globalThis.finishFlash();await appPromise;render();
choose([file('bad-read.bin',full)]);readers.at(-1).fail();render();assert(start().props.disabled);assert.match(text(tree),/文件读取或校验失败/);
choose([file('corrupt.bin',app)]);const corrupt=app.slice();corrupt[200]^=1;readers.at(-1).finish(corrupt);render();assert(start().props.disabled);assert.match(text(tree),/校验/);
choose([file('a.bin',app),file('b.bin',full)]);assert.match(text(tree),/请选择一个/);assert(start().props.disabled);
choose([{name:'bad.zip',size:1000}]);assert.match(text(tree),/请选择 .bin/);choose([{name:'too-big.bin',size:17*1024*1024}]);assert.match(text(tree),/16 MB/);
console.log('Unified flasher component: single input, real app/full plan, data-impact confirmation, cancel, stale read, read/corruption failure, app erase reset, busy/double-start guards PASS');
