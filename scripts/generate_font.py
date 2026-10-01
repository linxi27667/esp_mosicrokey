"""Generate a redistributable 14px Noto Sans SC LVGL font (OFL-1.1)."""
from pathlib import Path
import argparse
from PIL import Image, ImageDraw, ImageFont

parser = argparse.ArgumentParser()
parser.add_argument("font")
parser.add_argument("output")
args = parser.parse_args()
font = ImageFont.truetype(args.font, 14)
ranges = [(0x20, 0x7E), (0x2000, 0x206F), (0x3000, 0x303F), (0x4E00, 0x9FFF), (0xFF00, 0xFFEF)]
bitmaps = bytearray()
glyphs = ["{0}"]
cmaps = []
for first, last in ranges:
    start = len(glyphs)
    cmaps.append(f"{{.range_start={first},.range_length={last-first+1},.glyph_id_start={start},.type=LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY}}")
    for code in range(first, last + 1):
        character = chr(code)
        x0, y0, x1, y1 = font.getbbox(character, anchor="ls")
        width, height = x1 - x0, y1 - y0
        image = Image.new("L", (max(1, width), max(1, height)))
        ImageDraw.Draw(image).text((-x0, -y0), character, font=font, fill=255, anchor="ls")
        pixels = [int(p >= 100) for p in image.getdata()] if width and height else []
        packed = bytearray((len(pixels)+7)//8)
        for index, value in enumerate(pixels):
            packed[index//8] |= value << (7-index%8)
        glyphs.append(f"{{.bitmap_index={len(bitmaps)},.adv_w={round(font.getlength(character)*16)},.box_w={width},.box_h={height},.ofs_x={x0},.ofs_y={-y1}}}")
        bitmaps.extend(packed)
output = Path(args.output)
output.parent.mkdir(parents=True, exist_ok=True)
with output.open("w", encoding="utf-8", newline="\n") as stream:
    stream.write("/* Generated from Noto Sans SC, SIL OFL 1.1. See assets/fonts/OFL.txt. */\n#include <lvgl.h>\n")
    stream.write("static const uint8_t bitmap[]={\n")
    for index in range(0, len(bitmaps), 32):
        stream.write(",".join(str(x) for x in bitmaps[index:index+32])+",\n")
    stream.write("};\nstatic const lv_font_fmt_txt_glyph_dsc_t glyphs[]={\n"+",\n".join(glyphs)+"\n};\n")
    stream.write("static const lv_font_fmt_txt_cmap_t cmaps[]={\n"+",\n".join(cmaps)+"\n};\n")
    stream.write("static const lv_font_fmt_txt_dsc_t descriptor={.glyph_bitmap=bitmap,.glyph_dsc=glyphs,.cmaps=cmaps,.cmap_num=5,.bpp=1};\n")
    stream.write("const lv_font_t mosicrokey_font14={.get_glyph_dsc=lv_font_get_glyph_dsc_fmt_txt,.get_glyph_bitmap=lv_font_get_bitmap_fmt_txt,.line_height=18,.base_line=4,.dsc=&descriptor};\n")
print(f"Generated {len(glyphs)-1} glyphs, {len(bitmaps)} bitmap bytes: {output}")
