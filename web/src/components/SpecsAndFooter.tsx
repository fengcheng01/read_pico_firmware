import React from 'react';
import { translations, type Lang } from '../utils/i18n';
import { Cpu, Tv, BatteryCharging, HardDrive, KeyRound, Radio } from 'lucide-react';

interface SpecsProps {
  lang: Lang;
}

export const SpecsAndFooter: React.FC<SpecsProps> = ({ lang }) => {
  const tSpecs = translations[lang].specs;
  const tFoot = translations[lang].footer;

  const specList = [
    {
      icon: <Cpu className="w-5 h-5 text-[#cc785c]" />,
      label: tSpecs.mcu,
      value: tSpecs.mcuVal,
    },
    {
      icon: <Tv className="w-5 h-5 text-[#cc785c]" />,
      label: tSpecs.display,
      value: tSpecs.displayVal,
    },
    {
      icon: <BatteryCharging className="w-5 h-5 text-[#cc785c]" />,
      label: tSpecs.pmu,
      value: tSpecs.pmuVal,
    },
    {
      icon: <HardDrive className="w-5 h-5 text-[#cc785c]" />,
      label: tSpecs.storage,
      value: tSpecs.storageVal,
    },
    {
      icon: <KeyRound className="w-5 h-5 text-[#cc785c]" />,
      label: tSpecs.battery,
      value: tSpecs.batteryVal,
    },
    {
      icon: <Radio className="w-5 h-5 text-[#cc785c]" />,
      label: tSpecs.connectivity,
      value: tSpecs.connectivityVal,
    },
  ];

  return (
    <>
      {/* 规格参数区域 (Claude feature card 风格: #efe9de, 12px 圆角) */}
      <section id="specs" className="py-20 px-4 sm:px-6 lg:px-8 max-w-[1200px] mx-auto">
        <div className="text-center mb-12">
          <span className="text-[12px] font-mono uppercase tracking-[1.5px] text-[#6c6a64] bg-[#efe9de] px-3 py-1 rounded-full border border-[#e6dfd8]">
            {tSpecs.tag}
          </span>
          <h2 className="text-3xl sm:text-4xl font-claude-serif text-[#141413] mt-4 mb-3">
            {tSpecs.title}
          </h2>
        </div>

        <div className="rounded-[12px] bg-[#efe9de] border border-[#e6dfd8] p-6 sm:p-10 divide-y divide-[#e6dfd8]">
          {specList.map((s, idx) => (
            <div key={idx} className="py-4 first:pt-0 last:pb-0 flex flex-col sm:flex-row sm:items-center justify-between gap-2">
              <div className="flex items-center gap-3 text-sm font-semibold text-[#141413]">
                <div>{s.icon}</div>
                <span>{s.label}</span>
              </div>
              <div className="text-xs sm:text-sm font-mono text-[#3d3d3a] sm:text-right max-w-xl">
                {s.value}
              </div>
            </div>
          ))}
        </div>
      </section>

      {/* Claude 规范签名页脚: #181715 深色底，文本 #a09d96，64px padding */}
      <footer className="mt-16 bg-[#181715] text-[#a09d96] py-16 px-4 sm:px-6 lg:px-8 border-t border-[#252320]">
        <div className="max-w-[1200px] mx-auto flex flex-col sm:flex-row items-center justify-between gap-6 text-center sm:text-left text-xs">
          <div className="space-y-2">
            <div className="flex items-center justify-center sm:justify-start gap-2 text-[#faf9f5]">
              <svg className="w-4 h-4 text-[#cc785c]" viewBox="0 0 24 24" fill="currentColor">
                <path d="M12 0L13.5 10.5L24 12L13.5 13.5L12 24L10.5 13.5L0 12L10.5 10.5L12 0Z" />
              </svg>
              <span className="font-claude-serif text-base tracking-tight">小纸 Pico · Read Pico</span>
            </div>
            <div className="text-[#a09d96]">{tFoot.license}</div>
            <div className="text-[#6c6a64]">{tFoot.disclaimer}</div>
          </div>

          <div className="flex items-center gap-6 text-xs text-[#a09d96]">
            <a href="#features" className="hover:text-[#faf9f5] transition-colors">功能</a>
            <a href="#gallery" className="hover:text-[#faf9f5] transition-colors">界面</a>
            <a href="#flasher" className="hover:text-[#faf9f5] transition-colors">烧录</a>
            <a href="https://github.com" target="_blank" rel="noreferrer" className="hover:text-[#faf9f5] transition-colors">GitHub</a>
          </div>
        </div>
      </footer>
    </>
  );
};
