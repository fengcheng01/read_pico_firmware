/* SPDX-License-Identifier: Apache-2.0
 * Web Serial 固件烧录器 (基于 esptool-js + 小纸 Pico 专有复位时序)
 * 中文：页面统一识别full/app，控制器按已校验计划写入；不自动重试部分写入。
 * English: The page identifies full/app images; this controller writes a validated plan without retrying partial writes.
 */

import { ESPLoader, Transport } from 'esptool-js';
import { picoRestartTransport, type EspLoaderLike, type TransportLike } from './reset';
import { identifyPicoApp } from './firmware';

export type FlashState =
  | 'idle'
  | 'connecting'
  | 'connected'
  | 'erasing'
  | 'writing'
  | 'resetting'
  | 'done'
  | 'error';

export interface FlashProgress {
  state: FlashState;
  percent: number;
  writtenBytes: number;
  totalBytes: number;
  message: string;
  error?: string;
}

export interface FlasherOptions {
  baudRate?: number;
  onProgress?: (progress: FlashProgress) => void;
  onLog?: (line: string) => void;
}

export interface FlashFileItem {
  data: Uint8Array;
  address: number;
  name: string;
}

export class PicoFlasher {
  private transport: Transport | null = null;
  private esploader: ESPLoader | null = null;
  private baudRate: number;
  private onProgress?: (progress: FlashProgress) => void;
  private onLog?: (line: string) => void;

  constructor(options: FlasherOptions = {}) {
    this.baudRate = options.baudRate || 460800;
    this.onProgress = options.onProgress;
    this.onLog = options.onLog;
  }

  private updateState(state: FlashState, message: string, percent = 0, writtenBytes = 0, totalBytes = 0, error?: string) {
    this.onProgress?.({
      state,
      percent,
      writtenBytes,
      totalBytes,
      message,
      error,
    });
  }

  public isSupported(): boolean {
    return typeof navigator !== 'undefined' && 'serial' in navigator;
  }

  /// 中文：读取设备 0x10000 应用描述并识别本固件；返回版本号，失败/非本固件返回 null。
  /// English: Read the on-device app description at 0x10000 and identify this firmware;
  /// returns the version, or null on failure or a foreign image.
  private async probePicoApp(): Promise<string | null> {
    if (!this.esploader) return null;
    try {
      return identifyPicoApp(new Uint8Array(await this.esploader.readFlash(0x10000, 0x100)));
    } catch (e: unknown) {
      this.onLog?.(`设备识别读取失败，按完整安装处理：${e instanceof Error ? e.message : String(e)}`);
      return null;
    }
  }

  /**
   * 烧录文件列表
   * @param files 待写入分区数组
   * @param eraseAll 是否全片擦除，仅完整包显式选择。/ Explicit full-image erase only.
   * @param upgrade 完整包提供的增量回退：连接后识别设备已运行本固件时，只写应用分区保留数据。
   * Full-image upgrade fallback: after connect, a device already running this firmware
   * switches the write to the app partition only, preserving data.
   */
  public async flashFiles(files: FlashFileItem[], eraseAll = false, upgrade?: { app: Uint8Array; name: string }): Promise<boolean> {
    if (!this.isSupported()) {
      throw new Error('当前浏览器不支持 Web Serial API，请使用 Chrome、Edge 或其他 Chromium 内核浏览器。');
    }

    if (files.length === 0) {
      throw new Error('未指定待烧录文件。');
    }

    let stage = '选择串口';
    let lastPercent = 0, lastWritten = 0, totalSize = 0;
    try {
      this.updateState('connecting', '请求连接设备串口…', 0);
      this.onLog?.('正在请求打开串口…');

      const device = await navigator.serial.requestPort({
        filters: [{ usbVendorId: 0x303a }],
      });
      stage = '串口打开与芯片握手';

      this.onLog?.('串口已选定，正在初始化 Transport…');
      this.transport = new Transport(device, true);

      const terminal = {
        clean: () => {},
        writeLine: (data: string) => this.onLog?.(data),
        write: (data: string) => this.onLog?.(data),
      };

      this.esploader = new ESPLoader({
        transport: this.transport,
        baudrate: this.baudRate,
        terminal: terminal,
        romBaudrate: 115200,
      });

      this.updateState('connecting', '正在握手并连接 ESP32-S3…', 5);
      const chip = await this.esploader.main();
      this.onLog?.(`设备握手成功: ${chip}`);

      // 完整包先识别设备：已运行本固件则改写为应用分区增量升级，保留设置与记录。
      // Full images probe first: a device already running this firmware upgrades in
      // place at the app partition, keeping settings and records.
      let effective = files;
      if (upgrade && !eraseAll) {
        this.updateState('connected', '正在识别设备固件…', 8);
        const deviceVersion = await this.probePicoApp();
        if (deviceVersion) {
          this.onLog?.(`识别到本机固件 Read_Pico ${deviceVersion}：改为保留数据增量升级，仅写 0x10000 应用分区。`);
          effective = [{ address: 0x10000, name: `${upgrade.name}（应用分区）`, data: upgrade.app }];
        } else {
          this.onLog?.('未识别到本机固件（空片/其他固件/读取失败）：按完整安装写入 0x0，设置与阅读记录将重置。');
        }
      }

      totalSize = effective.reduce((acc, f) => acc + f.data.byteLength, 0);
      this.onLog?.(`准备烧录 ${effective.length} 个分区，总计 ${(totalSize / 1024 / 1024).toFixed(2)} MB`);

      for (const item of effective) {
        this.onLog?.(`- [0x${item.address.toString(16).toUpperCase()}] ${item.name} (${(item.data.byteLength / 1024).toFixed(1)} KB)`);
      }

      this.updateState('connected', `已连接到芯片 (${chip})，正在准备写入…`, 10);

      // 写入过程中不自动重试，保留失败阶段。/ Never retry a partial write automatically; retain its failure stage.
      stage = eraseAll ? '擦除与写入' : '应用写入';
      this.updateState('writing', '正在写入固件…', 15, 0, totalSize);

      await this.esploader.writeFlash({
        fileArray: effective.map((f) => ({ data: f.data, address: f.address })),
        flashSize: 'keep',
        flashMode: 'keep',
        flashFreq: 'keep',
        eraseAll: eraseAll,
        compress: true,
        reportProgress: (fileIndex: number, written: number, total: number) => {
          const currentFile = effective[fileIndex];
          const filePct = total > 0 ? (written / total) * 100 : 0;
          const globalPct = Math.min(95, Math.floor(15 + (fileIndex / files.length) * 80 + ((total > 0 ? written / total : 0) / files.length) * 80));
          lastPercent = globalPct;
          lastWritten = effective.slice(0, fileIndex).reduce((n, f) => n + f.data.byteLength, 0) + written;
          this.updateState(
            'writing',
            `写入 [${currentFile ? currentFile.name : `分区 ${fileIndex}`}] : ${filePct.toFixed(1)}% (${(written / 1024).toFixed(0)} KB / ${(total / 1024).toFixed(0)} KB)`,
            globalPct,
            written,
            total
          );
        },
      });

      stage = '重启';
      this.onLog?.('Flash 写入完成，正在退出下载模式并发送小纸复位时序…');
      this.updateState('resetting', '正在退出下载模式并发送复位指令…', 96);

      const resetSent = await picoRestartTransport(
        this.esploader as unknown as EspLoaderLike,
        this.transport as unknown as TransportLike,
        undefined,
        {
          log: (m) => this.onLog?.(m),
          error: (m) => this.onLog?.(`[ERR] ${m}`),
        }
      );

      this.onLog?.(resetSent === 'hard' ? '已执行 RTS 硬件复位脉冲' : '复位未确认，请手动重启并核对版本');

      try {
        await this.transport.disconnect();
      } catch (e: unknown) {
        this.onLog?.(`串口断开提示: ${e instanceof Error ? e.message : String(e)}`);
      }

      this.updateState(
        'done',
        resetSent !== 'unconfirmed'
          ? '固件写入完成，已执行硬件复位脉冲。请查看设备是否启动并核对版本。'
          : '固件写入完成，但未确认复位成功。请手动重启设备并核对版本。',
        100,
        totalSize,
        totalSize
      );

      return true;
    } catch (err: unknown) {
      const detail = err instanceof Error ? `${err.name}: ${err.message}` : String(err);
      const errMsg = `${stage}阶段失败：${detail}`;
      this.onLog?.(`烧录失败: ${errMsg}`);
      this.updateState('error', errMsg, lastPercent, lastWritten, totalSize, errMsg);

      try {
        if (this.transport) {
          await this.transport.disconnect();
        }
      } catch {
        // ignore
      }
      return false;
    } finally {
      this.transport = null;
      this.esploader = null;
    }
  }
}
