/* SPDX-License-Identifier: Apache-2.0
 * 中文：先走 ROM 软复位命令让 bootloader 退出并启动应用，再用 esptool 硬复位
 * 时序兜底；任一成功即算复位已发送。
 * English: First ask the ROM to leave the loader and boot the app, then fall
 * back to the esptool hard-reset pulse; either success counts as sent.
 */
async function picoRestartTransport(esploader, transport, pause = ms => new Promise(resolve => setTimeout(resolve, ms))) {
  // ROM 命令通道与烧录相同：flashBegin(0,0)+flashFinish(true) 即退出下载模式。
  // / Same command channel as flashing: flashBegin(0,0)+flashFinish(true) leaves download mode.
  let softReset = false;
  try {
    await esploader.flashBegin(0, 0);
    await esploader.flashFinish(true);
    softReset = true;
  } catch (error) {
    console.log('soft reset unavailable:', error.message || error);
  }
  // 兜底：esptool hard_reset 的 RTS 复位脉冲（DTR 保持非下载态）。
  // / Fallback: the esptool hard_reset RTS pulse, keeping DTR out of download mode.
  await transport.setDTR(false);
  await transport.setRTS(true);
  await pause(200);
  await transport.setRTS(false);
  await pause(200);
  return softReset;
}
if (typeof module !== "undefined") module.exports = { picoRestartTransport };
