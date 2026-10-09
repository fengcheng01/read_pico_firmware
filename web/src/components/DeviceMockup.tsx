import React from 'react';

interface DeviceMockupProps {
  imageSrc: string;
  altText: string;
  className?: string;
}

/**
 * 小纸 Pico (Read Pico) 4.7" 经典白色实机外观结构：
 * 1. 经典纯净暖白/哑光白机身外壳 (Warm White / Pure White Polycarbonate / Metal Finish)
 * 2. 顶部右上实体电源按键 (轻微反光白色微凸按键)
 * 3. 顶栏装饰：左侧浅灰激光刻印品牌字标 "READ PICO · 4.7\""，右侧 PMU 状态指示灯 (LED 带柔光圈)
 * 4. 4.7 寸 684×1216 墨水屏可视区域，微凹白/浅灰沉板 Bezel，类纸漫反射表面
 * 5. 底部三实体电容按键：KEY1 (◀ 上页) / KEY2 (● 菜单/强刷) / KEY3 (▶ 下页)
 */
export const DeviceMockup: React.FC<DeviceMockupProps> = ({ imageSrc, altText, className = '' }) => {
  return (
    <div className={`relative mx-auto flex items-center justify-center p-2 select-none ${className}`}>
      {/* 小纸白色实机外壳容器 */}
      <div className="relative w-[360px] sm:w-[410px] rounded-[36px] bg-gradient-to-b from-[#ffffff] via-[#f7f6f2] to-[#edebe4] pt-10 pb-6 px-5 sm:px-6 shadow-[0_24px_50px_rgba(0,0,0,0.12),0_4px_16px_rgba(0,0,0,0.06)] border-2 border-[#e2dfd5] transition-transform duration-300 hover:scale-[1.01]">
        
        {/* 顶部开关键 (白色实机顶部右侧微凸按键) */}
        <div
          className="absolute -top-[5px] right-[48px] w-11 h-[5px] rounded-t-[3px] bg-[#e5e2d8] shadow-[0_-1px_2px_rgba(0,0,0,0.1)] border-t border-[#fcfbf7]"
          title="电源键 (短按锁屏 / 长按开关机)"
        />

        {/* 顶栏装饰排布：左侧实机品牌字样，右侧 PMU 状态指示灯孔 */}
        <div className="flex items-center justify-between px-2 pb-3.5">
          <span className="font-mono text-[11px] font-bold tracking-[1.5px] uppercase text-[#96948a]">
            Read Pico · 4.7"
          </span>
          <div
            className="w-2 h-2 rounded-full bg-[#38bdf8] shadow-[0_0_8px_rgba(56,189,248,0.7)] ring-1 ring-[#cbd5e1]"
            title="PMU 状态指示灯"
          />
        </div>

        {/* 墨水屏屏幕视口区 (4.7 寸 684×1216 物理比例，浅灰/哑光白沉板内圈) */}
        <div className="relative aspect-[684/1216] w-full overflow-hidden rounded-[8px] bg-[#f8f7f3] shadow-[inset_0_0_6px_rgba(0,0,0,0.15),inset_0_0_0_1px_rgba(0,0,0,0.12)] border border-[#d6d3c7]">
          <img
            src={imageSrc}
            alt={altText}
            className="w-full h-full object-cover select-none pointer-events-none"
            loading="lazy"
          />
          {/* 纯净纸面漫反射微层 */}
          <div className="pointer-events-none absolute inset-0 bg-gradient-to-tr from-black/[0.015] via-white/[0.04] to-transparent mix-blend-multiply" />
        </div>

        {/* 屏下三个白色经典物理按键 (KEY1 上页 / KEY2 菜单与强刷 / KEY3 下页) */}
        <div className="mt-4 flex items-center justify-around px-2 pt-1">
          {/* KEY1 */}
          <div className="flex flex-col items-center gap-1 group cursor-default">
            <div className="w-6 h-6 rounded-[5px] border-[1.5px] border-[#d4d1c4] bg-[#fbfaf6] flex items-center justify-center text-[#78766c] text-[10px] font-bold shadow-[0_1px_2px_rgba(0,0,0,0.06)] transition-all group-hover:border-[#9c9a8e] group-hover:bg-[#ffffff]">
              ◀
            </div>
            <span className="text-[10px] font-mono text-[#8a887d] tracking-tight">KEY1 上页</span>
          </div>

          {/* KEY2 (中心主控) */}
          <div className="flex flex-col items-center gap-1 group cursor-default">
            <div className="w-8 h-6 rounded-[5px] border-[1.5px] border-[#c8c5b8] bg-[#f5f4ed] flex items-center justify-center text-[#555349] text-[10px] font-bold shadow-[0_1px_2px_rgba(0,0,0,0.08)] transition-all group-hover:border-[#8c8a7e] group-hover:bg-[#ffffff]">
              ●
            </div>
            <span className="text-[10px] font-mono text-[#6e6c62] tracking-tight font-medium">KEY2 菜单</span>
          </div>

          {/* KEY3 */}
          <div className="flex flex-col items-center gap-1 group cursor-default">
            <div className="w-6 h-6 rounded-[5px] border-[1.5px] border-[#d4d1c4] bg-[#fbfaf6] flex items-center justify-center text-[#78766c] text-[10px] font-bold shadow-[0_1px_2px_rgba(0,0,0,0.06)] transition-all group-hover:border-[#9c9a8e] group-hover:bg-[#ffffff]">
              ▶
            </div>
            <span className="text-[10px] font-mono text-[#8a887d] tracking-tight">KEY3 下页</span>
          </div>
        </div>

      </div>
    </div>
  );
};
