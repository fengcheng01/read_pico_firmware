import { useEffect, useRef, useState } from 'react';
import { AlertCircle, ArrowRight, Check, CheckCircle2, FileCheck2, LoaderCircle, Terminal, Upload, Usb, X } from 'lucide-react';
import type { Lang } from '../utils/i18n';
import { PicoFlasher, type FlashProgress } from '../utils/flasher';
import { planFirmware } from '../utils/firmware';
const copy = {
 zh: {tag:'03 / 固件更新',title:'一个文件，一个入口。',intro:'选择 full 或 incremental BIN，自动识别镜像类型。核对写入计划后，再连接设备。',step1:'选择固件',step2:'核对计划',step3:'连接并烧录',drop:'拖入固件 BIN 文件',browse:'或点击选择本地文件',hint:'支持 full.bin / incremental.bin · 仅适配小纸 Pico RDP-G01-W',reading:'正在读取并检查镜像…',remove:'移除固件',full:'完整安装',app:'增量升级',version:'固件版本',target:'写入地址',bytes:'写入大小',impact:'数据影响',fullImpact:'完整包覆盖设置区，会重置设置与阅读记录；TF 卡文件不受本次写入影响。',appImpact:'只写应用区；已有相同 PageRest 分区时可保留设置与阅读记录。工具不会读取设备现有分区。',ready:'镜像结构校验通过',layout:'请确认当前是相同分区布局的 PageRest。首次安装或布局不明，请选择 full.bin 并先备份。',options:'高级选项',baud:'连接速率',erase:'额外擦除整片 Flash',eraseHelp:'仅完整包可用，默认关闭。会删除内置存储中的文件、设置及阅读记录，请先备份。',connect:'连接设备并开始烧录',busy:'正在烧录…',unsupported:'请使用桌面版 Chrome / Edge，并通过 HTTPS 或 localhost 打开页面。当前浏览器无法使用 Web Serial。',local:'文件仅在浏览器本地检查；点击确认烧录后才请求串口，不上传到服务器。',download:'准备好已下载到本机的 full 或 incremental BIN，再选择文件。',notice:'连接前请确认',notice1:'使用可传数据的 USB 线；如果设备在电脑 U 盘模式，先安全弹出并在设备上退出。',notice2:'烧录时保持连接。完成后核对设备版本；写入成功不等于已验证设备启动。',logs:'查看烧录日志',emptyLog:'尚未连接设备。',confirm:'核对本次烧录',cancel:'返回检查',confirmAction:'确认并选择串口',eraseConfirm:'已选择全片擦除：内置文件和所有设置将被删除。',error:'文件读取或校验失败，请重新选择本地下载的 BIN。',count:'请选择一个 BIN 文件。',extension:'请选择 .bin 固件文件。',size:'固件大小须在 80 字节至 16 MB 之间。',newFile:'已选择新文件，请先核对计划。'},
 en: {tag:'03 / FIRMWARE UPDATE',title:'One file. One place to begin.',intro:'Choose a full or incremental BIN. The image type is detected automatically. Check the plan before connecting.',step1:'Choose firmware',step2:'Check the plan',step3:'Connect & flash',drop:'Drop a firmware BIN here',browse:'or choose a local file',hint:'full.bin / incremental.bin · Read Pico RDP-G01-W only',reading:'Reading and checking the image…',remove:'Remove firmware',full:'Full installation',app:'Incremental upgrade',version:'Firmware version',target:'Write address',bytes:'Write size',impact:'Effect on data',fullImpact:'Full images overwrite settings storage and reset settings and reading records. TF card files are not written by this operation.',appImpact:'Writes the app only. Settings and records remain with a matching PageRest layout. The tool does not read existing device partitions.',ready:'Image structure checked',layout:'Confirm a matching PageRest layout. For a first install or unknown layout, back up first and choose full.bin.',options:'Advanced options',baud:'Connection speed',erase:'Additionally erase all flash',eraseHelp:'Full images only, off by default. Deletes internal files, settings and reading records. Back them up first.',connect:'Connect device & flash',busy:'Flashing…',unsupported:'Use desktop Chrome / Edge over HTTPS or localhost. Web Serial is unavailable in this browser.',local:'Files are inspected locally, never uploaded. Serial access is requested only after you confirm flashing.',download:'Have a full or incremental BIN downloaded locally, then choose the file.',notice:'Before connecting',notice1:'Use a USB data cable. If the device is mounted as a USB disk, safely eject it and exit disk mode on the device first.',notice2:'Keep the cable connected while writing. Check the version on the device afterward; successful writing does not verify a successful boot.',logs:'Flash logs',emptyLog:'No device connected.',confirm:'Check this flash operation',cancel:'Go back',confirmAction:'Confirm & choose serial port',eraseConfirm:'Full erase selected: internal files and all settings will be deleted.',error:'File read or validation failed. Choose a freshly downloaded local BIN.',count:'Choose exactly one BIN file.',extension:'Choose a .bin firmware file.',size:'Firmware must be between 80 bytes and 16 MB.',newFile:'New file selected. Check the plan first.'},
};
const idle: FlashProgress = {state:'idle',percent:0,writtenBytes:0,totalBytes:0,message:''};
type Selection = {file:File; plan:ReturnType<typeof planFirmware>};
// 中文：单入口按文件内容识别，确认前不访问串口；异步读取代次防止旧文件覆盖新选择。
// English: Detect content in one entry, access serial only after confirmation, and reject stale asynchronous reads.
export const FlasherSection = ({lang}:{lang:Lang}) => {
 const t=copy[lang];
 const [selection,setSelection]=useState<Selection|null>(null),[loading,setLoading]=useState(false),[fileError,setFileError]=useState('');
 const [progress,setProgress]=useState<FlashProgress>(idle),[logs,setLogs]=useState<string[]>([]);
 const [baudRate,setBaudRate]=useState(460800),[erase,setErase]=useState(false),[confirm,setConfirm]=useState(false),[supported,setSupported]=useState(false);
 const input=useRef<HTMLInputElement>(null),reader=useRef<FileReader|null>(null),generation=useRef(0),busy=useRef(false),dialog=useRef<HTMLDialogElement>(null),returnFocus=useRef<HTMLButtonElement>(null);
 const isFlashing=['connecting','connected','erasing','writing','resetting'].includes(progress.state);
 useEffect(()=>{setSupported(window.isSecureContext&&'serial' in navigator);return ()=>{++generation.current;reader.current?.abort();};},[]);
 useEffect(()=>{if(confirm&&!dialog.current?.open)dialog.current?.showModal();else if(!confirm&&dialog.current?.open)dialog.current?.close();},[confirm]);
 const appendLog=(line:string)=>setLogs(prev=>[...prev.slice(-299),`[${new Date().toLocaleTimeString()}] ${line}`]);
 const clear=()=>{++generation.current;reader.current?.abort();reader.current=null;setSelection(null);setErase(false);setLoading(false);setFileError('');setProgress(idle);setLogs([]);if(input.current)input.current.value='';};
 const pick=(files:FileList|null)=>{
  if(busy.current||!files?.length)return;
  const file=files[0];clear();
  if(files.length!==1){setFileError(t.count);return;}
  if(!/\.bin$/i.test(file.name)){setFileError(t.extension);return;}
  if(file.size<80||file.size>16*1024*1024){setFileError(t.size);return;}
  const id=generation.current;const r=new FileReader();reader.current=r;setLoading(true);
  r.onload=()=>{if(id!==generation.current)return;setLoading(false);reader.current=null;
   try{if(!(r.result instanceof ArrayBuffer))throw Error(t.error);const plan=planFirmware(new Uint8Array(r.result),file.name);setSelection({file,plan});}
   catch(e){setSelection(null);setFileError(e instanceof Error?e.message:t.error);}
  };
  r.onerror=()=>{if(id!==generation.current)return;setLoading(false);setSelection(null);reader.current=null;setFileError(`${t.error} ${r.error?.message||''}`);};
  r.onabort=()=>{if(id===generation.current){setLoading(false);reader.current=null;}};
  try{r.readAsArrayBuffer(file);}catch(e){setLoading(false);reader.current=null;setFileError(`${t.error} ${e instanceof Error?e.message:''}`);}
 };
 const closeConfirm=()=>{setConfirm(false);returnFocus.current?.focus();};
 const start=async()=>{
  if(busy.current||!selection||!supported)return;
  busy.current=true;closeConfirm();setFileError('');
  try {const plan=planFirmware(selection.plan.file.data,selection.file.name,erase);
   const flasher=new PicoFlasher({baudRate,onProgress:setProgress,onLog:appendLog});
   await flasher.flashFiles([plan.file],plan.eraseAll);
  }catch(e){const message=e instanceof Error?e.message:String(e);setProgress({...idle,state:'error',message,error:message});appendLog(message);}
  finally{busy.current=false;}
 };
 const full=selection?.plan.image.kind==='full';
 const impact=full?t.fullImpact:t.appImpact;
 const planRows=selection&&<dl className="plan-grid"><div><dt>{t.version}</dt><dd>{selection.plan.image.version||'—'}</dd></div><div><dt>{t.target}</dt><dd className="mono">0x{selection.plan.file.address.toString(16).toUpperCase()}</dd></div><div><dt>{t.bytes}</dt><dd className="mono">{(selection.plan.file.data.length/1024/1024).toFixed(2)} MB</dd></div></dl>;
 return <section id="flasher" className="section container flasher-section"><div className="section-heading"><p className="eyebrow">{t.tag}</p><h2>{t.title}</h2><p>{t.intro}</p></div>
 <div className="flasher-layout"><div className="flash-card"><ol className="flash-steps">{[t.step1,t.step2,t.step3].map((s,i)=><li key={s} className={(i===0||selection&&i===1||isFlashing&&i===2)?'reached':''}><span>{i+1}</span>{s}</li>)}</ol>
 <div className={`firmware-drop ${selection?'has-file':''}`} onDragOver={e=>e.preventDefault()} onDrop={e=>{e.preventDefault();pick(e.dataTransfer.files);}} aria-busy={loading}>
 <input ref={input} id="firmware-file" type="file" accept=".bin" disabled={isFlashing||loading} className="visually-hidden" onChange={e=>pick(e.target.files)} aria-label={t.step1}/>
 <label htmlFor="firmware-file" className={isFlashing?'disabled':''}>{loading?<LoaderCircle className="spinner" size={28}/>:selection?<FileCheck2 size={28}/>:<Upload size={28}/>}<strong>{loading?t.reading:selection?selection.file.name:t.drop}</strong><span>{selection?`${(selection.file.size/1024/1024).toFixed(2)} MB · ${full?t.full:t.app}`:t.browse}</span></label>
 {selection&&<button className="file-remove" disabled={isFlashing} aria-label={t.remove} onClick={clear}><X size={18}/></button>}
 </div><p className="fine-print">{t.hint}</p>
 {fileError&&<p className="error-message" role="alert"><AlertCircle size={18}/>{fileError}</p>}
 {selection&&<div className="flash-plan" aria-live="polite"><div className="plan-heading"><span className="verified"><CheckCircle2 size={17}/>{t.ready}</span><span className="plan-kind">{full?t.full:t.app}</span></div>{planRows}<div className={full?'data-impact warning':'data-impact'}><strong>{t.impact}</strong><p>{impact}</p>{!full&&<p>{t.layout}</p>}</div></div>}
 <details className="flash-options"><summary>{t.options}</summary><div><label htmlFor="baud-rate">{t.baud}</label><select id="baud-rate" value={baudRate} disabled={isFlashing} onChange={e=>setBaudRate(Number(e.target.value))}><option value={460800}>460800</option><option value={115200}>115200</option><option value={921600}>921600</option></select></div>{full&&<label className="erase-option"><input type="checkbox" checked={erase} disabled={isFlashing} onChange={e=>setErase(e.target.checked)}/><span>{t.erase}<small>{t.eraseHelp}</small></span></label>}</details>
 {!supported&&<p className="browser-notice"><Usb size={18}/>{t.unsupported}</p>}
 <button className="button primary flash-start" ref={returnFocus} disabled={!selection||loading||isFlashing||!supported} onClick={()=>setConfirm(true)}>{isFlashing?<LoaderCircle size={17} className="spinner"/>:<Usb size={17}/>} {isFlashing?t.busy:t.connect}<ArrowRight size={17}/></button><p className="fine-print">{t.local}</p>
 {progress.state!=='idle'&&<div className={`flash-progress ${progress.state}`} role="status"><div>{progress.state==='done'?<CheckCircle2 size={19}/>:progress.state==='error'?<AlertCircle size={19}/>:<LoaderCircle className="spinner" size={19}/>}<span>{progress.message}</span><strong>{progress.percent}%</strong></div><progress max="100" value={progress.percent} aria-label={t.step3}/><small className="mono">{progress.writtenBytes.toLocaleString()} / {progress.totalBytes.toLocaleString()} bytes</small></div>}
 <details className="flash-logs"><summary><Terminal size={15}/>{t.logs}<span>{logs.length}</span></summary><pre aria-label={t.logs}>{logs.length?logs.join('\n'):t.emptyLog}</pre></details>
 </div><aside className="flash-aside"><span className="aside-icon"><Usb size={25} strokeWidth={1.4}/></span><h3>{t.notice}</h3><p><Check size={16}/>{t.notice1}</p><p><Check size={16}/>{t.notice2}</p><p className="fine-print">{t.download}</p></aside></div>
 <dialog ref={dialog} className="flash-dialog" onCancel={closeConfirm} onClose={()=>{setConfirm(false);returnFocus.current?.focus();}} aria-labelledby="flash-confirm-title"><button className="dialog-close" onClick={closeConfirm} aria-label={t.cancel}><X size={21}/></button><p className="eyebrow">PAGEREST / {full?t.full:t.app}</p><h3 id="flash-confirm-title">{t.confirm}</h3>{selection&&<p className="confirm-filename">{selection.file.name}</p>}{planRows}<div className={full?'data-impact warning':'data-impact'}><p>{impact}</p>{!full&&<p>{t.layout}</p>}{erase&&<p><strong>{t.eraseConfirm}</strong></p>}</div><div className="dialog-actions"><button className="button secondary" onClick={closeConfirm} autoFocus>{t.cancel}</button><button className="button primary" onClick={start}>{t.confirmAction}</button></div></dialog>
 </section>;
};
