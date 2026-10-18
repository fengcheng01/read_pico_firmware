import { useState } from 'react';
import { ArrowRight, Check, Copy, Download, FileCheck2, ShieldCheck } from 'lucide-react';
import type { Lang } from '../utils/i18n';
import firmwareVersion from '../../firmware-version.txt?raw';

const version = firmwareVersion.trim();
const release = {
  version,
  full: {
    file: `PageRest-Read_Pico-${version}-full.bin`,
    href: `./firmware/${version}/PageRest-Read_Pico-${version}-full.bin`,
    bytes: '3,985,520 B', offset: '0x0',
    sha256: '1c84e92a8a94d84f1951fe0322f49f2b88fc6f340c0f21c5d69e4369cfac3c75',
  },
};

const copy = {
  zh: {
    tag: '03 / 下载固件', title: '准备好，再更新。',
    intro: (v: string) => `当前公开候选版 ${v}。一份完整固件同时用于日常升级、首次安装和设备恢复。`,
    built: '产品构建 · 2026-10-18 · RDP-G01-W', full: '通用完整固件',
    fullDesc: '下载同一个文件；连接设备后，再选择保留数据升级或全新安装。',
    offset: '完整写入地址', size: '文件大小', checksum: 'SHA-256',
    download: '下载 BIN', copied: '已复制', copy: '复制',
    warning: '保留数据升级只写应用区；选择全新安装时才完整写入，并可能重置设置与阅读记录。',
    verify: '浏览器会检查镜像并识别设备；固件文件不会上传到服务器。', flash: '选择安装方式',
  },
  en: {
    tag: '03 / DOWNLOAD FIRMWARE', title: 'Ready when you are.',
    intro: (v: string) => `Current public candidate ${v}. One complete image covers updates, first installs and recovery.`,
    built: 'Product build · 2026-10-18 · RDP-G01-W', full: 'Universal firmware',
    fullDesc: 'Download one file, then choose a data-preserving update or a fresh installation after connecting.',
    offset: 'Full-write address', size: 'File size', checksum: 'SHA-256',
    download: 'Download BIN', copied: 'Copied', copy: 'Copy',
    warning: 'A data-preserving update writes only the app. A fresh installation performs the full write and may reset settings and reading records.',
    verify: 'The browser checks the image and identifies the device; the file is never uploaded.', flash: 'Choose install mode',
  },
};

type ImageInfo = typeof release.full;

const ImageCard = ({ image, lang }: { image: ImageInfo; lang: Lang }) => {
  const t = copy[lang];
  const [copied, setCopied] = useState(false);
  const copyHash = async () => {
    try { await navigator.clipboard.writeText(image.sha256); setCopied(true); window.setTimeout(() => setCopied(false), 1600); } catch { /* Clipboard is optional; the hash remains selectable. */ }
  };
  return <article className="firmware-card full-card">
    <div className="firmware-card-top"><div><p className="firmware-type">{t.full}</p></div><FileCheck2 size={22} strokeWidth={1.4} /></div>
    <p className="firmware-desc">{t.fullDesc}</p>
    <dl className="firmware-meta"><div><dt>{t.offset}</dt><dd className="mono">{image.offset}</dd></div><div><dt>{t.size}</dt><dd className="mono">{image.bytes}</dd></div></dl>
    <div className="firmware-hash"><span>{t.checksum}</span><code>{image.sha256}</code><button type="button" onClick={copyHash} aria-label={copied ? t.copied : t.copy}>{copied ? <Check size={15} /> : <Copy size={15} />}</button></div>
    <a className="button primary firmware-download" href={image.href} download><Download size={16} />{t.download}<ArrowRight size={16} /></a>
  </article>;
};

export const FirmwareDownloadSection = ({ lang }: { lang: Lang }) => {
  const t = copy[lang];
  return <section id="firmware" className="section firmware-section"><div className="container">
    <div className="section-heading firmware-heading"><div><p className="eyebrow">{t.tag}</p><h2>{t.title}</h2></div><p>{t.intro(release.version)}</p></div>
    <div className="firmware-release-bar"><div className="firmware-release-version"><span className="firmware-live-dot" /> <strong>PageRest {release.version}</strong><span>{t.built}</span></div></div>
    <div className="firmware-grid"><ImageCard image={release.full} lang={lang} /></div>
    <div className="firmware-bottom"><p><ShieldCheck size={17} />{t.verify}</p><p className="firmware-warning">{t.warning}</p><a className="text-link" href="#flasher">{t.flash}<ArrowRight size={16} /></a></div>
  </div></section>;
};
