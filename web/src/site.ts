// 中文：官网使用已发布固件版本元数据，独立于私下设备源码；暂不公开仓库入口。/ English: Use published-firmware metadata independently of private device source, without repository links.
import version from '../firmware-version.txt?raw';
export const FIRMWARE_VERSION = version.trim();
