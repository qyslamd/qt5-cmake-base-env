"""生成**产品线**那套 SVG 图标（测量域：device_dmm / calibrate / rms /
trigger / waveform / writeback …，共 24 个）。

注意：**本仓（模板线）的图标不是这套**——模板线用的是 about / check /
curve 那一批。要往哪个仓写就 cd 到那里跑，或用 `--out <目录>` 指过去
（默认相对当前目录，即 `src/app/resources/icons/svg`）。

那份目录是**唯一真源**：Qt 那边 QIcon(":/resources/icons/svg/x.svg") 用，
HTML 原型用相对路径引同一批文件（新图记得在 res.qrc 里登记）。

风格对齐仓里已有的图标：48×48 viewBox、纯色块无渐变、Material 调色板
    #455A64 / #546E7A  主体深灰蓝
    #BBDEFB            浅蓝填充
    #2196F3            蓝，表示"进行/主操作"
    #66BB6A            绿，表示"合格/开始"
    #EF5350            红，表示"停止/锁定/危险"
    #F9A825 / #FFCA28  琥珀，表示"注意/信号"
"""
import os
import sys

OUT = 'src/app/resources/icons/svg'

DARK = '#455A64'
MID = '#546E7A'
GREY = '#78909C'
LIGHT = '#BBDEFB'
BLUE = '#2196F3'
GREEN = '#66BB6A'
RED = '#EF5350'
AMBER = '#F9A825'
AMBER_L = '#FFCA28'

ICONS = {}

# ── 运行控制 ───────────────────────────────────────────────

ICONS['run'] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
  <rect x="5" y="5" width="38" height="38" rx="8" fill="{DARK}"/>
  <rect x="8" y="8" width="32" height="32" rx="6" fill="{LIGHT}"/>
  <path fill="{GREEN}" d="M20 15.5 34 24 20 32.5z"/>
</svg>'''

ICONS['stop'] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
  <rect x="5" y="5" width="38" height="38" rx="8" fill="{DARK}"/>
  <rect x="8" y="8" width="32" height="32" rx="6" fill="#FFCDD2"/>
  <rect x="16" y="16" width="16" height="16" rx="2" fill="{RED}"/>
</svg>'''

# 循环箭头。**箭头必须落在外弧的终点上**：外弧从正上方(24,8)顺时针扫 270° 到
# 正左(8,24)，终点的切线方向朝正上，所以三角箭头在 (8,24) 指北。内弧反着走
# 270° 回来把环闭合。之前那份把三角形撇在右上角不管弧，看着就是散架的。
# 箭头三角的底边要**骑在环的厚度上**（环占 r=10..16），不是压在外沿：
# 正左那点环心在 x=24-13=11，所以三角以 x=11 居中
_ARC = ('M24 8A16 16 0 1 1 8 24L14 24A10 10 0 1 0 24 14Z', 'M11 12 17.5 24 4.5 24Z')

# 单点重测：环 + 箭头 + 中间那个"点"，与 refresh 的区别就在这个点
ICONS['retest'] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
  <path fill="{BLUE}" d="{_ARC[0]}"/>
  <path fill="{BLUE}" d="{_ARC[1]}"/>
  <circle cx="24" cy="24" r="5" fill="{DARK}"/>
</svg>'''

ICONS['refresh'] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
  <path fill="{BLUE}" d="{_ARC[0]}"/>
  <path fill="{BLUE}" d="{_ARC[1]}"/>
</svg>'''

# ── 连接 ───────────────────────────────────────────────────

ICONS['connect_all'] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
  <rect x="4" y="15" width="15" height="18" rx="3" fill="{DARK}"/>
  <rect x="19" y="18" width="9" height="4" fill="{GREY}"/>
  <rect x="19" y="26" width="9" height="4" fill="{GREY}"/>
  <rect x="29" y="12" width="15" height="24" rx="3" fill="{MID}"/>
  <rect x="32" y="15" width="9" height="18" rx="2" fill="{LIGHT}"/>
  <rect x="34.5" y="19" width="4" height="4" fill="{GREY}"/>
  <rect x="34.5" y="25" width="4" height="4" fill="{GREY}"/>
  <circle cx="26.5" cy="24" r="3.5" fill="{GREEN}"/>
</svg>'''

# 与 connect_all 同构，只把插头拉到一边、中间划一道红斜线。早先还在插头与
# 插座上各加一道红杠，24px 下红成一片——断开这件事一道斜线就够
ICONS['disconnect'] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
  <rect x="3" y="15" width="13" height="18" rx="3" fill="{DARK}"/>
  <rect x="16" y="18" width="5" height="4" fill="{GREY}"/>
  <rect x="16" y="26" width="5" height="4" fill="{GREY}"/>
  <rect x="31" y="12" width="14" height="24" rx="3" fill="{MID}"/>
  <rect x="33.5" y="15" width="9" height="18" rx="2" fill="#ECEFF1"/>
  <rect x="22" y="19" width="8" height="2.5" fill="{GREY}"/>
  <rect x="22" y="26.5" width="8" height="2.5" fill="{GREY}"/>
  <path fill="{RED}" d="M19.7 36.5 32.7 14 28.3 11.5 15.3 34Z"/>
</svg>'''

# ── 三台设备 ───────────────────────────────────────────────

ICONS['device_card'] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
  <rect x="5" y="7" width="38" height="30" rx="4" fill="{DARK}"/>
  <rect x="8" y="10" width="32" height="24" rx="2" fill="{LIGHT}"/>
  <rect x="17" y="15" width="14" height="14" rx="2" fill="{MID}"/>
  <rect x="20" y="18" width="8" height="8" rx="1" fill="{BLUE}"/>
  <path fill="{AMBER}" d="M11 37h3v6h-3zM18 37h3v6h-3zM25 37h3v6h-3zM32 37h3v6h-3z"/>
</svg>'''

ICONS['device_dmm'] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
  <rect x="7" y="4" width="34" height="40" rx="5" fill="{MID}"/>
  <rect x="11" y="8" width="26" height="15" rx="2" fill="{LIGHT}"/>
  <path fill="{DARK}" d="M15 15.5l3.5 4 3-6 3 8 3.5-5 2 2v-3h-2z"/>
  <circle cx="24" cy="33" r="8" fill="{DARK}"/>
  <circle cx="24" cy="33" r="5" fill="{BLUE}"/>
  <rect x="23" y="27" width="2" height="6" fill="{LIGHT}"/>
</svg>'''

ICONS['device_source'] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
  <rect x="4" y="9" width="40" height="30" rx="5" fill="{MID}"/>
  <rect x="8" y="13" width="32" height="14" rx="2" fill="{LIGHT}"/>
  <path fill="none" stroke="{BLUE}" stroke-width="2.6" stroke-linecap="round"
        d="M10.5 20q3.5-8 7 0t7 0 7 0 7 0"/>
  <circle cx="16" cy="33" r="4" fill="{DARK}"/>
  <circle cx="16" cy="33" r="2" fill="{AMBER}"/>
  <circle cx="32" cy="33" r="4" fill="{DARK}"/>
  <circle cx="32" cy="33" r="2" fill="{AMBER}"/>
</svg>'''

# ── 校准流程 ───────────────────────────────────────────────

ICONS['calibrate'] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
  <rect x="6" y="14" width="36" height="5" rx="2.5" fill="{MID}"/>
  <circle cx="17" cy="16.5" r="7" fill="{DARK}"/>
  <circle cx="17" cy="16.5" r="4" fill="{BLUE}"/>
  <rect x="6" y="30" width="36" height="5" rx="2.5" fill="{MID}"/>
  <circle cx="32" cy="32.5" r="7" fill="{DARK}"/>
  <circle cx="32" cy="32.5" r="4" fill="{GREEN}"/>
</svg>'''

ICONS['writeback'] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
  <path fill="{BLUE}" d="M20 6h8v14h7L24 32 13 20h7z"/>
  <rect x="8" y="34" width="32" height="10" rx="3" fill="{MID}"/>
  <rect x="12" y="37" width="24" height="4" rx="1" fill="{LIGHT}"/>
</svg>'''

ICONS['verify'] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
  <circle cx="21" cy="20" r="14" fill="{DARK}"/>
  <circle cx="21" cy="20" r="10.5" fill="{LIGHT}"/>
  <path fill="none" stroke="{GREEN}" stroke-width="3.4" stroke-linecap="round"
        stroke-linejoin="round" d="M16 20.5 19.6 24 26.5 16"/>
  <rect x="28" y="31" width="14" height="7" rx="3.5" fill="{MID}"
        transform="rotate(45 35 34.5)"/>
</svg>'''

# ── 参数 ───────────────────────────────────────────────────

ICONS['save_default'] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
  <path fill="{MID}" d="M6 10a4 4 0 0 1 4-4h24l8 8v24a4 4 0 0 1-4 4H10a4 4 0 0 1-4-4z"/>
  <rect x="15" y="6" width="16" height="14" rx="1" fill="{LIGHT}"/>
  <rect x="18" y="9" width="4" height="8" fill="{GREY}"/>
  <rect x="11" y="26" width="26" height="16" rx="2" fill="#ECEFF1"/>
  <rect x="15" y="30" width="18" height="2.5" fill="{GREY}"/>
  <rect x="15" y="35" width="18" height="2.5" fill="{GREY}"/>
</svg>'''

# 恢复默认：同一套环，反向（逆时针），灰的 —— 一眼分得出与 refresh 不同
ICONS['reset_default'] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
  <path fill="{GREY}" d="M24 8A16 16 0 1 0 40 24L34 24A10 10 0 1 1 24 14Z"/>
  <path fill="{GREY}" d="M37 12 30.5 24 43.5 24Z"/>
</svg>'''

# ── 导出 ───────────────────────────────────────────────────

ICONS['export_excel'] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
  <rect x="5" y="5" width="30" height="30" rx="3" fill="#1B5E20"/>
  <rect x="8" y="8" width="24" height="24" rx="1.5" fill="#E8F5E9"/>
  <path fill="#2E7D32" d="M8 8h24v1.6H8zM8 14.6h24V16H8zM8 21.2h24v1.6H8zM8 27.8h24V29H8z"/>
  <path fill="#2E7D32" d="M15.6 8h1.6v24h-1.6zM23.2 8H24.8v24h-1.6z"/>
  <path fill="{GREEN}" d="M30 24h8v10h5L34 44 25 34h5z"/>
</svg>'''

ICONS['export_report'] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
  <path fill="{DARK}" d="M11 4h18l10 10v20a4 4 0 0 1-4 4H11a4 4 0 0 1-4-4V8a4 4 0 0 1 4-4z"/>
  <path fill="{LIGHT}" d="M29 4l10 10H29z"/>
  <path fill="{GREY}" d="M13 20h18v2.5H13zM13 26h18v2.5H13z"/>
  <path fill="{BLUE}" d="M26.5 26h7v10h5.5L30 44.5 21 36h5.5z"/>
</svg>'''

ICONS['open_folder'] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
  <path fill="{AMBER}" d="M5 12a3 3 0 0 1 3-3h9l4 5h16a3 3 0 0 1 3 3v5H5z"/>
  <path fill="{AMBER_L}" d="M5 17h38l-4 19a3 3 0 0 1-3 2.5H9A3 3 0 0 1 6 36z"/>
</svg>'''

# ── 采集 ───────────────────────────────────────────────────

ICONS['trigger'] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
  <path fill="{AMBER}" d="M27 3 10 27h10l-1 18 17-24H26z"/>
</svg>'''

ICONS['waveform'] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
  <path fill="none" stroke="{BLUE}" stroke-width="3.2" stroke-linecap="round"
        d="M5 24c3.2-13 6.3-13 9.5 0s6.3 13 9.5 0 6.3-13 9.5 0 6.3 13 9.5 0"/>
</svg>'''

ICONS['fft'] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
  <path fill="{GREY}" d="M5 42h38v3H5z"/>
  <rect x="6" y="30" width="4.4" height="10" rx="1" fill="{BLUE}"/>
  <rect x="12.6" y="20" width="4.4" height="20" rx="1" fill="{BLUE}"/>
  <rect x="19.2" y="8" width="4.4" height="32" rx="1" fill="{DARK}"/>
  <rect x="25.8" y="24" width="4.4" height="16" rx="1" fill="{BLUE}"/>
  <rect x="32.4" y="32" width="4.4" height="8" rx="1" fill="{BLUE}"/>
  <rect x="39" y="36" width="4.4" height="4" rx="1" fill="{GREY}"/>
</svg>'''

# 有效值：波形**填成实心**，再在 RMS 高度压一根实蓝的横线。
# 画成"细线波形 + 一根线在顶上"时，24px 下那根线看着像压在天花板上，
# 读不出是"有效值电平"。填了底之后，横线是落在波形里的一个量级，才讲得通。
ICONS['rms'] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
  <path fill="{LIGHT}" d="M4 24c2.7-13 5.3-13 8 0s5.3 13 8 0 5.3-13 8 0 5.3 13 8 0z"/>
  <rect x="4" y="15" width="40" height="3.4" fill="{BLUE}"/>
</svg>'''

ICONS['autoscale'] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
  <rect x="11" y="11" width="26" height="26" rx="2" fill="none" stroke="{GREY}"
        stroke-width="2.5"/>
  <path fill="{BLUE}" d="M4 4h13v5H9v8H4zM44 4H31v5h8v8h5zM4 44h13v-5H9v-8H4zM44 44H31v-5h8v-8h5z"/>
</svg>'''

# 锁定幅度：一把锁，锁身上是那条被锁住的量程线。早先那版把坐标轴、量程条、
# 锁全塞进去，24px 下糊成一团——一个图标只讲一件事。
ICONS['lock_range'] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
  <path fill="none" stroke="{DARK}" stroke-width="5" stroke-linecap="round"
        d="M16 22v-7a8 8 0 0 1 16 0v7"/>
  <rect x="10" y="20" width="28" height="22" rx="4" fill="{RED}"/>
  <rect x="15" y="28" width="18" height="4" rx="2" fill="#FFEBEE"/>
  <circle cx="24" cy="37" r="2.4" fill="#FFEBEE"/>
</svg>'''

# ── 帮助 ───────────────────────────────────────────────────

ICONS['guide'] = f'''<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">
  <circle cx="24" cy="24" r="19" fill="{MID}"/>
  <circle cx="24" cy="24" r="15.5" fill="{LIGHT}"/>
  <path fill="{BLUE}" d="M24 12.5c-5 0-8.4 2.9-8.9 7.6h4.8c.3-2 1.7-3.2 4-3.2 2.2 0 3.7 1.2 3.7 3
    0 1.7-1 2.6-3.3 4.1-2.9 1.8-4.1 3.5-3.9 6.4l4.7 0c-.1-1.9.7-2.9 3-4.4 2.9-1.9 4.2-3.7 4.2-6.3
    0-4.4-3.5-7.2-8.3-7.2z"/>
  <circle cx="24" cy="35.5" r="2.7" fill="{BLUE}"/>
</svg>'''


def main():
    out = OUT
    if '--out' in sys.argv:
        out = sys.argv[sys.argv.index('--out') + 1]
    os.makedirs(out, exist_ok=True)
    for name, svg in sorted(ICONS.items()):
        path = os.path.join(out, name + '.svg')
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            f.write(svg + '\n')
    print('写出 %d 个图标到 %s' % (len(ICONS), out))
    for n in sorted(ICONS):
        print('  ', n)


if __name__ == '__main__':
    main()
