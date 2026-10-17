#!/usr/bin/env python3
"""
Generate Xiaohongshu release cards (1080x1440) using real native firmware screenshots.
"""
import os
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
IN_DIR = ROOT / "build-host/xiaohongshu"
OUT_DIR = ROOT / "build-host/xiaohongshu/cards"
OUT_DIR.mkdir(parents=True, exist_ok=True)

FONT_PATH = str(ROOT / "sdcard/fonts/ChillDuanSansVF.ttf")

def get_font(size):
    return ImageFont.truetype(FONT_PATH, size)

def draw_pill(draw, xy, text, font, fill="#EFEFEA", text_fill="#333333", border="#DDDDDA"):
    x0, y0, x1, y1 = xy
    draw.rounded_rectangle([x0, y0, x1, y1], radius=(y1 - y0) // 2, fill=fill, outline=border, width=1)
    bbox = font.getbbox(text)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    tx = x0 + (x1 - x0 - tw) // 2
    ty = y0 + (y1 - y0 - th) // 2 - bbox[1]
    draw.text((tx, ty), text, fill=text_fill, font=font)

def create_device_mockup(screen_img, target_screen_w=518, target_screen_h=920):
    # Resize screen image with high-quality resampling
    resized_screen = screen_img.convert("RGB").resize((target_screen_w, target_screen_h), Image.Resampling.LANCZOS)
    
    bezel_pad = 22
    bottom_pad = 54
    bezel_w = target_screen_w + bezel_pad * 2
    bezel_h = target_screen_h + bezel_pad + bottom_pad
    
    device = Image.new("RGBA", (bezel_w, bezel_h), (0, 0, 0, 0))
    d_draw = ImageDraw.Draw(device)
    
    # Device body - matte graphite body with subtle edge highlight
    d_draw.rounded_rectangle([0, 0, bezel_w, bezel_h], radius=24, fill="#1C1E22", outline="#32353B", width=2)
    
    # Inner screen recess line
    d_draw.rounded_rectangle([bezel_pad - 2, bezel_pad - 2, bezel_pad + target_screen_w + 1, bezel_pad + target_screen_h + 1],
                             radius=6, fill="#0F1012")
    
    # Paste screen
    device.paste(resized_screen, (bezel_pad, bezel_pad))
    
    # Three capacitive touch button markers at bottom bezel
    btn_y = bezel_pad + target_screen_h + 24
    center_x = bezel_w // 2
    for offset in [-76, 0, 76]:
        bx = center_x + offset
        d_draw.ellipse([bx - 4, btn_y - 4, bx + 4, btn_y + 4], fill="#444850")
        d_draw.ellipse([bx - 1.5, btn_y - 1.5, bx + 1.5, btn_y + 1.5], fill="#7A7F8A")
        
    # Subtle logo/brand at top or bottom
    f_sub = get_font(11)
    brand = "READ PICO"
    b_box = f_sub.getbbox(brand)
    bw = b_box[2] - b_box[0]
    d_draw.text((center_x - bw // 2, bezel_pad // 2 - 4), brand, fill="#484C55", font=f_sub)
    
    return device

def render_card(title, subtitle, pills, screen_path, footer_specs, output_path, badge="小纸 Pico · 专属阅读固件"):
    W, H = 1080, 1440
    # Clean warm minimalist paper background
    canvas = Image.new("RGB", (W, H), "#F7F7F4")
    draw = ImageDraw.Draw(canvas)
    
    # Subtle top hairline
    draw.line([60, 40, 1020, 40], fill="#E4E4DF", width=1)
    
    # Top brand / badge line
    f_badge = get_font(18)
    draw.text((60, 56), badge.upper(), fill="#8C9098", font=f_badge)
    
    f_ver = get_font(18)
    draw.text((1020 - f_ver.getbbox("v0.5.14 · OPEN SOURCE")[2], 56), "v0.5.14 · OPEN SOURCE", fill="#8C9098", font=f_ver)
    
    # Title
    f_title = get_font(46)
    draw.text((60, 94), title, fill="#161719", font=f_title)
    
    # Subtitle
    f_sub = get_font(21)
    draw.text((60, 154), subtitle, fill="#5E626B", font=f_sub)
    
    # Pills
    f_pill = get_font(17)
    px = 60
    py = 194
    for p in pills:
        bbox = f_pill.getbbox(p)
        pw = bbox[2] - bbox[0] + 28
        draw_pill(draw, [px, py, px + pw, py + 34], p, f_pill, fill="#EBECE7", text_fill="#26282D", border="#DCDDD7")
        px += pw + 12
        
    # Render device with shadow
    screen_img = Image.open(screen_path)
    mockup = create_device_mockup(screen_img)
    
    mw, mh = mockup.size
    dev_x = (W - mw) // 2
    dev_y = 252
    
    # Soft drop shadow
    shadow = Image.new("RGBA", (mw + 80, mh + 80), (0, 0, 0, 0))
    s_draw = ImageDraw.Draw(shadow)
    s_draw.rounded_rectangle([36, 40, 36 + mw, 40 + mh], radius=32, fill=(0, 0, 0, 65))
    shadow = shadow.filter(ImageFilter.GaussianBlur(radius=24))
    canvas.paste(shadow, (dev_x - 40, dev_y - 20), shadow)
    
    # Paste device
    canvas.paste(mockup, (dev_x, dev_y), mockup)
    
    # Footer line and specs
    draw.line([60, 1374, 1020, 1374], fill="#E2E2DC", width=1)
    f_footer = get_font(17)
    draw.text((60, 1388), footer_specs, fill="#7C8088", font=f_footer)
    
    star_txt = "★ GITHUB OPEN SOURCE"
    sw = f_footer.getbbox(star_txt)[2]
    draw.text((1020 - sw, 1388), star_txt, fill="#40434A", font=f_footer)
    
    canvas.save(output_path, "PNG", quality=95)
    print(f"Generated: {output_path}")

def main():
    cards = [
        {
            "badge": "小纸 PICO · 沉浸阅读升级",
            "title": "正文翻页零黑闪 · 极致清晰通透",
            "subtitle": "专属调优 GL16 与直刷波形，告别刺眼黑闪，保留 8 档平滑抗锯齿",
            "pills": ["GL16 零黑闪", "8档原厂边缘灰阶", "独立防漏触摸队列", "字号与行距精调"],
            "screen": IN_DIR / "3_reader.png",
            "footer": "ESP32-S3 · 120MHz · 4.7寸 1216×684 · 16级灰阶 · 原生波形驱动",
            "out": OUT_DIR / "card_1_reading.png",
        },
        {
            "badge": "小纸 PICO · 自由排版系统",
            "title": "TXT / EPUB 深度支持 · 原生目录与封面",
            "subtitle": "EPUB 封面自动提取，支持 TF 卡随心加 TTF 字体，防串行辅助线",
            "pills": ["EPUB 原书封面", "TF 扩展 TTF 字体", "防串行辅助虚线", "夜间反色模式"],
            "screen": IN_DIR / "4_tools.png",
            "footer": "NCX/nav 章节直达 · 进度精准定位 · 金句长按摘录 · 历史自动续读",
            "out": OUT_DIR / "card_2_typography.png",
        },
        {
            "badge": "小纸 PICO · 无线互联生态",
            "title": "免拔卡插线 · 手机扫码秒级传书",
            "subtitle": "设备自建热点生成二维码，手机扫码拖拽上传；支持 WiFi 与 KOReader 同步",
            "pills": ["热点扫码秒传", "浏览器网页拖拽", "USB 一键磁盘模式", "kosync 进度同步"],
            "screen": IN_DIR / "5_transfer.png",
            "footer": "2.4GHz WiFi 触屏免密连 · 支持批量上传 · 离页断网省电 · 兼容 KOReader",
            "out": OUT_DIR / "card_3_transfer.png",
        },
        {
            "badge": "小纸 PICO · 桌面生活美学",
            "title": "极简黄历/时钟锁屏 · 桌面艺术摆件",
            "subtitle": "传统农历与宜读书大字，内置阅读时长手帐，微安级低功耗拿起唤醒",
            "pills": ["传统农历黄历", "大字极简时钟", "7日阅读手帐", "4位安全密码锁"],
            "screen": IN_DIR / "6_today.png",
            "footer": "硬件 PMU RTC 高精度计时 · 分钟静默走时 · 超长待机 · 拿起即读",
            "out": OUT_DIR / "card_4_lifestyle.png",
        },
    ]
    
    for c in cards:
        render_card(c["title"], c["subtitle"], c["pills"], c["screen"], c["footer"], c["out"], c["badge"])

if __name__ == "__main__":
    main()

# Generate 5th card: Shelf
render_card(
    title="多源书架管理 · 真实书封与检索",
    subtitle="EPUB 真实封面有界提取，支持拼音与英文快搜，批量管理与阅读续读",
    pills=["EPUB 原生封面", "拼音/首字母快搜", "单本管理/批量清理", "历史无缝续读"],
    screen_path=IN_DIR / "2_shelf.png",
    footer_specs="TF 卡 + 内置存储双源支持 · 真实阅读进度保存 · 沉浸式单次开书",
    output_path=OUT_DIR / "card_5_shelf.png",
    badge="小纸 PICO · 书库检索管理"
)
print("Generated: card_5_shelf.png")
