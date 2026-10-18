/* SPDX-License-Identifier: Apache-2.0
 * 中文：识别 ESP32-S3 应用/合并镜像，生成烧录计划；不读设备、不写串口。
 * English: Identify ESP32-S3 app/merged images and prepare flash plans without device or serial access.
 */
export interface FirmwareImage {
  kind: 'app' | 'full';
  version: string;
  appOffset: number;
  app: Uint8Array;
}

const APP_OFFSET = 0x10000;
const APP_LIMIT = 0x400000;
const u32 = (data: Uint8Array, at: number): number =>
  new DataView(data.buffer, data.byteOffset, data.byteLength).getUint32(at, true);

function imageLength(data: Uint8Array, start: number): number {
  if (start + 24 > data.length || data[start] !== 0xe9 || data[start + 12] !== 9 || data[start + 13] !== 0)
    throw new Error('不是有效的小纸 Pico ESP32-S3 镜像。');
  const segments = data[start + 1];
  if (!segments || segments > 16) throw new Error('固件段数无效，请重新下载文件。');
  let cursor = start + 24;
  let checksum = 0xef;
  for (let i = 0; i < segments; ++i) {
    if (cursor + 8 > data.length) throw new Error('固件文件不完整，请重新下载。');
    const length = u32(data, cursor + 4);
    cursor += 8;
    if (length > APP_LIMIT || cursor + length > data.length) throw new Error('固件段被截断或超出范围。');
    for (let p = cursor; p < cursor + length; ++p) checksum ^= data[p];
    cursor += length;
  }
  const checksumAt = start + ((cursor - start) | 15);
  const end = checksumAt + 1 + (data[start + 23] === 1 ? 32 : 0);
  if (end > data.length || data[checksumAt] !== checksum) throw new Error('固件长度或校验不匹配，请重新下载。');
  return end - start;
}

export function inspectFirmware(data: Uint8Array): FirmwareImage {
  if (data.length < 80 || data.length > 16 * 1024 * 1024) throw new Error('固件文件大小无效。');
  const full = data.length > APP_OFFSET + 80 && data[0x8000] === 0xaa && data[0x8001] === 0x50;
  const start = full ? APP_OFFSET : 0;
  let capacity = APP_LIMIT;
  if (full) {
    imageLength(data, 0);
    let appSize = 0;
    for (let at = 0x8000; at < 0x9000; at += 32) {
      if (data[at] !== 0xaa || data[at + 1] !== 0x50) break;
      if (data[at + 2] === 0 && u32(data, at + 4) === APP_OFFSET) appSize = u32(data, at + 8);
    }
    if (!appSize || appSize > 16 * 1024 * 1024 - APP_OFFSET)
      throw new Error('该完整包没有有效的 0x10000 应用分区。');
    capacity = appSize;
  }
  const length = imageLength(data, start);
  if (length > capacity || u32(data, start + 32) !== 0xabcd5432)
    throw new Error('没有找到有效应用；请选 PageRest 的 full 或 incremental BIN。');
  if (start + length !== data.length) throw new Error('应用末尾存在额外内容，请使用官方打包产物。');
  const version = new TextDecoder().decode(data.subarray(start + 48, start + 80)).split('\0')[0];
  return { kind: full ? 'full' : 'app', version, appOffset: APP_OFFSET, app: data.slice(start, start + length) };
}

export function prepareFirmware(data: Uint8Array, name: string, upgrade: boolean) {
  const image = inspectFirmware(data);
  if (!upgrade && image.kind !== 'full')
    throw new Error('这是增量应用 BIN。请选择“保留数据升级”，写入地址为 0x10000；不能按完整包写到 0x0。');
  return {
    name: upgrade && image.kind === 'full' ? `${name}（应用分区）` : name,
    address: upgrade ? APP_OFFSET : 0,
    data: upgrade ? image.app : data,
  };
}

/// 中文：单一入口按内容生成地址，完整包覆盖NVS；增量禁止全擦。/ English: Plan one entry by content; full covers NVS, while apps forbid full erase.
export function planFirmware(data: Uint8Array, name: string, eraseAll = false) {
  const image = inspectFirmware(data);
  if (image.kind === 'app' && eraseAll) throw new Error('增量升级不能全片擦除，请使用完整 full.bin 进行重装。');
  return { image, eraseAll, file: { name, address: image.kind === 'full' ? 0 : APP_OFFSET, data } };
}

/// 中文：识别设备端 0x10000 应用描述是否本固件；返回设备版本号，非本固件返回 null。
/// 供烧录器连接后探测：完整包在已运行本固件的设备上自动改为增量升级。
/// English: Identify the on-device app description at 0x10000 as this firmware;
/// returns the device version, or null otherwise. Used by the flasher after connect:
/// full images switch to an app-only upgrade on devices already running this firmware.
export function identifyPicoApp(head: Uint8Array): string | null {
  if (head.length < 0x100 || head[0] !== 0xe9 || u32(head, 32) !== 0xabcd5432) return null;
  const field = (offset: number) =>
    new TextDecoder().decode(head.subarray(32 + offset, 32 + offset + 32)).split('\0')[0].trim();
  if (!/read[ _-]?pico/i.test(field(48))) return null;
  return field(16) || field(48);
}
