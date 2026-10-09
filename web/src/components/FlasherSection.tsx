import React, { useState, useRef } from 'react';
import { translations, type Lang } from '../utils/i18n';
import { PicoFlasher, type FlashProgress, type FlashFileItem } from '../utils/flasher';
import {
  UploadCloud,
  CheckCircle2,
  AlertCircle,
  RefreshCw,
  Terminal,
  Cpu,
  AlertTriangle,
  Layers,
  ShieldAlert,
  Trash2,
  X,
} from 'lucide-react';

interface FlasherSectionProps {
  lang: Lang;
}

type FlashMode = 'modeA' | 'modeB';
type ModeBType = 'merged' | 'trio';

export const FlasherSection: React.FC<FlasherSectionProps> = ({ lang }) => {
  const t = translations[lang].flasher;
  const [mode, setMode] = useState<FlashMode>('modeA');
  const [modeBType, setModeBType] = useState<ModeBType>('merged');
  const [eraseAll, setEraseAll] = useState<boolean>(false);
  const [baudRate, setBaudRate] = useState<number>(460800);

  const [fileA, setFileA] = useState<{ file: File; buffer: Uint8Array } | null>(null);
  const [fileBMerged, setFileBMerged] = useState<{ file: File; buffer: Uint8Array } | null>(null);
  const [fileBBoot, setFileBBoot] = useState<{ file: File; buffer: Uint8Array } | null>(null);
  const [fileBPart, setFileBPart] = useState<{ file: File; buffer: Uint8Array } | null>(null);
  const [fileBApp, setFileBApp] = useState<{ file: File; buffer: Uint8Array } | null>(null);

  const [showConfirmModal, setShowConfirmModal] = useState<boolean>(false);

  const [progress, setProgress] = useState<FlashProgress>({
    state: 'idle',
    percent: 0,
    writtenBytes: 0,
    totalBytes: 0,
    message: '',
  });
  const [logs, setLogs] = useState<string[]>([]);
  const [showLogs, setShowLogs] = useState<boolean>(false);

  const fileInputRefA = useRef<HTMLInputElement>(null);
  const fileInputRefMerged = useRef<HTMLInputElement>(null);
  const fileInputRefBoot = useRef<HTMLInputElement>(null);
  const fileInputRefPart = useRef<HTMLInputElement>(null);
  const fileInputRefApp = useRef<HTMLInputElement>(null);

  const appendLog = (line: string) => {
    setLogs((prev) => [...prev.slice(-300), `[${new Date().toLocaleTimeString()}] ${line}`]);
  };

  const readFileToUint8 = (file: File, callback: (buf: Uint8Array) => void) => {
    const reader = new FileReader();
    reader.onload = () => {
      if (reader.result instanceof ArrayBuffer) {
        callback(new Uint8Array(reader.result));
        appendLog(`已载入文件: ${file.name} (${(file.size / 1024).toFixed(1)} KB)`);
      }
    };
    reader.readAsArrayBuffer(file);
  };

  const getFilesToFlash = (): FlashFileItem[] => {
    if (mode === 'modeA') {
      if (!fileA) return [];
      return [
        {
          name: fileA.file.name,
          address: 0x10000,
          data: fileA.buffer,
        },
      ];
    } else {
      if (modeBType === 'merged') {
        if (!fileBMerged) return [];
        return [
          {
            name: fileBMerged.file.name,
            address: 0x0000,
            data: fileBMerged.buffer,
          },
        ];
      } else {
        const list: FlashFileItem[] = [];
        if (fileBBoot) list.push({ name: fileBBoot.file.name, address: 0x0000, data: fileBBoot.buffer });
        if (fileBPart) list.push({ name: fileBPart.file.name, address: 0x8000, data: fileBPart.buffer });
        if (fileBApp) list.push({ name: fileBApp.file.name, address: 0x10000, data: fileBApp.buffer });
        return list;
      }
    }
  };

  const isReady = () => {
    if (mode === 'modeA') return fileA !== null;
    if (modeBType === 'merged') return fileBMerged !== null;
    return fileBBoot !== null && fileBPart !== null && fileBApp !== null;
  };

  const handleStartFlash = async () => {
    setShowConfirmModal(false);
    const files = getFilesToFlash();
    if (files.length === 0) return;

    const flasher = new PicoFlasher({
      baudRate,
      onProgress: (p) => setProgress(p),
      onLog: (l) => appendLog(l),
    });

    if (!flasher.isSupported()) {
      alert(t.unsupportedBrowser);
      return;
    }

    setShowLogs(true);
    await flasher.flashFiles(files, mode === 'modeB' ? eraseAll : false);
  };

  const isFlashing =
    progress.state === 'connecting' ||
    progress.state === 'connected' ||
    progress.state === 'writing' ||
    progress.state === 'resetting';

  return (
    <section id="flasher" className="py-24 px-4 sm:px-6 lg:px-8 max-w-[1200px] mx-auto">
      <div className="text-center mb-10">
        <span className="text-[12px] font-mono uppercase tracking-[1.5px] text-[#6c6a64] bg-[#efe9de] px-3 py-1 rounded-full border border-[#e6dfd8]">
          {t.tag}
        </span>
        <h2 className="text-3xl sm:text-4xl font-claude-serif text-[#141413] mt-4 mb-3">
          {t.title}
        </h2>
        <p className="text-[#3d3d3a] max-w-2xl mx-auto text-base sm:text-lg">
          {t.subtitle}
        </p>
      </div>

      {/* 模式选择切换 Tab (Claude 风格分段控制器) */}
      <div className="flex flex-col sm:flex-row items-center justify-center gap-1.5 p-1.5 bg-[#efe9de] rounded-[8px] border border-[#e6dfd8] mb-6 max-w-2xl mx-auto">
        <button
          onClick={() => setMode('modeA')}
          disabled={isFlashing}
          className={`w-full sm:w-1/2 py-2.5 px-4 rounded-[6px] text-xs sm:text-sm font-medium transition-all ${
            mode === 'modeA'
              ? 'bg-[#faf9f5] text-[#141413] shadow-xs'
              : 'text-[#6c6a64] hover:text-[#141413]'
          }`}
        >
          {t.tabModeA}
        </button>
        <button
          onClick={() => setMode('modeB')}
          disabled={isFlashing}
          className={`w-full sm:w-1/2 py-2.5 px-4 rounded-[6px] text-xs sm:text-sm font-medium transition-all ${
            mode === 'modeB'
              ? 'bg-[#faf9f5] text-[#141413] shadow-xs'
              : 'text-[#6c6a64] hover:text-[#141413]'
          }`}
        >
          {t.tabModeB}
        </button>
      </div>

      {/* 模式差异与跨固件强提醒 */}
      {mode === 'modeA' ? (
        <div className="mb-8 rounded-[12px] bg-[#fcf5e9] border border-[#edd7b2] p-5 flex items-start gap-3.5 shadow-2xs">
          <AlertTriangle className="w-5 h-5 text-[#cc785c] shrink-0 mt-0.5" />
          <div className="space-y-1 text-xs sm:text-sm text-[#73431f] leading-relaxed">
            <div className="font-semibold text-[#5c3214]">{t.modeADesc}</div>
            <div className="text-[11px] sm:text-xs opacity-95">{t.modeAWarning}</div>
          </div>
        </div>
      ) : (
        <div className="mb-8 rounded-[12px] bg-[#f0f7f5] border border-[#cbe6df] p-5 flex items-start gap-3.5 shadow-2xs">
          <ShieldAlert className="w-5 h-5 text-[#5db8a6] shrink-0 mt-0.5" />
          <div className="space-y-1 text-xs sm:text-sm text-[#1b5e50] leading-relaxed">
            <div className="font-semibold text-[#13493e]">{t.modeBDesc}</div>
            <div className="text-[11px] sm:text-xs opacity-95">{t.modeBWarning}</div>
          </div>
        </div>
      )}

      {/* 烧录操作主体卡片 (Claude 风格白米底卡片 + 8px/12px 圆角) */}
      <div className="rounded-[12px] bg-[#faf9f5] border border-[#e6dfd8] p-6 sm:p-10 shadow-2xs">
        {/* 模式 A：单文件上传 */}
        {mode === 'modeA' && (
          <div>
            <div
              onDragOver={(e) => e.preventDefault()}
              onDrop={(e) => {
                e.preventDefault();
                const f = e.dataTransfer.files?.[0];
                if (f && f.name.endsWith('.bin')) {
                  readFileToUint8(f, (buf) => setFileA({ file: f, buffer: buf }));
                }
              }}
              onClick={() => !isFlashing && fileInputRefA.current?.click()}
              className={`border border-dashed rounded-[8px] p-8 sm:p-12 text-center transition-all cursor-pointer ${
                fileA
                  ? 'border-[#cc785c] bg-[#fcf8f5]'
                  : 'border-[#dcd4c9] hover:border-[#cc785c] bg-[#f5f0e8]'
              } ${isFlashing ? 'pointer-events-none opacity-60' : ''}`}
            >
              <input
                type="file"
                ref={fileInputRefA}
                accept=".bin"
                className="hidden"
                onChange={(e) => {
                  const f = e.target.files?.[0];
                  if (f) readFileToUint8(f, (buf) => setFileA({ file: f, buffer: buf }));
                }}
              />
              <div className="mx-auto w-12 h-12 rounded-[8px] bg-[#faf9f5] border border-[#e6dfd8] flex items-center justify-center text-[#cc785c] mb-4">
                <UploadCloud className="w-6 h-6" />
              </div>
              {fileA ? (
                <div className="flex flex-col items-center">
                  <div className="font-semibold text-base text-[#141413]">{fileA.file.name}</div>
                  <div className="text-xs text-[#6c6a64] mt-1 font-mono">
                    0x10000 · {(fileA.file.size / 1024 / 1024).toFixed(2)} MB
                  </div>
                  <button
                    type="button"
                    disabled={isFlashing}
                    onClick={(e) => {
                      e.stopPropagation();
                      setFileA(null);
                      if (fileInputRefA.current) fileInputRefA.current.value = '';
                    }}
                    className="mt-3 inline-flex items-center gap-1 px-3 py-1 text-xs text-[#c64545] hover:text-[#a93232] hover:bg-[#fce8e8] rounded-[4px] border border-[#edd2d2] transition-colors"
                  >
                    <Trash2 className="w-3.5 h-3.5" />
                    <span>{t.removeFile}</span>
                  </button>
                </div>
              ) : (
                <div>
                  <div className="text-[#141413] font-medium text-sm mb-1">
                    {t.dropText} <span className="underline font-semibold text-[#cc785c]">{t.browseText}</span>
                  </div>
                  <div className="text-xs text-[#8e8b82] font-mono">read_pico.bin (0x10000)</div>
                </div>
              )}
            </div>
          </div>
        )}

        {/* 模式 B：合并包或三件套 */}
        {mode === 'modeB' && (
          <div className="space-y-6">
            <div className="flex items-center gap-5 text-xs font-medium text-[#3d3d3a] pb-2 border-b border-[#e6dfd8]">
              <span className="font-mono text-[#8e8b82]">{t.modeBSelectType}</span>
              <label className="flex items-center gap-1.5 cursor-pointer">
                <input
                  type="radio"
                  name="modeBType"
                  checked={modeBType === 'merged'}
                  onChange={() => setModeBType('merged')}
                  className="accent-[#cc785c]"
                />
                <span>{t.modeBMerged}</span>
              </label>
              <label className="flex items-center gap-1.5 cursor-pointer">
                <input
                  type="radio"
                  name="modeBType"
                  checked={modeBType === 'trio'}
                  onChange={() => setModeBType('trio')}
                  className="accent-[#cc785c]"
                />
                <span>{t.modeBTrio}</span>
              </label>
            </div>

            {modeBType === 'merged' ? (
              <div
                onDragOver={(e) => e.preventDefault()}
                onDrop={(e) => {
                  e.preventDefault();
                  const f = e.dataTransfer.files?.[0];
                  if (f && f.name.endsWith('.bin')) {
                    readFileToUint8(f, (buf) => setFileBMerged({ file: f, buffer: buf }));
                  }
                }}
                onClick={() => !isFlashing && fileInputRefMerged.current?.click()}
                className={`border border-dashed rounded-[8px] p-8 sm:p-12 text-center transition-all cursor-pointer ${
                  fileBMerged
                    ? 'border-[#cc785c] bg-[#fcf8f5]'
                    : 'border-[#dcd4c9] hover:border-[#cc785c] bg-[#f5f0e8]'
                } ${isFlashing ? 'pointer-events-none opacity-60' : ''}`}
              >
                <input
                  type="file"
                  ref={fileInputRefMerged}
                  accept=".bin"
                  className="hidden"
                  onChange={(e) => {
                    const f = e.target.files?.[0];
                    if (f) readFileToUint8(f, (buf) => setFileBMerged({ file: f, buffer: buf }));
                  }}
                />
                <div className="mx-auto w-12 h-12 rounded-[8px] bg-[#faf9f5] border border-[#e6dfd8] flex items-center justify-center text-[#cc785c] mb-4">
                  <Layers className="w-6 h-6" />
                </div>

                {fileBMerged ? (
                  <div className="flex flex-col items-center">
                    <div className="font-semibold text-base text-[#141413]">{fileBMerged.file.name}</div>
                    <div className="text-xs text-[#6c6a64] mt-1 font-mono">
                      全量镜像：0x0000 · {(fileBMerged.file.size / 1024 / 1024).toFixed(2)} MB
                    </div>
                    <button
                      type="button"
                      disabled={isFlashing}
                      onClick={(e) => {
                        e.stopPropagation();
                        setFileBMerged(null);
                        if (fileInputRefMerged.current) fileInputRefMerged.current.value = '';
                      }}
                      className="mt-3 inline-flex items-center gap-1 px-3 py-1 text-xs text-[#c64545] hover:text-[#a93232] hover:bg-[#fce8e8] rounded-[4px] border border-[#edd2d2] transition-colors"
                    >
                      <Trash2 className="w-3.5 h-3.5" />
                      <span>{t.removeFile}</span>
                    </button>
                  </div>
                ) : (
                  <div>
                    <div className="text-[#141413] font-medium text-sm mb-1">
                      {t.dropText} <span className="underline font-semibold text-[#cc785c]">{t.browseText}</span>
                    </div>
                    <div className="text-xs text-[#8e8b82] font-mono">
                      merged-firmware.bin (0x0000)
                    </div>
                  </div>
                )}
              </div>
            ) : (
              <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
                <div
                  onClick={() => !isFlashing && fileInputRefBoot.current?.click()}
                  className={`relative border border-dashed rounded-[8px] p-4 text-center cursor-pointer transition-all ${
                    fileBBoot ? 'border-[#cc785c] bg-[#fcf8f5]' : 'border-[#dcd4c9] bg-[#f5f0e8] hover:border-[#cc785c]'
                  }`}
                >
                  {fileBBoot && !isFlashing && (
                    <button
                      type="button"
                      title={t.removeFile}
                      onClick={(e) => {
                        e.stopPropagation();
                        setFileBBoot(null);
                        if (fileInputRefBoot.current) fileInputRefBoot.current.value = '';
                      }}
                      className="absolute top-2 right-2 p-1 text-[#6c6a64] hover:text-[#c64545] hover:bg-[#fce8e8] rounded-[4px] transition-colors"
                    >
                      <X className="w-3.5 h-3.5" />
                    </button>
                  )}
                  <input
                    type="file"
                    ref={fileInputRefBoot}
                    accept=".bin"
                    className="hidden"
                    onChange={(e) => {
                      const f = e.target.files?.[0];
                      if (f) readFileToUint8(f, (buf) => setFileBBoot({ file: f, buffer: buf }));
                    }}
                  />
                  <div className="text-xs font-mono font-medium text-[#141413] mb-1">{t.bootloaderLabel}</div>
                  {fileBBoot ? (
                    <div className="text-xs text-[#cc785c] font-mono truncate">{fileBBoot.file.name}</div>
                  ) : (
                    <div className="text-[11px] text-[#8e8b82]">bootloader.bin</div>
                  )}
                </div>

                <div
                  onClick={() => !isFlashing && fileInputRefPart.current?.click()}
                  className={`relative border border-dashed rounded-[8px] p-4 text-center cursor-pointer transition-all ${
                    fileBPart ? 'border-[#cc785c] bg-[#fcf8f5]' : 'border-[#dcd4c9] bg-[#f5f0e8] hover:border-[#cc785c]'
                  }`}
                >
                  {fileBPart && !isFlashing && (
                    <button
                      type="button"
                      title={t.removeFile}
                      onClick={(e) => {
                        e.stopPropagation();
                        setFileBPart(null);
                        if (fileInputRefPart.current) fileInputRefPart.current.value = '';
                      }}
                      className="absolute top-2 right-2 p-1 text-[#6c6a64] hover:text-[#c64545] hover:bg-[#fce8e8] rounded-[4px] transition-colors"
                    >
                      <X className="w-3.5 h-3.5" />
                    </button>
                  )}
                  <input
                    type="file"
                    ref={fileInputRefPart}
                    accept=".bin"
                    className="hidden"
                    onChange={(e) => {
                      const f = e.target.files?.[0];
                      if (f) readFileToUint8(f, (buf) => setFileBPart({ file: f, buffer: buf }));
                    }}
                  />
                  <div className="text-xs font-mono font-medium text-[#141413] mb-1">{t.partitionLabel}</div>
                  {fileBPart ? (
                    <div className="text-xs text-[#cc785c] font-mono truncate">{fileBPart.file.name}</div>
                  ) : (
                    <div className="text-[11px] text-[#8e8b82]">partition-table.bin</div>
                  )}
                </div>

                <div
                  onClick={() => !isFlashing && fileInputRefApp.current?.click()}
                  className={`relative border border-dashed rounded-[8px] p-4 text-center cursor-pointer transition-all ${
                    fileBApp ? 'border-[#cc785c] bg-[#fcf8f5]' : 'border-[#dcd4c9] bg-[#f5f0e8] hover:border-[#cc785c]'
                  }`}
                >
                  {fileBApp && !isFlashing && (
                    <button
                      type="button"
                      title={t.removeFile}
                      onClick={(e) => {
                        e.stopPropagation();
                        setFileBApp(null);
                        if (fileInputRefApp.current) fileInputRefApp.current.value = '';
                      }}
                      className="absolute top-2 right-2 p-1 text-[#6c6a64] hover:text-[#c64545] hover:bg-[#fce8e8] rounded-[4px] transition-colors"
                    >
                      <X className="w-3.5 h-3.5" />
                    </button>
                  )}
                  <input
                    type="file"
                    ref={fileInputRefApp}
                    accept=".bin"
                    className="hidden"
                    onChange={(e) => {
                      const f = e.target.files?.[0];
                      if (f) readFileToUint8(f, (buf) => setFileBApp({ file: f, buffer: buf }));
                    }}
                  />
                  <div className="text-xs font-mono font-medium text-[#141413] mb-1">{t.appLabel}</div>
                  {fileBApp ? (
                    <div className="text-xs text-[#cc785c] font-mono truncate">{fileBApp.file.name}</div>
                  ) : (
                    <div className="text-[11px] text-[#8e8b82]">read_pico.bin</div>
                  )}
                </div>
              </div>
            )}

            <div className="pt-2">
              <label className="flex items-center gap-2 cursor-pointer text-xs sm:text-sm text-[#3d3d3a]">
                <input
                  type="checkbox"
                  checked={eraseAll}
                  disabled={isFlashing}
                  onChange={(e) => setEraseAll(e.target.checked)}
                  className="rounded-[4px] accent-[#cc785c]"
                />
                <span className="font-medium">{t.eraseCheckbox}</span>
              </label>
              <div className="text-[11px] text-[#8e8b82] pl-5 mt-0.5">{t.eraseHint}</div>
            </div>
          </div>
        )}

        {/* 控制配置栏 */}
        <div className="mt-8 flex flex-col sm:flex-row items-stretch sm:items-center justify-between gap-4 pt-6 border-t border-[#e6dfd8]">
          <div className="flex items-center gap-3">
            <label className="text-xs font-mono uppercase tracking-wider text-[#6c6a64] flex items-center gap-1.5">
              <Cpu className="w-4 h-4 text-[#cc785c]" /> {t.baudRate}
            </label>
            <select
              value={baudRate}
              disabled={isFlashing}
              onChange={(e) => setBaudRate(Number(e.target.value))}
              className="bg-[#faf9f5] border border-[#e6dfd8] rounded-[6px] px-3 py-1.5 text-xs font-mono text-[#141413] focus:outline-none focus:border-[#cc785c]"
            >
              <option value={921600}>921600 (High Speed)</option>
              <option value={460800}>460800 (Standard)</option>
              <option value={115200}>115200 (Safe)</option>
            </select>
          </div>

          {/* Claude 珊瑚色主按钮 */}
          <button
            onClick={() => setShowConfirmModal(true)}
            disabled={!isReady() || isFlashing}
            className={`px-8 py-3 rounded-[8px] font-medium text-sm transition-colors flex items-center justify-center gap-2 ${
              !isReady() || isFlashing
                ? 'bg-[#e6dfd8] text-[#8e8b82] cursor-not-allowed'
                : 'bg-[#cc785c] hover:bg-[#a9583e] text-[#ffffff] shadow-xs active:scale-[0.99]'
            }`}
          >
            {isFlashing ? (
              <>
                <RefreshCw className="w-4 h-4 animate-spin" />
                {t.flashingBtn}
              </>
            ) : (
              t.startBtn
            )}
          </button>
        </div>

        {/* 烧录进度条 */}
        {progress.state !== 'idle' && (
          <div className="mt-8 p-6 rounded-[8px] bg-[#f5f0e8] border border-[#e6dfd8]">
            <div className="flex justify-between items-center mb-2">
              <div className="text-sm font-medium text-[#141413] flex items-center gap-2">
                {progress.state === 'done' && <CheckCircle2 className="w-4 h-4 text-[#5db872]" />}
                {progress.state === 'error' && <AlertCircle className="w-4 h-4 text-[#c64545]" />}
                {isFlashing && <RefreshCw className="w-4 h-4 text-[#cc785c] animate-spin" />}
                <span>{progress.message}</span>
              </div>
              <div className="font-mono text-sm font-bold text-[#cc785c]">{progress.percent}%</div>
            </div>

            <div className="w-full h-2 bg-[#e6dfd8] rounded-full overflow-hidden">
              <div
                className={`h-full transition-all duration-200 rounded-full ${
                  progress.state === 'error'
                    ? 'bg-[#c64545]'
                    : progress.state === 'done'
                    ? 'bg-[#5db872]'
                    : 'bg-[#cc785c]'
                }`}
                style={{ width: `${progress.percent}%` }}
              />
            </div>
          </div>
        )}

        {/* 串口终端可折叠输出 (Claude 代码窗口卡片规范: #181715, JetBrains Mono, 12px rounded) */}
        <div className="mt-8">
          <button
            onClick={() => setShowLogs(!showLogs)}
            className="flex items-center gap-2 text-xs font-mono text-[#6c6a64] hover:text-[#141413] transition-colors"
          >
            <Terminal className="w-4 h-4 text-[#cc785c]" />
            <span>{t.consoleLogs} ({logs.length})</span>
            <span>{showLogs ? '▲' : '▼'}</span>
          </button>

          {showLogs && (
            <div className="mt-3 p-4 bg-[#181715] text-[#a09d96] font-mono text-xs rounded-[8px] h-48 overflow-y-auto space-y-1 select-text border border-[#252320]">
              {logs.length === 0 ? (
                <div className="text-[#6c6a64]">串口暂无活动记录…</div>
              ) : (
                logs.map((log, index) => <div key={index}>{log}</div>)
              )}
            </div>
          )}
        </div>
      </div>

      {/* 烧录须知 */}
      <div className="mt-8 p-6 rounded-[8px] bg-[#efe9de] border border-[#e6dfd8] text-xs text-[#3d3d3a] space-y-2 leading-relaxed">
        <div className="font-semibold text-[#141413] text-sm mb-1">{t.noticeHeader}</div>
        <p>• {t.notice1}</p>
        <p>• {t.notice2}</p>
        <p>• {t.notice3}</p>
      </div>

      {/* 安全防呆确认弹窗 */}
      {showConfirmModal && (
        <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-black/50 backdrop-blur-xs">
          <div className="bg-[#faf9f5] rounded-[12px] max-w-md w-full p-6 sm:p-8 shadow-xl border border-[#e6dfd8] animate-in fade-in zoom-in-95 duration-150">
            <div className="w-10 h-10 rounded-[8px] bg-[#fcf5e9] border border-[#edd7b2] text-[#cc785c] flex items-center justify-center mb-4">
              <AlertTriangle className="w-5 h-5" />
            </div>

            <h3 className="text-xl font-claude-serif text-[#141413] mb-2">{t.confirmTitle}</h3>
            <p className="text-sm text-[#3d3d3a] leading-relaxed mb-6">
              {mode === 'modeA' ? t.confirmModeAContent : t.confirmModeBContent}
            </p>

            <div className="flex gap-3 justify-end">
              <button
                onClick={() => setShowConfirmModal(false)}
                className="px-4 py-2.5 rounded-[6px] border border-[#e6dfd8] text-xs font-medium text-[#3d3d3a] hover:bg-[#efe9de]"
              >
                {t.cancelBtn}
              </button>
              <button
                onClick={handleStartFlash}
                className="px-4 py-2.5 rounded-[6px] bg-[#cc785c] hover:bg-[#a9583e] text-xs font-medium text-[#ffffff] shadow-2xs"
              >
                {t.confirmBtn}
              </button>
            </div>
          </div>
        </div>
      )}
    </section>
  );
};
