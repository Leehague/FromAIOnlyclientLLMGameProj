"""
Art Resource Generator for Chronicle of the Ashen Crown
Reads data/art_mapping.txt and produces all 39 PNG assets specified in docs/art_resources.md
with cohesive dark-fantasy 2D pixel art and exact sprite-sheet alignment.
"""
import math
import os
import random
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAPPING_PATH = os.path.join(ROOT, "data", "art_mapping.txt")


def load_mapping():
    mapping = {}
    with open(MAPPING_PATH, "r", encoding="utf-8") as f:
        for line in f:
            s = line.strip()
            if not s or s.startswith("#"):
                continue
            parts = s.split()
            if len(parts) >= 2:
                mapping[int(parts[0])] = os.path.join(ROOT, parts[1])
    return mapping


def save_img(img: Image.Image, path: str):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path, "PNG")


def px_rect(draw, x0, y0, x1, y1, fill, outline=None):
    draw.rectangle([int(x0), int(y0), int(x1), int(y1)], fill=fill, outline=outline)


def px_ellipse(draw, x0, y0, x1, y1, fill, outline=None):
    draw.ellipse([int(x0), int(y0), int(x1), int(y1)], fill=fill, outline=outline)


def noisy_fill(img, base_rgb, variance=10, seed=1):
    rng = random.Random(seed)
    w, h = img.size
    px = img.load()
    for y in range(h):
        for x in range(w):
            d = rng.randint(-variance, variance)
            r = max(0, min(255, base_rgb[0] + d))
            g = max(0, min(255, base_rgb[1] + d))
            b = max(0, min(255, base_rgb[2] + d))
            px[x, y] = (r, g, b, 255)


# ============================================================
# 1. TILES (1..16) - 32x32
# ============================================================
def gen_tile(art_id: int) -> Image.Image:
    img = Image.new("RGBA", (32, 32), (0, 0, 0, 255))
    d = ImageDraw.Draw(img)
    rng = random.Random(100 + art_id)

    if art_id == 1:  # grass
        noisy_fill(img, (54, 98, 48), 9, seed=1)
        for _ in range(28):
            x, y = rng.randint(1, 30), rng.randint(2, 30)
            c = rng.choice([(74, 128, 60, 255), (38, 72, 35, 255), (86, 142, 68, 255)])
            d.point((x, y), fill=c)
            d.point((x, y - 1), fill=c)

    elif art_id == 2:  # dirt_path
        noisy_fill(img, (138, 106, 68), 11, seed=2)
        for _ in range(22):
            x, y = rng.randint(2, 29), rng.randint(2, 29)
            c = rng.choice([(105, 78, 48, 255), (168, 136, 94, 255)])
            d.rectangle([x, y, x + 1, y + 1], fill=c)

    elif art_id == 3:  # cobblestone
        noisy_fill(img, (86, 84, 78), 5, seed=3)
        stones = [
            (1, 1, 14, 10), (16, 1, 30, 10),
            (1, 12, 9, 21), (11, 12, 23, 21), (25, 12, 30, 21),
            (1, 23, 15, 30), (17, 23, 30, 30),
        ]
        for (x0, y0, x1, y1) in stones:
            v = rng.randint(-12, 12)
            col = (146 + v, 140 + v, 128 + v, 255)
            hi = (176 + v, 170 + v, 158 + v, 255)
            d.rectangle([x0, y0, x1, y1], fill=col, outline=(62, 60, 56, 255))
            d.line([x0 + 1, y0 + 1, x1 - 1, y0 + 1], fill=hi)

    elif art_id == 4:  # dungeon_floor
        noisy_fill(img, (52, 56, 68), 7, seed=4)
        for gy in (0, 16):
            for gx in (0, 16):
                d.rectangle([gx, gy, gx + 15, gy + 15], outline=(34, 36, 46, 255))
                d.line([gx + 1, gy + 1, gx + 14, gy + 1], fill=(70, 76, 92, 255))
        d.line([5, 6, 11, 12, 9, 14], fill=(28, 30, 38, 255))
        d.line([22, 19, 27, 25], fill=(28, 30, 38, 255))

    elif art_id == 5:  # red_carpet
        noisy_fill(img, (132, 24, 34), 8, seed=5)
        d.rectangle([0, 0, 31, 31], outline=(196, 154, 58, 255))
        d.rectangle([2, 0, 2, 31], fill=(218, 178, 76, 255))
        d.rectangle([29, 0, 29, 31], fill=(218, 178, 76, 255))
        for y in range(4, 32, 8):
            d.polygon([(16, y), (20, y + 3), (16, y + 6), (12, y + 3)], fill=(162, 36, 48, 255))

    elif art_id == 6:  # bridge
        noisy_fill(img, (108, 72, 42), 8, seed=6)
        for x in range(0, 32, 6):
            d.line([x, 0, x, 31], fill=(64, 42, 22, 255))
            d.line([x + 1, 0, x + 1, 31], fill=(134, 94, 58, 255))
        d.rectangle([0, 0, 31, 3], fill=(82, 52, 28, 255))
        d.rectangle([0, 28, 31, 31], fill=(82, 52, 28, 255))
        for px in (3, 15, 27):
            d.rectangle([px, 0, px + 2, 4], fill=(154, 110, 68, 255))
            d.rectangle([px, 27, px + 2, 31], fill=(154, 110, 68, 255))

    elif art_id == 7:  # tree
        noisy_fill(img, (44, 82, 40), 7, seed=7)
        px_ellipse(d, 4, 21, 28, 30, (22, 44, 24, 255))
        px_rect(d, 12, 17, 20, 28, (92, 58, 34, 255), (52, 32, 18, 255))
        px_ellipse(d, 3, 2, 29, 22, (28, 68, 34, 255), (16, 40, 20, 255))
        px_ellipse(d, 6, 4, 26, 18, (46, 102, 48, 255))
        px_ellipse(d, 9, 5, 22, 13, (74, 136, 64, 255))

    elif art_id == 8:  # house_wall
        noisy_fill(img, (186, 178, 162), 6, seed=8)
        d.rectangle([0, 0, 31, 31], outline=(88, 64, 44, 255))
        d.rectangle([0, 0, 3, 31], fill=(96, 68, 44, 255))
        d.rectangle([28, 0, 31, 31], fill=(96, 68, 44, 255))
        d.rectangle([0, 25, 31, 31], fill=(108, 102, 96, 255))
        # glowing warm window
        px_rect(d, 10, 7, 21, 19, (245, 200, 95, 255), (68, 46, 30, 255))
        d.line([15, 7, 15, 19], fill=(68, 46, 30, 255))
        d.line([10, 13, 21, 13], fill=(68, 46, 30, 255))

    elif art_id == 9:  # house_roof
        noisy_fill(img, (148, 56, 44), 8, seed=9)
        for y in range(0, 32, 6):
            d.line([0, y, 31, y], fill=(96, 32, 24, 255))
            d.line([0, y + 1, 31, y + 1], fill=(182, 82, 64, 255))
            off = (y // 6) % 2 * 4
            for x in range(off, 32, 8):
                d.line([x, y, x, y + 5], fill=(108, 38, 28, 255))

    elif art_id == 10:  # dungeon_wall
        noisy_fill(img, (34, 38, 52), 6, seed=10)
        d.rectangle([0, 0, 31, 7], fill=(58, 64, 86, 255))
        d.line([0, 8, 31, 8], fill=(18, 20, 30, 255))
        d.rectangle([1, 10, 15, 20], fill=(44, 48, 66, 255), outline=(20, 22, 32, 255))
        d.rectangle([17, 10, 30, 20], fill=(40, 44, 60, 255), outline=(20, 22, 32, 255))
        d.rectangle([1, 22, 22, 30], fill=(38, 42, 58, 255), outline=(20, 22, 32, 255))
        d.rectangle([24, 22, 30, 30], fill=(46, 50, 68, 255), outline=(20, 22, 32, 255))

    elif art_id == 11:  # water
        noisy_fill(img, (36, 102, 148), 7, seed=11)
        for y, x0 in [(6, 4), (14, 14), (23, 6), (28, 18)]:
            d.line([x0, y, x0 + 9, y], fill=(118, 198, 232, 255))
            d.line([x0 + 2, y + 1, x0 + 7, y + 1], fill=(175, 232, 250, 255))

    elif art_id == 12:  # lava
        noisy_fill(img, (176, 42, 16), 14, seed=12)
        px_ellipse(d, 3, 4, 18, 16, (238, 118, 22, 255))
        px_ellipse(d, 7, 7, 14, 13, (255, 210, 65, 255))
        px_ellipse(d, 14, 17, 29, 28, (232, 105, 20, 255))
        px_ellipse(d, 18, 20, 25, 25, (255, 225, 85, 255))

    elif art_id == 13:  # flower_grass
        img = gen_tile(1)
        d = ImageDraw.Draw(img)
        flowers = [(7, 8, (250, 225, 75, 255)), (22, 10, (245, 130, 165, 255)),
                   (13, 22, (250, 250, 245, 255)), (25, 24, (250, 215, 70, 255))]
        for fx, fy, fc in flowers:
            d.point((fx, fy), fill=(255, 255, 255, 255))
            for ox, oy in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                d.point((fx + ox, fy + oy), fill=fc)

    elif art_id == 14:  # fountain
        img = gen_tile(3)
        d = ImageDraw.Draw(img)
        px_ellipse(d, 2, 3, 29, 29, (124, 128, 136, 255), (56, 60, 68, 255))
        px_ellipse(d, 6, 7, 25, 25, (64, 176, 232, 255), (42, 118, 168, 255))
        px_ellipse(d, 11, 11, 20, 20, (165, 235, 255, 255))
        px_rect(d, 14, 8, 17, 18, (220, 248, 255, 255))

    elif art_id == 15:  # portal_field (blue arcane)
        img = gen_tile(3)
        d = ImageDraw.Draw(img)
        px_ellipse(d, 2, 2, 29, 29, (30, 88, 168, 255), (120, 210, 255, 255))
        px_ellipse(d, 7, 7, 24, 24, (72, 185, 250, 255))
        px_ellipse(d, 12, 12, 19, 19, (220, 248, 255, 255))

    elif art_id == 16:  # portal_boss (crimson rune)
        img = gen_tile(4)
        d = ImageDraw.Draw(img)
        px_ellipse(d, 2, 2, 29, 29, (120, 18, 28, 255), (255, 92, 68, 255))
        px_ellipse(d, 7, 7, 24, 24, (220, 48, 36, 255))
        px_ellipse(d, 12, 12, 19, 19, (255, 215, 115, 255))

    return img


# ============================================================
# 2. PLAYER 8-DIRECTION SPRITE SHEETS (101..103) - 192x384 (48x48 x 4x8)
# Rows: 0:S, 1:SW, 2:W, 3:NW, 4:N, 5:NE, 6:E, 7:SE
# ============================================================
DIR_ANGLES = [
    (0.0, 1.0),          # 0 Down (S)
    (-0.707, 0.707),     # 1 DownLeft (SW)
    (-1.0, 0.0),         # 2 Left (W)
    (-0.707, -0.707),    # 3 UpLeft (NW)
    (0.0, -1.0),         # 4 Up (N)
    (0.707, -0.707),     # 5 UpRight (NE)
    (1.0, 0.0),          # 6 Right (E)
    (0.707, 0.707),      # 7 DownRight (SE)
]


def draw_knight_frame(draw, ox, oy, dir_idx, frame_idx, mode):
    fx, fy = DIR_ANGLES[dir_idx]
    cx, cy = ox + 24, oy + 26

    # Animation offsets
    bob = 0
    leg_l = 0
    leg_r = 0
    arm_swing = 0
    if mode == "idle":
        bob = [0, -1, -1, 0][frame_idx]
    elif mode == "walk":
        bob = [0, -2, 0, -2][frame_idx]
        leg_l = [0, -3, 0, 3][frame_idx]
        leg_r = [0, 3, 0, -3][frame_idx]
        arm_swing = [0, 2, 0, -2][frame_idx]
    elif mode == "attack":
        bob = [0, -1, 1, 0][frame_idx]

    facing_up = fy < -0.2
    facing_down = fy > 0.2
    facing_left = fx < -0.2
    facing_right = fx > 0.2

    # Palette
    outline = (20, 22, 30, 255)
    steel_dark = (112, 124, 146, 255)
    steel_mid = (168, 182, 206, 255)
    steel_hi = (224, 234, 248, 255)
    cape_dark = (26, 62, 128, 255)
    cape_mid = (44, 102, 196, 255)
    cape_hi = (78, 142, 235, 255)
    gold = (228, 184, 72, 255)
    boot = (62, 46, 36, 255)
    visor = (32, 38, 52, 255)
    eye_glow = (120, 225, 255, 255)

    # Helper to draw cape (behind when facing down/side, in front when facing up)
    def draw_cape():
        flutter = [0, 1, 0, -1][frame_idx] if mode != "idle" else (1 if frame_idx in (1, 2) else 0)
        cape_dx = int(-fx * 5)
        cape_dy = int(-fy * 2)
        pts = [
            (cx - 7 + cape_dx, cy - 5 + bob),
            (cx + 7 + cape_dx, cy - 5 + bob),
            (cx + 9 + cape_dx + flutter, cy + 11 + bob + cape_dy),
            (cx - 9 + cape_dx - flutter, cy + 11 + bob + cape_dy),
        ]
        draw.polygon(pts, fill=cape_mid, outline=outline)
        draw.line([cx - 4 + cape_dx, cy - 2 + bob, cx - 5 + cape_dx, cy + 9 + bob], fill=cape_hi)
        draw.line([cx + 4 + cape_dx, cy - 2 + bob, cx + 5 + cape_dx, cy + 9 + bob], fill=cape_dark)

    # Helper to draw sword & attack slash arc
    def draw_sword():
        # Right hand side relative to facing vector
        rx, ry = -fy, fx
        base_x = cx + fx * 6 + rx * 7
        base_y = cy - 1 + bob + fy * 4 + ry * 4

        if mode == "attack":
            # Sweeping slash from -65 deg to +65 deg across facing angle
            base_ang = math.atan2(fy, fx)
            sweep = [-1.15, -0.25, 0.75, 1.15][frame_idx]
            ang = base_ang + sweep
            tip_x = cx + math.cos(ang) * 20
            tip_y = cy + bob + math.sin(ang) * 20
            hilt_x = cx + math.cos(ang) * 7
            hilt_y = cy + bob + math.sin(ang) * 7
            draw.line([hilt_x, hilt_y, tip_x, tip_y], fill=steel_hi, width=3)
            draw.line([hilt_x, hilt_y, tip_x, tip_y], fill=(255, 255, 255, 255), width=1)
            px_ellipse(draw, hilt_x - 2, hilt_y - 2, hilt_x + 2, hilt_y + 2, gold, outline)
            # Glowing slash trail on frames 1 & 2
            if frame_idx in (1, 2):
                prev_ang = base_ang + sweep - 0.85
                for step in range(6):
                    t = step / 5.0
                    a = prev_ang + (ang - prev_ang) * t
                    sx = cx + math.cos(a) * 18
                    sy = cy + bob + math.sin(a) * 18
                    px_ellipse(draw, sx - 2, sy - 2, sx + 2, sy + 2, (160, 235, 255, 200))
        else:
            # Carried sword angled in facing direction
            ang = math.atan2(fy, fx) + 0.55
            sx0 = base_x + arm_swing * (0.4 if abs(fx) > 0.5 else 0.0)
            sy0 = base_y + arm_swing * (0.4 if abs(fy) > 0.5 else 0.0)
            sx1 = sx0 + math.cos(ang) * 11
            sy1 = sy0 + math.sin(ang) * 11
            draw.line([sx0, sy0, sx1, sy1], fill=steel_hi, width=2)
            px_rect(draw, sx0 - 2, sy0 - 1, sx0 + 2, sy0 + 1, gold)

    # Helper to draw shield (on left hand side relative to facing)
    def draw_shield():
        lx, ly = fy, -fx
        shx = cx + fx * 4 + lx * 8
        shy = cy + 1 + bob + fy * 3 + ly * 3
        px_ellipse(draw, shx - 4, shy - 5, shx + 4, shy + 5, cape_dark, outline)
        px_ellipse(draw, shx - 2, shy - 3, shx + 2, shy + 3, gold)

    # Layer ordering depending on facing direction
    if not facing_up:
        draw_cape()
    if facing_up:
        draw_sword()
        draw_shield()

    # Legs / Boots (animated walk stride)
    if abs(fx) > 0.6 and abs(fy) < 0.3:
        # Side walk stride along X
        px_rect(draw, cx - 3 + leg_l, cy + 7, cx + 1 + leg_l, cy + 15, boot, outline)
        px_rect(draw, cx - 1 + leg_r, cy + 7, cx + 3 + leg_r, cy + 15, boot, outline)
    else:
        # Front/Back/Diagonal stride
        px_rect(draw, cx - 6, cy + 7 + (leg_l // 2), cx - 1, cy + 15 + (leg_l // 2), boot, outline)
        px_rect(draw, cx + 1, cy + 7 + (leg_r // 2), cx + 6, cy + 15 + (leg_r // 2), boot, outline)

    # Armored Torso
    px_rect(draw, cx - 7, cy - 5 + bob, cx + 7, cy + 8 + bob, steel_mid, outline)
    px_rect(draw, cx - 5, cy - 3 + bob, cx + 5, cy + 4 + bob, steel_hi)
    # Belt & gold buckle
    px_rect(draw, cx - 7, cy + 4 + bob, cx + 7, cy + 7 + bob, (78, 52, 34, 255))
    px_rect(draw, cx - 2, cy + 4 + bob, cx + 2, cy + 7 + bob, gold)
    # Shoulder pauldrons
    px_ellipse(draw, cx - 10, cy - 6 + bob, cx - 4, cy + 1 + bob, steel_dark, outline)
    px_ellipse(draw, cx + 4, cy - 6 + bob, cx + 10, cy + 1 + bob, steel_dark, outline)

    # Helmet / Head
    hx, hy = cx, cy - 11 + bob
    px_ellipse(draw, hx - 7, hy - 7, hx + 7, hy + 6, steel_mid, outline)
    px_ellipse(draw, hx - 5, hy - 6, hx + 5, hy - 1, steel_hi)
    # Helmet plume (shifts slightly opposite facing direction)
    plume_x = hx - int(fx * 3)
    plume_y = hy - 8
    px_ellipse(draw, plume_x - 3, plume_y - 3, plume_x + 3, plume_y + 2, cape_hi, outline)

    # Visor & glowing eyes (visible when not facing straight up)
    if not facing_up:
        vx = hx + int(fx * 4)
        vy = hy + (1 if facing_down else 0)
        vw = 4 if (facing_left or facing_right) else 5
        px_rect(draw, vx - vw, vy - 1, vx + vw, vy + 2, visor)
        if facing_left and not facing_down:
            draw.point((vx - 2, vy), fill=eye_glow)
        elif facing_right and not facing_down:
            draw.point((vx + 2, vy), fill=eye_glow)
        else:
            draw.point((vx - 2, vy), fill=eye_glow)
            draw.point((vx + 2, vy), fill=eye_glow)

    if facing_up:
        draw_cape()
    else:
        draw_shield()
        draw_sword()


def gen_player_sheet(mode: str) -> Image.Image:
    img = Image.new("RGBA", (48 * 4, 48 * 8), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    for row in range(8):
        for col in range(4):
            draw_knight_frame(d, col * 48, row * 48, row, col, mode)
    return img


# ============================================================
# 3. FIELD MONSTERS (201..203) - 160x40 (40x40 x 4)
# ============================================================
def gen_monster_sheet(art_id: int) -> Image.Image:
    img = Image.new("RGBA", (160, 40), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    outline = (20, 20, 26, 255)

    for f in range(4):
        ox = f * 40
        cx, cy = ox + 20, 22
        if art_id == 201:  # slime
            squish = [0, 3, 1, -2][f]
            lift = [0, 0, -3, -2][f]
            px_ellipse(d, cx - 13 - squish, cy - 8 + squish + lift, cx + 13 + squish, cy + 12 + lift,
                       (105, 118, 138, 235), outline)
            px_ellipse(d, cx - 9 - squish, cy - 5 + squish + lift, cx + 8 + squish, cy + 5 + lift,
                       (148, 165, 190, 230))
            px_ellipse(d, cx - 6, cy - 4 + lift, cx - 2, cy - 1 + lift, (220, 235, 250, 240))
            # Glowing crimson eyes
            px_rect(d, cx - 6, cy + 1 + lift, cx - 3, cy + 4 + lift, (240, 45, 55, 255))
            px_rect(d, cx + 3, cy + 1 + lift, cx + 6, cy + 4 + lift, (240, 45, 55, 255))

        elif art_id == 202:  # goblin
            bob = [0, -2, 0, -2][f]
            stride = [0, 3, 0, -3][f]
            skin = (92, 152, 72, 255)
            skin_hi = (126, 188, 96, 255)
            tunic = (118, 72, 42, 255)
            # Legs
            px_rect(d, cx - 5 + stride, cy + 6, cx - 1 + stride, cy + 14, (65, 42, 28, 255), outline)
            px_rect(d, cx + 1 - stride, cy + 6, cx + 5 - stride, cy + 14, (65, 42, 28, 255), outline)
            # Body
            px_rect(d, cx - 6, cy - 4 + bob, cx + 6, cy + 7 + bob, tunic, outline)
            # Pointed ears
            d.polygon([(cx - 12, cy - 10 + bob), (cx - 5, cy - 12 + bob), (cx - 5, cy - 7 + bob)], fill=skin, outline=outline)
            d.polygon([(cx + 12, cy - 10 + bob), (cx + 5, cy - 12 + bob), (cx + 5, cy - 7 + bob)], fill=skin, outline=outline)
            # Head
            px_ellipse(d, cx - 6, cy - 14 + bob, cx + 6, cy - 3 + bob, skin, outline)
            px_ellipse(d, cx - 4, cy - 12 + bob, cx + 4, cy - 8 + bob, skin_hi)
            # Yellow menacing eyes
            px_rect(d, cx - 4, cy - 9 + bob, cx - 1, cy - 7 + bob, (255, 225, 60, 255))
            px_rect(d, cx + 1, cy - 9 + bob, cx + 4, cy - 7 + bob, (255, 225, 60, 255))
            # Dagger
            d.line([cx + 7, cy + bob, cx + 14, cy - 5 + bob - stride // 2], fill=(210, 220, 232, 255), width=2)

        elif art_id == 203:  # skeleton
            bob = [0, -1, 0, -1][f]
            stride = [0, 2, 0, -2][f]
            bone = (224, 220, 206, 255)
            bone_dk = (165, 160, 146, 255)
            # Legs
            d.line([cx - 3 + stride, cy + 6, cx - 3 + stride, cy + 14], fill=bone, width=2)
            d.line([cx + 3 - stride, cy + 6, cx + 3 - stride, cy + 14], fill=bone, width=2)
            # Ribcage
            px_rect(d, cx - 5, cy - 4 + bob, cx + 5, cy + 6 + bob, (46, 48, 58, 255), outline)
            for ry in (-2, 1, 4):
                d.line([cx - 4, cy + ry + bob, cx + 4, cy + ry + bob], fill=bone)
            # Skull
            px_ellipse(d, cx - 6, cy - 15 + bob, cx + 6, cy - 4 + bob, bone, outline)
            px_rect(d, cx - 4, cy - 11 + bob, cx - 1, cy - 8 + bob, (24, 18, 28, 255))
            px_rect(d, cx + 1, cy - 11 + bob, cx + 4, cy - 8 + bob, (24, 18, 28, 255))
            d.point((cx - 2, cy - 9 + bob), fill=(255, 90, 65, 255))
            d.point((cx + 2, cy - 9 + bob), fill=(255, 90, 65, 255))
            # Rusty shield & axe
            px_ellipse(d, cx - 12, cy - 4 + bob, cx - 5, cy + 6 + bob, (118, 72, 54, 255), outline)
            d.line([cx + 7, cy + 4 + bob, cx + 12, cy - 9 + bob], fill=(120, 82, 48, 255), width=2)
            d.polygon([(cx + 10, cy - 11 + bob), (cx + 17, cy - 8 + bob), (cx + 12, cy - 3 + bob)], fill=bone_dk, outline=outline)

    return img


# ============================================================
# 4. BOSS MONSTER (301) - 320x160 (80x80 x 4 cols x 2 rows)
# Row 0: Phase 1, Row 1: Phase 2 (Fiery Enraged)
# ============================================================
def gen_boss_sheet() -> Image.Image:
    img = Image.new("RGBA", (320, 160), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    outline = (16, 12, 18, 255)

    for row in range(2):
        enraged = (row == 1)
        for col in range(4):
            ox, oy = col * 80, row * 80
            cx, cy = ox + 40, oy + 44
            bob = [0, -3, -1, 2][col]

            # Fiery aura in Phase 2
            if enraged:
                for i in range(6):
                    ang = col * 0.8 + i * (math.pi / 3.0)
                    fx = cx + math.cos(ang) * 28
                    fy = cy + math.sin(ang) * 24 + bob
                    px_ellipse(d, fx - 6, fy - 6, fx + 6, fy + 6, (255, 90, 20, 150))

            # Tattered crimson cape
            d.polygon([
                (cx - 20, cy - 12 + bob), (cx + 20, cy - 12 + bob),
                (cx + 28, cy + 26 + bob), (cx - 28, cy + 26 + bob)
            ], fill=(118, 22, 30, 255) if not enraged else (185, 38, 22, 255), outline=outline)

            # Heavy armored legs
            px_rect(d, cx - 14, cy + 12, cx - 4, cy + 28, (46, 44, 54, 255), outline)
            px_rect(d, cx + 4, cy + 12, cx + 14, cy + 28, (46, 44, 54, 255), outline)

            # Obsidian Armor Torso
            px_rect(d, cx - 18, cy - 14 + bob, cx + 18, cy + 14 + bob, (54, 52, 66, 255), outline)
            px_rect(d, cx - 13, cy - 9 + bob, cx + 13, cy + 7 + bob, (78, 76, 94, 255))
            # Glowing core rune on chest
            core_c = (255, 110, 45, 255) if not enraged else (255, 225, 75, 255)
            d.polygon([(cx, cy - 7 + bob), (cx + 6, cy + bob), (cx, cy + 7 + bob), (cx - 6, cy + bob)], fill=core_c)

            # Massive Spiked Pauldrons
            px_ellipse(d, cx - 28, cy - 18 + bob, cx - 12, cy - 1 + bob, (42, 40, 52, 255), outline)
            px_ellipse(d, cx + 12, cy - 18 + bob, cx + 28, cy - 1 + bob, (42, 40, 52, 255), outline)
            d.polygon([(cx - 24, cy - 17 + bob), (cx - 30, cy - 26 + bob), (cx - 17, cy - 16 + bob)], fill=(190, 180, 165, 255), outline=outline)
            d.polygon([(cx + 24, cy - 17 + bob), (cx + 30, cy - 26 + bob), (cx + 17, cy - 16 + bob)], fill=(190, 180, 165, 255), outline=outline)

            # Crowned Horned Helmet
            px_ellipse(d, cx - 12, cy - 30 + bob, cx + 12, cy - 10 + bob, (48, 46, 58, 255), outline)
            # Ashen Crown horns
            d.polygon([(cx - 11, cy - 26 + bob), (cx - 18, cy - 40 + bob), (cx - 5, cy - 28 + bob)], fill=(215, 172, 68, 255), outline=outline)
            d.polygon([(cx + 11, cy - 26 + bob), (cx + 18, cy - 40 + bob), (cx + 5, cy - 28 + bob)], fill=(215, 172, 68, 255), outline=outline)
            d.polygon([(cx - 4, cy - 28 + bob), (cx, cy - 39 + bob), (cx + 4, cy - 28 + bob)], fill=(235, 195, 85, 255), outline=outline)
            # Glowing eyes
            eye_c = (255, 65, 45, 255) if not enraged else (255, 240, 95, 255)
            px_rect(d, cx - 7, cy - 22 + bob, cx - 2, cy - 19 + bob, eye_c)
            px_rect(d, cx + 2, cy - 22 + bob, cx + 7, cy - 19 + bob, eye_c)

            # Flaming Greatsword
            sw_y = [-4, 2, 4, -2][col]
            d.line([cx + 22, cy + 4 + bob + sw_y, cx + 33, cy - 26 + bob + sw_y],
                   fill=(255, 125, 35, 255) if enraged else (190, 200, 215, 255), width=5)
            d.line([cx + 22, cy + 4 + bob + sw_y, cx + 33, cy - 26 + bob + sw_y],
                   fill=(255, 235, 120, 255) if enraged else (240, 245, 255, 255), width=2)
            px_rect(d, cx + 17, cy + 2 + bob + sw_y, cx + 27, cy + 6 + bob + sw_y, (215, 172, 68, 255), outline)

    return img


# ============================================================
# 5. NPCs (401..403) - 160x40 (40x40 x 4)
# ============================================================
def gen_npc_sheet(art_id: int) -> Image.Image:
    img = Image.new("RGBA", (160, 40), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    outline = (22, 22, 30, 255)
    skin = (238, 198, 164, 255)

    for f in range(4):
        ox = f * 40
        cx, cy = ox + 20, 22
        bob = [0, -1, -1, 0][f]

        if art_id == 401:  # Elder Eldric
            px_rect(d, cx - 7, cy - 4 + bob, cx + 7, cy + 13, (112, 78, 52, 255), outline)
            px_rect(d, cx - 3, cy - 2 + bob, cx + 3, cy + 12, (168, 128, 72, 255))
            px_ellipse(d, cx - 6, cy - 14 + bob, cx + 6, cy - 3 + bob, skin, outline)
            # White hair & beard
            px_ellipse(d, cx - 7, cy - 16 + bob, cx + 7, cy - 10 + bob, (235, 235, 240, 255))
            d.polygon([(cx - 5, cy - 5 + bob), (cx + 5, cy - 5 + bob), (cx, cy + 3 + bob)], fill=(235, 235, 240, 255))
            # Staff
            d.line([cx + 10, cy - 12 + bob, cx + 10, cy + 13], fill=(138, 92, 48, 255), width=2)
            px_ellipse(d, cx + 8, cy - 15 + bob, cx + 12, cy - 11 + bob, (255, 215, 75, 255))

        elif art_id == 402:  # Merchant Mara
            px_rect(d, cx - 7, cy - 4 + bob, cx + 7, cy + 13, (152, 48, 54, 255), outline)
            px_rect(d, cx - 4, cy + bob, cx + 4, cy + 12, (228, 212, 182, 255))
            px_ellipse(d, cx - 6, cy - 14 + bob, cx + 6, cy - 3 + bob, skin, outline)
            px_ellipse(d, cx - 7, cy - 16 + bob, cx + 7, cy - 10 + bob, (112, 62, 34, 255))
            # Coin pouch
            px_ellipse(d, cx + 6, cy + 2 + bob, cx + 13, cy + 9 + bob, (228, 184, 62, 255), outline)

        elif art_id == 403:  # Healer Sera
            px_rect(d, cx - 7, cy - 4 + bob, cx + 7, cy + 13, (232, 242, 252, 255), outline)
            px_rect(d, cx - 3, cy - 3 + bob, cx + 3, cy + 12, (92, 182, 235, 255))
            px_ellipse(d, cx - 6, cy - 14 + bob, cx + 6, cy - 3 + bob, skin, outline)
            px_ellipse(d, cx - 7, cy - 16 + bob, cx + 7, cy - 9 + bob, (238, 206, 118, 255))
            # Healing staff with cyan gem
            d.line([cx - 10, cy - 12 + bob, cx - 10, cy + 13], fill=(205, 198, 178, 255), width=2)
            px_ellipse(d, cx - 13, cy - 16 + bob, cx - 7, cy - 10 + bob, (95, 235, 255, 255), outline)

        # Eyes
        d.point((cx - 2, cy - 9 + bob), fill=(25, 25, 35, 255))
        d.point((cx + 2, cy - 9 + bob), fill=(25, 25, 35, 255))

    return img


# ============================================================
# 6. ITEM ICONS (501..509) - 32x32
# ============================================================
def gen_item_icon(art_id: int) -> Image.Image:
    img = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    outline = (20, 20, 28, 255)

    if art_id in (501, 502):  # Potions
        hi = (art_id == 502)
        px_rect(d, 13, 4, 18, 8, (186, 140, 74, 255) if not hi else (238, 196, 68, 255), outline)
        px_rect(d, 12, 8, 19, 12, (190, 215, 235, 255), outline)
        r = 10 if hi else 8
        px_ellipse(d, 16 - r, 20 - r + 1, 16 + r, 20 + r - 1, (220, 38, 52, 255) if not hi else (245, 52, 92, 255), outline)
        px_ellipse(d, 16 - r + 3, 20 - r + 3, 14, 18, (255, 165, 175, 255))

    elif art_id in (503, 504, 505):  # Swords
        blade_col = {503: (178, 186, 198, 255), 504: (218, 232, 248, 255), 505: (105, 222, 255, 255)}[art_id]
        hilt_col = {503: (140, 102, 58, 255), 504: (224, 184, 72, 255), 505: (245, 210, 95, 255)}[art_id]
        d.line([7, 25, 25, 7], fill=outline, width=6)
        d.line([9, 23, 25, 7], fill=blade_col, width=4)
        d.line([11, 21, 25, 7], fill=(255, 255, 255, 255), width=1)
        d.line([7, 17, 15, 25], fill=hilt_col, width=3)
        d.line([5, 27, 10, 22], fill=(95, 62, 36, 255), width=3)

    elif art_id in (506, 507, 508):  # Armors
        main_col = {506: (146, 94, 54, 255), 507: (172, 184, 206, 255), 508: (56, 54, 72, 255)}[art_id]
        trim_col = {506: (192, 142, 86, 255), 507: (226, 192, 82, 255), 508: (245, 118, 42, 255)}[art_id]
        px_rect(d, 7, 7, 24, 25, main_col, outline)
        px_rect(d, 4, 8, 9, 15, main_col, outline)
        px_rect(d, 22, 8, 27, 15, main_col, outline)
        px_rect(d, 11, 10, 20, 18, trim_col)
        px_rect(d, 13, 12, 18, 16, main_col)

    elif art_id == 509:  # Ashen Crown
        d.polygon([
            (4, 24), (5, 10), (11, 16), (16, 7), (21, 16), (27, 10), (28, 24)
        ], fill=(232, 186, 68, 255), outline=outline)
        px_rect(d, 5, 21, 27, 25, (186, 138, 42, 255), outline)
        px_ellipse(d, 13, 15, 18, 20, (235, 42, 55, 255), outline)
        px_ellipse(d, 7, 17, 10, 20, (92, 205, 255, 255))
        px_ellipse(d, 22, 17, 25, 20, (92, 205, 255, 255))

    return img


# ============================================================
# 7. EFFECTS & UI ICONS (601..604)
# ============================================================
def gen_effect(art_id: int) -> Image.Image:
    if art_id == 604:  # Coin 32x32
        img = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
        px_ellipse(d, 5, 5, 26, 26, (235, 182, 42, 255), (96, 66, 18, 255))
        px_ellipse(d, 9, 9, 22, 22, (255, 225, 95, 255))
        px_rect(d, 14, 11, 17, 20, (215, 152, 28, 255))
        return img

    img = Image.new("RGBA", (128, 32), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    for f in range(4):
        ox = f * 32
        cx, cy = ox + 16, 16
        if art_id == 601:  # Slash arc
            w = 5 - f
            start_a = -70 + f * 12
            end_a = 70 + f * 12
            d.arc([ox + 3, 3, ox + 28, 28], start=start_a, end=end_a,
                  fill=(215, 245, 255, max(80, 245 - f * 45)), width=max(2, w))
            d.arc([ox + 6, 6, ox + 25, 25], start=start_a + 10, end=end_a - 10,
                  fill=(255, 255, 255, max(60, 220 - f * 50)), width=2)

        elif art_id == 602:  # Whirlwind ring
            r = 7 + f * 2
            a = max(70, 240 - f * 45)
            d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=(255, 220, 95, a), width=3)
            d.ellipse([cx - r + 3, cy - r + 3, cx + r - 3, cy + r - 3], outline=(140, 230, 255, a), width=2)

        elif art_id == 603:  # Fireball
            r = 8 + (f % 2)
            px_ellipse(d, cx - r, cy - r, cx + r, cy + r, (240, 72, 24, 240))
            px_ellipse(d, cx - r + 3, cy - r + 3, cx + r - 3, cy + r - 3, (255, 175, 40, 255))
            px_ellipse(d, cx - 3, cy - 3, cx + 3, cy + 3, (255, 248, 190, 255))

    return img


def main():
    mapping = load_mapping()
    generated = 0
    for art_id, out_path in sorted(mapping.items()):
        if 1 <= art_id <= 16:
            img = gen_tile(art_id)
        elif art_id == 101:
            img = gen_player_sheet("idle")
        elif art_id == 102:
            img = gen_player_sheet("walk")
        elif art_id == 103:
            img = gen_player_sheet("attack")
        elif 201 <= art_id <= 203:
            img = gen_monster_sheet(art_id)
        elif art_id == 301:
            img = gen_boss_sheet()
        elif 401 <= art_id <= 403:
            img = gen_npc_sheet(art_id)
        elif 501 <= art_id <= 509:
            img = gen_item_icon(art_id)
        elif 601 <= art_id <= 604:
            img = gen_effect(art_id)
        else:
            continue

        save_img(img, out_path)
        generated += 1

    print(f"[generate_art] Generated {generated} / {len(mapping)} PNG art assets from data/art_mapping.txt")


if __name__ == "__main__":
    main()
