/* SPDX-License-Identifier: Apache-2.0
 * 中文：退出烧录协议后仍发送小纸的硬件复位脉冲；命令应答不代表设备已经重启。
 * English: Send the Pico hardware-reset pulse after leaving the flash protocol; an ACK does not establish a reboot.
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

// 退出烧录协议后执行板级RTS复位。/ Reset via board RTS after leaving the flash protocol.
export async function picoRestartTransport(
  esploader: EspLoaderLike,
  transport: TransportLike,
  pause: (ms: number) => Promise<void> = delay,
  logger?: FlasherLogger
): Promise<'hard' | 'unconfirmed'> {
  try {
    // ESP32-S3 stub可应答但仍驻留，不能因ACK跳过板级复位。
    // The ESP32-S3 stub may ACK while remaining active; never skip board reset on that ACK.
    await esploader.flashBegin(0, 0);
    await esploader.flashFinish(true);
    logger?.log('退出烧录命令已应答，继续发送硬件复位脉冲');
  } catch (err: unknown) {
    const message = err instanceof Error ? err.message : String(err);
    logger?.log(`软复位未响应，尝试硬复位时序: ${message}`);
  }

  // 恢复实机曾可用的必发RTS脉冲，DTR保持非下载态。
  // Restore the previously working mandatory RTS pulse, keeping DTR outside download state.
  try {
    await transport.setDTR(false);
    await transport.setRTS(true);
    await pause(200);
    await transport.setRTS(false);
    await pause(200);
    return 'hard';
  } catch (err: unknown) {
    const message = err instanceof Error ? err.message : String(err);
    logger?.log(`硬复位脉冲执行报错: ${message}`);
  }

  return 'unconfirmed';
}
