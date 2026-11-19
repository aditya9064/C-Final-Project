import sys
from PIL import Image, ImageDraw, ImageFont
# args: out.png  HEIGHT  label1 img1 label2 img2 ...
out = sys.argv[1]; Hh = int(sys.argv[2]); rest = sys.argv[3:]
pairs = [(rest[i], rest[i+1]) for i in range(0, len(rest), 2)]
GAP=14; LBL=30; BG=(158,160,162)
tiles=[]
for label, path in pairs:
    im = Image.open(path).convert("RGB")
    w = int(im.width * Hh/im.height); im = im.resize((w, Hh))
    tiles.append((label, im))
Wtot = sum(t[1].width for t in tiles) + GAP*(len(tiles)+1)
canvas = Image.new("RGB", (Wtot, Hh+LBL+GAP), BG)
d = ImageDraw.Draw(canvas)
try: font = ImageFont.truetype("arialbd.ttf", 20)
except Exception: font = ImageFont.load_default()
x = GAP
for label, im in tiles:
    canvas.paste(im, (x, LBL))
    d.text((x+4, 6), label, fill=(20,20,20), font=font)
    x += im.width + GAP
canvas.save(out); print("SAVED", out, canvas.size)
