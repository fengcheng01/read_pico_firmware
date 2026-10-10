import React, { useState } from 'react';
import { translations, type Lang } from '../utils/i18n';
import { DeviceMockup } from './DeviceMockup';
import { BookOpen, Library, Calendar, Wifi, Check } from 'lucide-react';

import readerImg from '../assets/screenshots/reader.png';
import wallpaperImg from '../assets/screenshots/wallpaper.png';
import shelfImg from '../assets/screenshots/shelf.png';
import homeImg from '../assets/screenshots/home.png';
import journalTodayImg from '../assets/screenshots/journal_today.png';
import journalMonthImg from '../assets/screenshots/journal_month.png';
import journalQuotesImg from '../assets/screenshots/journal_quotes.png';
import transferImg from '../assets/screenshots/transfer.png';

interface ContentProps {
  lang: Lang;
}

type GalleryTab = 'reading' | 'wallpaper' | 'shelf' | 'home' | 'journal' | 'calendar' | 'quotes' | 'transfer';

export const FeaturesAndGallery: React.FC<ContentProps> = ({ lang }) => {
  const tFeat = translations[lang].features;
  const tGal = translations[lang].gallery;

  const [activeTab, setActiveTab] = useState<GalleryTab>('reading');

  const galleryImages: Record<GalleryTab, { src: string; alt: string; label: string; desc: string }> = {
    reading: {
      src: readerImg,
      alt: 'Read Pico Reading View',
      label: tGal.tabs.reading,
      desc: lang === 'zh' ? '真实图文正文排版 · 16 级灰阶抗锯齿字形与章节底栏' : 'Real illustrated prose layout with 16-level grayscale typography',
    },
    wallpaper: {
      src: wallpaperImg,
      alt: 'Read Pico Wallpaper Lock Screen View',
      label: tGal.tabs.wallpaper,
      desc: lang === 'zh' ? '自定义壁纸锁屏 · 0.5%/99.5% 自动色阶扩展对比度 + 16 级有序抖动，无残影高清定稿' : 'Custom photo wallpaper lock screen with 16-level dither and auto-level contrast boost',
    },
    shelf: {
      src: shelfImg,
      alt: 'Read Pico Bookshelf View',
      label: tGal.tabs.shelf,
      desc: lang === 'zh' ? '卡片书架展示 · 真实图书封面解析、内置/TF来源与精准进度' : 'Typographic cards with extracted covers, dual storage sources and progress',
    },
    home: {
      src: homeImg,
      alt: 'Read Pico Home View',
      label: tGal.tabs.home,
      desc: lang === 'zh' ? '首页正在读 · 最近阅读卡片、断点续读快捷入口' : 'Home screen with last-read book card and instant resume',
    },
    journal: {
      src: journalTodayImg,
      alt: 'Read Pico Today Journal View',
      label: tGal.tabs.journal,
      desc: lang === 'zh' ? '手帐今日足迹 · 7天阅读走势柱状图、阅读时长大盘与打卡印章' : 'Today footprint: 7-day reading trend chart and focus duration stats',
    },
    calendar: {
      src: journalMonthImg,
      alt: 'Read Pico Monthly Calendar View',
      label: tGal.tabs.calendar,
      desc: lang === 'zh' ? '手帐打卡月历 · 实心黑块标记达标阅读日、连续打卡天数统计' : 'Monthly calendar: stamped reading achievements and continuous streak days',
    },
    quotes: {
      src: journalQuotesImg,
      alt: 'Read Pico Quotes & Notes View',
      label: tGal.tabs.quotes,
      desc: lang === 'zh' ? '手帐金句便签 · 正文长按精选句子生成便签卡片，点击直达原文' : 'Quotes & notes: clipped sentences with chapter references and instant jump',
    },
    transfer: {
      src: transferImg,
      alt: 'Read Pico Transfer View',
      label: tGal.tabs.transfer,
      desc: lang === 'zh' ? '局域网传书管理页 · 浏览器直接访问，支持图书/字体/锁屏壁纸拖拽上传与在线管理删除' : 'Web transfer interface: browser drag-and-drop for books, fonts and wallpapers with online management',
    },
  };

  const featureCards = [
    {
      icon: <BookOpen className="w-5 h-5 text-[#cc785c]" />,
      item: tFeat.reading,
    },
    {
      icon: <Library className="w-5 h-5 text-[#cc785c]" />,
      item: tFeat.shelf,
    },
    {
      icon: <Calendar className="w-5 h-5 text-[#cc785c]" />,
      item: tFeat.journal,
    },
    {
      icon: <Wifi className="w-5 h-5 text-[#cc785c]" />,
      item: tFeat.transfer,
    },
  ];

  const tabList: GalleryTab[] = ['reading', 'wallpaper', 'shelf', 'home', 'journal', 'calendar', 'quotes', 'transfer'];

  return (
    <>
      {/* 核心功能板块 (Claude feature-card: #efe9de, 32px padding, 12px rounded, Tiempos 标题) */}
      <section id="features" className="py-24 px-4 sm:px-6 lg:px-8 max-w-[1200px] mx-auto">
        <div className="text-center mb-16">
          <span className="text-[12px] font-mono uppercase tracking-[1.5px] text-[#6c6a64] bg-[#efe9de] px-3 py-1 rounded-full border border-[#e6dfd8]">
            {tFeat.tag}
          </span>
          <h2 className="text-3xl sm:text-5xl font-claude-serif text-[#141413] mt-4 mb-4">
            {tFeat.title}
          </h2>
          <p className="text-[#3d3d3a] max-w-2xl mx-auto text-base sm:text-lg">
            {tFeat.subtitle}
          </p>
        </div>

        <div className="grid grid-cols-1 md:grid-cols-2 gap-8">
          {featureCards.map((c, idx) => (
            <div
              key={idx}
              className="p-8 rounded-[12px] bg-[#efe9de] border border-[#e6dfd8] flex flex-col justify-between transition-colors hover:border-[#d8cfc4]"
            >
              <div>
                <div className="w-10 h-10 rounded-[8px] bg-[#faf9f5] border border-[#e6dfd8] flex items-center justify-center mb-6 shadow-2xs">
                  {c.icon}
                </div>
                <h3 className="text-xl font-claude-serif text-[#141413] mb-2">
                  {c.item.title}
                </h3>
                <p className="text-sm text-[#3d3d3a] mb-6 leading-relaxed">
                  {c.item.desc}
                </p>
              </div>

              <ul className="space-y-3 pt-6 border-t border-[#e6dfd8]">
                <li className="flex items-start gap-2.5 text-xs sm:text-sm text-[#252523]">
                  <Check className="w-4 h-4 text-[#cc785c] shrink-0 mt-0.5" />
                  <span>{c.item.bullet1}</span>
                </li>
                <li className="flex items-start gap-2.5 text-xs sm:text-sm text-[#252523]">
                  <Check className="w-4 h-4 text-[#cc785c] shrink-0 mt-0.5" />
                  <span>{c.item.bullet2}</span>
                </li>
                <li className="flex items-start gap-2.5 text-xs sm:text-sm text-[#252523]">
                  <Check className="w-4 h-4 text-[#cc785c] shrink-0 mt-0.5" />
                  <span>{c.item.bullet3}</span>
                </li>
              </ul>
            </div>
          ))}
        </div>
      </section>

      {/* 真机渲染效果图板块 */}
      <section id="gallery" className="py-20 px-4 sm:px-6 lg:px-8 max-w-[1200px] mx-auto">
        <div className="text-center mb-10">
          <span className="text-[12px] font-mono uppercase tracking-[1.5px] text-[#6c6a64] bg-[#efe9de] px-3 py-1 rounded-full border border-[#e6dfd8]">
            {tGal.tag}
          </span>
          <h2 className="text-3xl sm:text-4xl font-claude-serif text-[#141413] mt-4 mb-3">
            {tGal.title}
          </h2>
          <p className="text-[#3d3d3a] max-w-2xl mx-auto text-base">
            {tGal.subtitle}
          </p>

          {/* 切换 Tab (Claude category-tab 规范: inactive 透明/muted, active: surface-card/ink) */}
          <div className="mt-8 flex flex-wrap justify-center gap-2 max-w-4xl mx-auto p-1.5 rounded-[8px] bg-[#efe9de] border border-[#e6dfd8]">
            {tabList.map((tabKey) => (
              <button
                key={tabKey}
                onClick={() => setActiveTab(tabKey)}
                className={`px-4 py-2 rounded-[6px] text-xs sm:text-sm font-medium transition-all ${
                  activeTab === tabKey
                    ? 'bg-[#faf9f5] text-[#141413] shadow-xs'
                    : 'text-[#6c6a64] hover:text-[#141413]'
                }`}
              >
                {tGal.tabs[tabKey]}
              </button>
            ))}
          </div>
        </div>

        {/* Mockup 显示区域 */}
        <div className="flex flex-col items-center">
          <DeviceMockup
            imageSrc={galleryImages[activeTab].src}
            altText={galleryImages[activeTab].alt}
          />
          <div className="mt-6 flex flex-col items-center gap-1.5 text-center">
            <div className="flex items-center gap-2 text-xs font-mono text-[#141413] font-semibold">
              <span className="w-1.5 h-1.5 rounded-full bg-[#cc785c]" />
              <span>{galleryImages[activeTab].label} · 原生 684×1216 灰阶物理渲染帧</span>
            </div>
            <p className="text-xs text-[#6c6a64] max-w-md">
              {galleryImages[activeTab].desc}
            </p>
          </div>
        </div>
      </section>
    </>
  );
};
