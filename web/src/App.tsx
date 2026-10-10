import React, { useState } from 'react';
import { translations, type Lang } from './utils/i18n';
import { FlasherSection } from './components/FlasherSection';
import { FeaturesAndGallery } from './components/FeaturesAndGallery';
import { SpecsAndFooter } from './components/SpecsAndFooter';
import { DeviceMockup } from './components/DeviceMockup';
import { Globe, ArrowRight, Code2 } from 'lucide-react';

export const App: React.FC = () => {
  const [lang, setLang] = useState<Lang>('zh');
  const t = translations[lang];

  return (
    <div className="min-h-screen flex flex-col justify-between bg-[#faf9f5] text-[#141413] selection:bg-[#cc785c] selection:text-[#ffffff]">
      {/* 顶部导航 (Claude top-nav 规范: 64px, #faf9f5, 1px #e6dfd8 hairline) */}
      <header className="sticky top-0 z-50 backdrop-blur-md bg-[#faf9f5]/90 border-b border-[#e6dfd8] transition-all">
        <div className="max-w-[1200px] mx-auto px-4 sm:px-6 lg:px-8 h-16 flex items-center justify-between">
          <div className="flex items-center gap-3">
            {/* Anthropic / Claude 风格星芒标识 */}
            <svg
              className="w-5 h-5 text-[#141413]"
              viewBox="0 0 24 24"
              fill="currentColor"
            >
              <path d="M12 0L13.5 10.5L24 12L13.5 13.5L12 24L10.5 13.5L0 12L10.5 10.5L12 0Z" />
            </svg>
            <span className="font-claude-serif text-2xl tracking-tight text-[#141413]">
              {t.brand}
            </span>
            <span className="hidden sm:inline-block text-[11px] font-mono text-[#6c6a64] bg-[#efe9de] px-2 py-0.5 rounded-[4px] border border-[#e6dfd8]">
              {t.subBrand}
            </span>
          </div>

          <nav className="flex items-center gap-5 sm:gap-7 text-sm font-medium text-[#3d3d3a]">
            <a href="#features" className="hover:text-[#cc785c] transition-colors">
              {t.nav.features}
            </a>
            <a href="#gallery" className="hover:text-[#cc785c] transition-colors">
              {t.nav.gallery}
            </a>
            <a href="#flasher" className="hover:text-[#cc785c] transition-colors">
              {t.nav.flasher}
            </a>
            <a href="#specs" className="hidden sm:inline-block hover:text-[#cc785c] transition-colors">
              {t.nav.specs}
            </a>

            {/* 语言切换 (Claude 风格轻质按键) */}
            <button
              onClick={() => setLang(lang === 'zh' ? 'en' : 'zh')}
              className="flex items-center gap-1.5 px-3 py-1.5 rounded-[6px] bg-[#efe9de] hover:bg-[#e8e0d2] border border-[#e6dfd8] text-xs font-mono transition-colors text-[#141413]"
              title="Switch Language / 切换语言"
            >
              <Globe className="w-3.5 h-3.5 text-[#6c6a64]" />
              <span>{lang === 'zh' ? 'EN' : '中文'}</span>
            </button>

            {/* GitHub 图标 */}
            <a
              href="https://github.com"
              target="_blank"
              rel="noreferrer"
              className="p-1.5 rounded-[6px] text-[#6c6a64] hover:text-[#141413] hover:bg-[#efe9de] transition-colors"
              title="GitHub"
            >
              <Code2 className="w-5 h-5" />
            </a>
          </nav>
        </div>
      </header>

      {/* Hero 区域 (Claude 规范: 96px 间距，Copernicus/Tiempos 衬线大标，Coral 主色 CTA) */}
      <section className="pt-20 pb-24 px-4 sm:px-6 lg:px-8 max-w-[1200px] mx-auto flex flex-col lg:flex-row items-center justify-between gap-12 lg:gap-8">
        <div className="max-w-xl text-center lg:text-left">
          {/* Claude 风格 badge-pill 标签 (带最新固件版本号) */}
          <div className="inline-flex items-center gap-2 px-3 py-1 rounded-full bg-[#efe9de] border border-[#e6dfd8] text-xs font-medium text-[#6c6a64] mb-6">
            <span className="w-1.5 h-1.5 rounded-full bg-[#cc785c]" />
            <span>ESP32-S3 · 4.7" E-Paper · Firmware v0.5.45</span>
          </div>

          <h1 className="text-4xl sm:text-6xl font-claude-serif text-[#141413] leading-[1.08] mb-6">
            {t.tagline}
          </h1>

          <p className="text-[#3d3d3a] text-base sm:text-lg leading-relaxed mb-8">
            {t.description}
          </p>

          <div className="flex flex-wrap items-center justify-center lg:justify-start gap-4">
            {/* button-primary: Claude Coral #cc785c */}
            <a
              href="#flasher"
              className="px-6 py-3 rounded-[8px] bg-[#cc785c] hover:bg-[#a9583e] text-[#ffffff] font-medium text-sm flex items-center gap-2 shadow-xs transition-colors"
            >
              <span>{t.nav.flasher}</span>
              <ArrowRight className="w-4 h-4" />
            </a>
            {/* button-secondary: 浅米色底 + hairline 描边 */}
            <a
              href="#features"
              className="px-6 py-3 rounded-[8px] bg-[#faf9f5] hover:bg-[#efe9de] text-[#141413] font-medium text-sm border border-[#e6dfd8] transition-colors flex items-center gap-2"
            >
              <span>{t.nav.features}</span>
            </a>
          </div>
        </div>

        {/* Hero 右侧机身展示图 */}
        <div className="w-full max-w-md lg:max-w-sm flex justify-center">
          <DeviceMockup imageSrc="./screenshots/reader.png" altText="Read Pico Hardware Mockup" />
        </div>
      </section>

      {/* 核心功能与真机渲染图 */}
      <FeaturesAndGallery lang={lang} />

      {/* 在线烧录器 */}
      <FlasherSection lang={lang} />

      {/* 参数规格与 Claude 风格深色页脚 */}
      <SpecsAndFooter lang={lang} />
    </div>
  );
};
