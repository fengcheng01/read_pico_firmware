/* SPDX-License-Identifier: Apache-2.0
 * 中文：先走 ROM 软复位命令让 bootloader 退出并启动应用，再用 esptool 硬复位
 * 时序兜底；任一成功即算复位已发送。
 * English: First ask the ROM to leave the loader and boot the app, then fall
 * back to the esptool hard-reset pulse; either success counts as sent.
 */

export interface FlasherLogger {
  log: (msg: string) => void;
  error: (msg: string) => void;
}

export interface EspLoaderLike {
  flashBegin(size: number, offset: number): Promise<void>;
  flashFinish(reboot: boolean): Promise<void>;
}

export interface TransportLike {
  setDTR(state: boolean): Promise<void>;
  setRTS(state: boolean): Promise<void>;
}

function delay(ms: number): Promise<void> {
  const { promise, resolve } = Promise.withResolvers<void>();
  setTimeout(resolve, ms);
  return promise;
}

/**
 * Pico 专有复位逻辑：软复位命令优先，RTS 硬复位脉冲兜底
 */
export async function picoRestartTransport(
  esploader: EspLoaderLike,
  transport: TransportLike,
  pause: (ms: number) => Promise<void> = delay,
  logger?: FlasherLogger
): Promise<boolean> {
  let softReset = false;
  try {
    // ROM 命令通道与烧录相同：flashBegin(0,0)+flashFinish(true) 即退出下载模式
    await esploader.flashBegin(0, 0);
    await esploader.flashFinish(true);
    softReset = true;
    logger?.log('ROM 软复位命令发送成功 (flashBegin 0 + flashFinish true)');
  } catch (err: unknown) {
    const message = err instanceof Error ? err.message : String(err);
    logger?.log(`软复位未响应，尝试硬复位时序: ${message}`);
  }

  // 兜底：esptool hard_reset 的 RTS 复位脉冲（DTR 保持非下载态）
  try {
    await transport.setDTR(false);
    await transport.setRTS(true);
    await pause(200);
    await transport.setRTS(false);
    await pause(200);
  } catch (err: unknown) {
    const message = err instanceof Error ? err.message : String(err);
    logger?.log(`硬复位脉冲执行报错: ${message}`);
  }

  return softReset;
}
