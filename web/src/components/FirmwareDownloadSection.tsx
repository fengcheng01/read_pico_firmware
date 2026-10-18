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
  incremental: {
    file: `PageRest-Read_Pico-${version}-incremental.bin`,
    href: `./firmware/${version}/PageRest-Read_Pico-${version}-incremental.bin`,
    bytes: '3,919,984 B', offset: '0x10000',
    sha256: '3da9216757300620a4e76376371ba1b90ba3dd51acea22796240070d69e3c242',
  },
};

const copy = {
  zh: {
    tag: '03 / 下载固件', title: '准备好，再更新。',
    intro: (v: string) => `当前公开候选版 ${v}。两份镜像来自同一次产品构建，下载后可以直接交给下方的在线烧录工具。`,
    built: '同次构建 · 2026-10-18 · RDP-G01-W', full: '完整安装', incremental: '增量升级',
    fullDesc: '首次安装、恢复设备或分区结构不确定时使用。', incrementalDesc: '已有 PageRest 且希望保留设置与阅读记录时优先使用。',
    recommended: '日常升级推荐', offset: '写入地址', size: '文件大小', checksum: 'SHA-256',
    download: '下载 BIN', copied: '已复制', copy: '复制',
    warning: 'full.bin 适合首次安装；完整写入可能重置设置与阅读记录。增量包不是二进制差分补丁。',
    verify: '浏览器会在烧录前再次检查镜像结构；文件不会上传到服务器。', flash: '去在线烧录',
  },
  en: {
    tag: '03 / DOWNLOAD FIRMWARE', title: 'Ready when you are.',
    intro: (v: string) => `Current public candidate ${v}. Both images come from one product build and can be handed to the web flasher below.`,
    built: 'One build · 2026-10-18 · RDP-G01-W', full: 'Full installation', incremental: 'Incremental upgrade',
    fullDesc: 'Use for a first install, recovery or an unknown partition layout.', incrementalDesc: 'Preferred for an existing PageRest device when keeping settings and reading records.',
    recommended: 'Recommended for updates', offset: 'Write address', size: 'File size', checksum: 'SHA-256',
    download: 'Download BIN', copied: 'Copied', copy: 'Copy',
    warning: 'full.bin is for first install; a full write may reset settings and reading records. The incremental image is not a binary delta.',
    verify: 'The browser checks the image again before flashing; the file is never uploaded.', flash: 'Open web flasher',
  },
};

type ImageInfo = typeof release.full;

const ImageCard = ({ image, kind, lang, recommended }: { image: ImageInfo; kind: 'full' | 'incremental'; lang: Lang; recommended?: boolean }) => {
  const t = copy[lang];
  const [copied, setCopied] = useState(false);
  const copyHash = async () => {
    try { await navigator.clipboard.writeText(image.sha256); setCopied(true); window.setTimeout(() => setCopied(false), 1600); } catch { /* Clipboard is optional; the hash remains selectable. */ }
  };
  const isFull = kind === 'full';
  return <article className={`firmware-card ${isFull ? 'full-card' : 'incremental-card'}`}>
    <div className="firmware-card-top"><div><p className="firmware-type">{isFull ? t.full : t.incremental}{recommended && <small className="firmware-recommended-note">{t.recommended}</small>}</p></div><FileCheck2 size={22} strokeWidth={1.4} /></div>
    <p className="firmware-desc">{isFull ? t.fullDesc : t.incrementalDesc}</p>
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
    <div className="firmware-grid"><ImageCard image={release.full} kind="full" lang={lang} /><ImageCard image={release.incremental} kind="incremental" lang={lang} recommended /></div>
    <div className="firmware-bottom"><p><ShieldCheck size={17} />{t.verify}</p><p className="firmware-warning">{t.warning}</p><a className="text-link" href="#flasher">{t.flash}<ArrowRight size={16} /></a></div>
  </div></section>;
};
