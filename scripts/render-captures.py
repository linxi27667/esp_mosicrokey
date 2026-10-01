"""Convert actual LVGL RGB565 captures to PNG; render a review contact sheet."""
from pathlib import Path
import argparse
from PIL import Image, ImageDraw

parser = argparse.ArgumentParser()
parser.add_argument("input", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
files = sorted(args.input.glob("*.ppm"))
if not files:
    parser.error("No LVGL captures found; run scripts/test-host.ps1 first")
sheet = Image.new("RGB", (4 * 330, ((len(files) + 3) // 4) * 282), "#202830")
draw = ImageDraw.Draw(sheet)
for index, path in enumerate(files):
    with Image.open(path) as image:
        if image.size != (160, 128):
            raise ValueError(f"Unexpected frame size: {path}")
        image.save(args.output / f"{path.stem}.png")
        x, y = (index % 4) * 330 + 5, (index // 4) * 282 + 22
        sheet.paste(image.resize((320, 256), Image.Resampling.NEAREST), (x, y))
        draw.text((x, y - 18), path.stem, fill="white")
sheet.save(args.output / "contact-sheet.png")
print(f"Rendered {len(files)} native LVGL captures to {args.output}")
