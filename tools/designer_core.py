"""
Core backend for Chronicle of the Ashen Crown - Game Designer Tool.
Shared by BOTH the Desktop GUI (tools/designer_ui.py) and CLI (tools/designer_tool.py).

Features:
1. Map creation, editing, flood-fill, resize, validation (data/maps/*.txt + data/maps.json)
2. Art resource upload, art_mapping.txt registration, docs/art_resources.md sync, verification
3. Art resource requirement tracking (data/art_requests.json) for future LLM image generation
4. Game design JSON document viewer, search (single & global), structured editing, validation
"""
from __future__ import annotations

import copy
import datetime
import json
import os
import re
import shutil
from collections import deque
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

from PIL import Image

# Category -> (id_min, id_max, assets_subdir, default_size)
ART_CATEGORIES: Dict[str, Tuple[int, int, str, str]] = {
    "tiles":      (1,   99,  "assets/tiles",      "32x32"),
    "characters": (101, 199, "assets/characters", "192x384"),
    "monsters":   (201, 299, "assets/monsters",   "160x40"),
    "boss":       (301, 399, "assets/boss",       "320x160"),
    "npcs":       (401, 499, "assets/npcs",       "160x40"),
    "items":      (501, 599, "assets/items",      "32x32"),
    "effects":    (601, 699, "assets/effects",    "128x32"),
}

# Supported Design JSON documents: doc_key -> metadata
# mode: "list" (root[collection_key] is list of objects with id_field),
#       "object" (root is a single spec object like player.json),
#       "kv" (root[collection_key] is dict of key -> string like strings.json)
DESIGN_DOCS: Dict[str, Dict[str, Any]] = {
    "items":    {"file": "data/items.json",    "label": "아이템 (items.json)",      "mode": "list",   "collection": "items",    "id_field": "id",  "name_field": "name"},
    "monsters": {"file": "data/monsters.json", "label": "필드 몬스터 (monsters.json)", "mode": "list", "collection": "monsters", "id_field": "id",  "name_field": "name"},
    "bosses":   {"file": "data/bosses.json",   "label": "보스 몬스터 (bosses.json)",   "mode": "list", "collection": "bosses",   "id_field": "id",  "name_field": "name"},
    "npcs":     {"file": "data/npcs.json",     "label": "NPC (npcs.json)",          "mode": "list",   "collection": "npcs",     "id_field": "id",  "name_field": "name"},
    "quests":   {"file": "data/quests.json",   "label": "퀘스트 (quests.json)",      "mode": "list",   "collection": "quests",   "id_field": "id",  "name_field": "title"},
    "maps":     {"file": "data/maps.json",     "label": "맵 메타데이터 (maps.json)", "mode": "list",   "collection": "maps",     "id_field": "id",  "name_field": "name"},
    "tiles":    {"file": "data/tiles.json",    "label": "타일 정의 (tiles.json)",    "mode": "list",   "collection": "tiles",    "id_field": "id",  "name_field": "name"},
    "player":   {"file": "data/player.json",   "label": "플레이어 스펙 (player.json)","mode": "object", "collection": None,       "id_field": None,  "name_field": "name"},
    "strings":  {"file": "data/strings.json",  "label": "UI 문자열 (strings.json)",  "mode": "kv",     "collection": "strings",  "id_field": None,  "name_field": None},
}


def now_iso() -> str:
    return datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")


class DesignerCore:
    def __init__(self, project_root: Optional[str | Path] = None):
        if project_root is None:
            self.root = Path(__file__).resolve().parent.parent
        else:
            self.root = Path(project_root).resolve()

        self.data_dir = self.root / "data"
        self.maps_dir = self.data_dir / "maps"
        self.docs_dir = self.root / "docs"
        self.assets_dir = self.root / "assets"
        self.art_mapping_path = self.data_dir / "art_mapping.txt"
        self.art_requests_path = self.data_dir / "art_requests.json"
        self.ui_layout_path = self.data_dir / "ui_layout.json"
        self.art_doc_path = self.docs_dir / "art_resources.md"

    # ------------------------------------------------------------------
    # Low-level JSON I/O
    # ------------------------------------------------------------------
    def _read_json(self, rel_path: str | Path) -> Any:
        p = self.root / rel_path if not Path(rel_path).is_absolute() else Path(rel_path)
        with open(p, "r", encoding="utf-8") as f:
            return json.load(f)

    def _write_json(self, rel_path: str | Path, data: Any) -> None:
        p = self.root / rel_path if not Path(rel_path).is_absolute() else Path(rel_path)
        p.parent.mkdir(parents=True, exist_ok=True)
        with open(p, "w", encoding="utf-8", newline="\n") as f:
            json.dump(data, f, ensure_ascii=False, indent=2)
            f.write("\n")

    # ==================================================================
    # 1. MAP CREATION & EDITING (data/maps/*.txt + data/maps.json)
    # ==================================================================
    def list_tiles(self) -> List[Dict[str, Any]]:
        data = self._read_json("data/tiles.json")
        return list(data.get("tiles", []))

    def get_tile_map(self) -> Dict[int, Dict[str, Any]]:
        return {int(t["id"]): t for t in self.list_tiles()}

    def list_maps(self) -> List[Dict[str, Any]]:
        maps_json = self._read_json("data/maps.json")
        result = []
        for m in maps_json.get("maps", []):
            txt_path = self.root / m["txt_path"]
            w, h = 0, 0
            if txt_path.exists():
                grid, _ = self._parse_map_txt(txt_path)
                h = len(grid)
                w = len(grid[0]) if h > 0 else 0
            entry = dict(m)
            entry["width"] = w
            entry["height"] = h
            entry["txt_exists"] = txt_path.exists()
            result.append(entry)
        return result

    def _parse_map_txt(self, path: Path) -> Tuple[List[List[int]], List[str]]:
        comments: List[str] = []
        grid: List[List[int]] = []
        with open(path, "r", encoding="utf-8") as f:
            for raw in f:
                s = raw.strip()
                if not s:
                    continue
                if s.startswith("#"):
                    comments.append(s)
                    continue
                row = [int(tok) for tok in s.split()]
                if row:
                    grid.append(row)
        return grid, comments

    def load_map(self, map_id: str) -> Dict[str, Any]:
        maps_json = self._read_json("data/maps.json")
        meta = next((m for m in maps_json.get("maps", []) if m["id"] == map_id), None)
        if meta is None:
            raise ValueError(f"Map ID not found in data/maps.json: {map_id}")
        txt_path = self.root / meta["txt_path"]
        if not txt_path.exists():
            raise FileNotFoundError(f"Map txt file not found: {txt_path}")
        grid, comments = self._parse_map_txt(txt_path)
        h = len(grid)
        w = len(grid[0]) if h > 0 else 0
        return {
            "meta": meta,
            "width": w,
            "height": h,
            "grid": grid,
            "comments": comments,
        }

    def save_map_grid(
        self,
        map_id: str,
        grid: List[List[int]],
        comments: Optional[List[str]] = None,
    ) -> Dict[str, Any]:
        if not grid or not grid[0]:
            raise ValueError("Grid cannot be empty")
        w = len(grid[0])
        for row in grid:
            if len(row) != w:
                raise ValueError("All rows in tile grid must have the same width")
        valid_tiles = set(self.get_tile_map().keys())
        for y, row in enumerate(grid):
            for x, tid in enumerate(row):
                if tid not in valid_tiles:
                    raise ValueError(f"Invalid tile ID {tid} at ({x}, {y})")

        maps_json = self._read_json("data/maps.json")
        meta = next((m for m in maps_json.get("maps", []) if m["id"] == map_id), None)
        if meta is None:
            raise ValueError(f"Map ID not found in data/maps.json: {map_id}")

        txt_path = self.root / meta["txt_path"]
        txt_path.parent.mkdir(parents=True, exist_ok=True)
        h = len(grid)

        header_lines = comments if comments else [
            f"# Map: {map_id} ({meta.get('name', map_id)}) | Size: {w} x {h}",
            "# Generated/Edited by Designer Tool",
        ]
        # Keep size header up to date if standard header format is present
        if header_lines and header_lines[0].startswith(f"# Map: {map_id}"):
            header_lines = list(header_lines)
            header_lines[0] = f"# Map: {map_id} ({meta.get('name', map_id)}) | Size: {w} x {h}"

        with open(txt_path, "w", encoding="utf-8", newline="\n") as f:
            for c in header_lines:
                f.write(c.rstrip() + "\n")
            for row in grid:
                f.write(" ".join(f"{int(tid):02d}" for tid in row) + "\n")

        return {"map_id": map_id, "txt_path": meta["txt_path"], "width": w, "height": h}

    def create_map(
        self,
        map_id: str,
        name: str,
        map_type: str = "field",
        width: int = 24,
        height: int = 18,
        fill_tile: int = 0,
        border_tile: int = 6,
        safe_zone: Optional[bool] = None,
    ) -> Dict[str, Any]:
        if not re.match(r"^[a-z0-9_]+$", map_id):
            raise ValueError("map_id must contain only lowercase letters, digits, and underscores")
        if width < 4 or height < 4 or width > 128 or height > 128:
            raise ValueError("width and height must be between 4 and 128")
        if map_type not in ("town", "field", "boss"):
            raise ValueError("map_type must be one of: town, field, boss")

        valid_tiles = self.get_tile_map()
        if fill_tile not in valid_tiles or border_tile not in valid_tiles:
            raise ValueError("fill_tile and border_tile must be valid tile IDs from data/tiles.json")

        maps_json = self._read_json("data/maps.json")
        if any(m["id"] == map_id for m in maps_json.get("maps", [])):
            raise ValueError(f"Map ID already exists: {map_id}")

        if safe_zone is None:
            safe_zone = (map_type == "town")

        tint = [255, 250, 240] if map_type == "town" else ([245, 225, 225] if map_type == "boss" else [235, 240, 235])
        rel_txt = f"data/maps/{map_id}.txt"
        new_meta = {
            "id": map_id,
            "name": name,
            "type": map_type,
            "txt_path": rel_txt,
            "safe_zone": bool(safe_zone),
            "ambient_tint": tint,
            "warps": [],
            "monster_spawns": [],
            "boss_spawn": None,
        }
        maps_json.setdefault("maps", []).append(new_meta)
        self._write_json("data/maps.json", maps_json)

        grid = []
        for y in range(height):
            row = []
            for x in range(width):
                if x == 0 or y == 0 or x == width - 1 or y == height - 1:
                    row.append(border_tile)
                else:
                    row.append(fill_tile)
            grid.append(row)

        self.save_map_grid(map_id, grid)
        return {"meta": new_meta, "width": width, "height": height}

    def delete_map(self, map_id: str, remove_txt: bool = True) -> Dict[str, Any]:
        maps_json = self._read_json("data/maps.json")
        maps_list = maps_json.get("maps", [])
        target = next((m for m in maps_list if m["id"] == map_id), None)
        if target is None:
            raise ValueError(f"Map ID not found: {map_id}")
        maps_json["maps"] = [m for m in maps_list if m["id"] != map_id]
        self._write_json("data/maps.json", maps_json)
        txt_path = self.root / target["txt_path"]
        if remove_txt and txt_path.exists():
            txt_path.unlink()
        return {"deleted": map_id, "removed_txt": remove_txt}

    def set_map_tile(self, map_id: str, x: int, y: int, tile_id: int) -> Dict[str, Any]:
        m = self.load_map(map_id)
        grid = m["grid"]
        if not (0 <= y < m["height"] and 0 <= x < m["width"]):
            raise IndexError(f"Coordinate ({x}, {y}) out of bounds ({m['width']}x{m['height']})")
        old_tile = grid[y][x]
        grid[y][x] = tile_id
        self.save_map_grid(map_id, grid, m["comments"])
        return {"map_id": map_id, "x": x, "y": y, "old_tile": old_tile, "new_tile": tile_id}

    def fill_map_rect(
        self, map_id: str, x0: int, y0: int, x1: int, y1: int, tile_id: int
    ) -> Dict[str, Any]:
        m = self.load_map(map_id)
        grid = m["grid"]
        xa, xb = min(x0, x1), max(x0, x1)
        ya, yb = min(y0, y1), max(y0, y1)
        if xa < 0 or ya < 0 or xb >= m["width"] or yb >= m["height"]:
            raise IndexError(f"Rect ({xa},{ya})-({xb},{yb}) out of bounds ({m['width']}x{m['height']})")
        count = 0
        for y in range(ya, yb + 1):
            for x in range(xa, xb + 1):
                grid[y][x] = tile_id
                count += 1
        self.save_map_grid(map_id, grid, m["comments"])
        return {"map_id": map_id, "filled_count": count, "tile_id": tile_id}

    @staticmethod
    def flood_fill_grid(grid: List[List[int]], start_x: int, start_y: int, new_tile: int) -> int:
        h = len(grid)
        w = len(grid[0]) if h else 0
        if not (0 <= start_y < h and 0 <= start_x < w):
            return 0
        target = grid[start_y][start_x]
        if target == new_tile:
            return 0
        q = deque([(start_x, start_y)])
        grid[start_y][start_x] = new_tile
        changed = 1
        while q:
            cx, cy = q.popleft()
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = cx + dx, cy + dy
                if 0 <= nx < w and 0 <= ny < h and grid[ny][nx] == target:
                    grid[ny][nx] = new_tile
                    q.append((nx, ny))
                    changed += 1
        return changed

    def resize_map(
        self, map_id: str, new_width: int, new_height: int, fill_tile: int = 0
    ) -> Dict[str, Any]:
        if new_width < 4 or new_height < 4 or new_width > 128 or new_height > 128:
            raise ValueError("new_width and new_height must be between 4 and 128")
        m = self.load_map(map_id)
        old_grid = m["grid"]
        old_h, old_w = m["height"], m["width"]
        new_grid = [[fill_tile for _ in range(new_width)] for _ in range(new_height)]
        for y in range(min(old_h, new_height)):
            for x in range(min(old_w, new_width)):
                new_grid[y][x] = old_grid[y][x]
        return self.save_map_grid(map_id, new_grid, m["comments"])

    def validate_maps(self) -> Dict[str, Any]:
        tile_map = self.get_tile_map()
        issues: List[str] = []
        maps_data = self.list_maps()
        map_ids = {m["id"] for m in maps_data}

        for m in maps_data:
            mid = m["id"]
            if not m["txt_exists"]:
                issues.append(f"[{mid}] Missing map txt file: {m['txt_path']}")
                continue
            loaded = self.load_map(mid)
            grid = loaded["grid"]
            w, h = loaded["width"], loaded["height"]
            for y, row in enumerate(grid):
                if len(row) != w:
                    issues.append(f"[{mid}] Row {y} length mismatch ({len(row)} != {w})")
                for x, tid in enumerate(row):
                    if tid not in tile_map:
                        issues.append(f"[{mid}] Unknown tile ID {tid} at ({x}, {y})")

            for warp in m.get("warps", []):
                wx, wy = warp["tile"]
                if not (0 <= wx < w and 0 <= wy < h):
                    issues.append(f"[{mid}] Warp out of bounds: ({wx}, {wy})")
                if warp["target_map"] not in map_ids:
                    issues.append(f"[{mid}] Warp targets unknown map: {warp['target_map']}")

            for sp in m.get("monster_spawns", []):
                sx, sy = sp["tile"]
                if not (0 <= sx < w and 0 <= sy < h):
                    issues.append(f"[{mid}] Monster spawn out of bounds: ({sx}, {sy})")
                elif not tile_map.get(grid[sy][sx], {}).get("walkable", False):
                    issues.append(f"[{mid}] Monster spawn ({sx}, {sy}) is on non-walkable tile {grid[sy][sx]}")

            bs = m.get("boss_spawn")
            if bs:
                bx, by = bs["tile"]
                if not (0 <= bx < w and 0 <= by < h):
                    issues.append(f"[{mid}] Boss spawn out of bounds: ({bx}, {by})")
                elif not tile_map.get(grid[by][bx], {}).get("walkable", False):
                    issues.append(f"[{mid}] Boss spawn ({bx}, {by}) is on non-walkable tile {grid[by][bx]}")

        return {"ok": len(issues) == 0, "checked_maps": len(maps_data), "issues": issues}

    # ==================================================================
    # 2. ART RESOURCE UPLOAD & MAPPING MANAGEMENT
    # ==================================================================
    def list_art_mappings(self) -> List[Dict[str, Any]]:
        entries: List[Dict[str, Any]] = []
        if not self.art_mapping_path.exists():
            return entries
        with open(self.art_mapping_path, "r", encoding="utf-8") as f:
            for line in f:
                s = line.strip()
                if not s or s.startswith("#"):
                    continue
                parts = s.split(maxsplit=1)
                if len(parts) < 2:
                    continue
                art_id = int(parts[0])
                rel_path = parts[1].strip()
                abs_path = self.root / rel_path
                exists = abs_path.exists()
                size_str = ""
                if exists:
                    try:
                        with Image.open(abs_path) as im:
                            size_str = f"{im.width}x{im.height}"
                    except Exception:
                        size_str = "invalid"
                entries.append({
                    "art_id": art_id,
                    "path": rel_path,
                    "category": self._guess_art_category(art_id),
                    "exists": exists,
                    "size": size_str,
                })
        entries.sort(key=lambda e: e["art_id"])
        return entries

    @staticmethod
    def _guess_art_category(art_id: int) -> str:
        for cat, (lo, hi, _, _) in ART_CATEGORIES.items():
            if lo <= art_id <= hi:
                return cat
        return "custom"

    def suggest_next_art_id(self, category: str) -> int:
        if category not in ART_CATEGORIES:
            raise ValueError(f"Unknown art category: {category}. Choose from {list(ART_CATEGORIES.keys())}")
        lo, hi, _, _ = ART_CATEGORIES[category]
        used = {e["art_id"] for e in self.list_art_mappings()}
        for candidate in range(lo, hi + 1):
            if candidate not in used:
                return candidate
        raise ValueError(f"No free Art IDs left in category '{category}' ({lo}..{hi})")

    def _save_art_mappings(self, entries: List[Dict[str, Any]]) -> None:
        sorted_entries = sorted(entries, key=lambda e: int(e["art_id"]))
        section_titles = {
            "tiles":      "# --- Tiles (1..99) ---",
            "characters": "# --- Player 8-Direction Sprite Sheets (101..199) ---",
            "monsters":   "# --- Field Monsters (201..299) ---",
            "boss":       "# --- Boss Monster (301..399) ---",
            "npcs":       "# --- NPCs (401..499) ---",
            "items":      "# --- Item Icons (501..599) ---",
            "effects":    "# --- Effects & UI Icons (601..699) ---",
            "custom":     "# --- Custom Art Resources ---",
        }
        grouped: Dict[str, List[Dict[str, Any]]] = {}
        for e in sorted_entries:
            cat = self._guess_art_category(int(e["art_id"]))
            grouped.setdefault(cat, []).append(e)

        lines = [
            "# Art Resource ID -> Relative File Path Mapping",
            "# Format: <ArtID> <RelativePath>",
            "# Game logic (ResourceManager) reads ONLY this file to load all textures.",
            "",
        ]
        order = ["tiles", "characters", "monsters", "boss", "npcs", "items", "effects", "custom"]
        for cat in order:
            if cat not in grouped:
                continue
            lines.append(section_titles[cat])
            for e in grouped[cat]:
                lines.append(f"{int(e['art_id'])} {e['path']}")
            lines.append("")

        with open(self.art_mapping_path, "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(lines).rstrip() + "\n")

    def _append_art_doc_entry(self, art_id: int, rel_path: str, size_str: str, concept: str) -> None:
        if not self.art_doc_path.exists():
            return
        with open(self.art_doc_path, "r", encoding="utf-8") as f:
            content = f.read()
        marker = f"| `{art_id}` |"
        if marker in content:
            return
        section_header = "\n### 3.8 툴로 추가 업로드된 아트 리소스\n| Art ID | 경로 | 규격 | 아트 컨셉 |\n| :---: | :--- | :---: | :--- |\n"
        row = f"| `{art_id}` | `{rel_path}` | {size_str} | {concept or '기획자 툴을 통해 업로드된 에셋'} |\n"
        if "### 3.8 툴로 추가 업로드된 아트 리소스" not in content:
            content = content.rstrip() + "\n" + section_header + row
        else:
            content = content.rstrip() + "\n" + row
        with open(self.art_doc_path, "w", encoding="utf-8", newline="\n") as f:
            f.write(content)

    def upload_art_resource(
        self,
        source_file: str | Path,
        category: str,
        art_id: Optional[int] = None,
        target_name: Optional[str] = None,
        resize: Optional[str] = None,
        concept: str = "",
        fulfill_request_id: Optional[str] = None,
        overwrite: bool = False,
    ) -> Dict[str, Any]:
        src = Path(source_file).resolve()
        if not src.exists():
            raise FileNotFoundError(f"Source image not found: {src}")
        if category not in ART_CATEGORIES:
            raise ValueError(f"Unknown category '{category}'. Valid: {list(ART_CATEGORIES.keys())}")

        if art_id is None:
            art_id = self.suggest_next_art_id(category)

        mappings = self.list_art_mappings()
        existing = next((e for e in mappings if e["art_id"] == art_id), None)
        if existing and not overwrite:
            raise ValueError(f"Art ID {art_id} already mapped to '{existing['path']}'. Pass overwrite=True to replace.")

        _, _, subdir, _ = ART_CATEGORIES[category]
        stem = target_name if target_name else src.stem
        stem = re.sub(r"[^a-zA-Z0-9_\-]", "_", stem)
        if not stem.lower().endswith(".png"):
            stem += ".png"
        rel_path = f"{subdir}/{stem}".replace("\\", "/")
        dst = self.root / rel_path
        dst.parent.mkdir(parents=True, exist_ok=True)

        with Image.open(src) as im:
            rgba = im.convert("RGBA")
            if resize:
                m = re.match(r"^(\d+)x(\d+)$", resize.lower().strip())
                if not m:
                    raise ValueError("resize must be formatted as WxH, e.g. 32x32 or 160x40")
                rgba = rgba.resize((int(m.group(1)), int(m.group(2))), Image.Resampling.NEAREST)
            rgba.save(dst, "PNG")
            final_size = f"{rgba.width}x{rgba.height}"

        if existing:
            existing["path"] = rel_path
        else:
            mappings.append({"art_id": art_id, "path": rel_path})
        self._save_art_mappings(mappings)
        self._append_art_doc_entry(art_id, rel_path, final_size, concept)

        fulfilled = None
        if fulfill_request_id:
            fulfilled = self.fulfill_art_request(fulfill_request_id, art_id, rel_path)

        return {
            "art_id": art_id,
            "category": category,
            "path": rel_path,
            "size": final_size,
            "fulfilled_request": fulfilled["request_id"] if fulfilled else None,
        }

    def remove_art_mapping(self, art_id: int, delete_file: bool = False) -> Dict[str, Any]:
        mappings = self.list_art_mappings()
        target = next((e for e in mappings if e["art_id"] == art_id), None)
        if not target:
            raise ValueError(f"Art ID {art_id} not found in art_mapping.txt")
        mappings = [e for e in mappings if e["art_id"] != art_id]
        self._save_art_mappings(mappings)
        if delete_file:
            p = self.root / target["path"]
            if p.exists():
                p.unlink()
        return {"removed_art_id": art_id, "path": target["path"], "deleted_file": delete_file}

    def verify_art_resources(self) -> Dict[str, Any]:
        entries = self.list_art_mappings()
        missing = [e for e in entries if not e["exists"]]
        return {
            "ok": len(missing) == 0,
            "total": len(entries),
            "missing_count": len(missing),
            "missing": missing,
        }

    # ==================================================================
    # 3. ART RESOURCE REQUIREMENTS QUEUE (data/art_requests.json)
    # ==================================================================
    def _load_art_requests_file(self) -> Dict[str, Any]:
        if not self.art_requests_path.exists():
            data = {"version": 1, "next_seq": 1, "requests": []}
            self._write_json(self.art_requests_path, data)
            return data
        return self._read_json(self.art_requests_path)

    def list_art_requests(self, status: str = "all") -> List[Dict[str, Any]]:
        data = self._load_art_requests_file()
        reqs = data.get("requests", [])
        if status and status != "all":
            reqs = [r for r in reqs if r.get("status") == status]
        return reqs

    def add_art_request(
        self,
        title: str,
        prompt: str,
        category: str = "items",
        size: Optional[str] = None,
        suggested_art_id: Optional[int] = None,
        target_path: str = "",
        notes: str = "",
    ) -> Dict[str, Any]:
        if not title.strip():
            raise ValueError("title is required")
        if not prompt.strip():
            raise ValueError("prompt (short description of required art) is required")
        if category not in ART_CATEGORIES:
            raise ValueError(f"Unknown category '{category}'. Valid: {list(ART_CATEGORIES.keys())}")

        data = self._load_art_requests_file()
        seq = int(data.get("next_seq", 1))
        req_id = f"REQ-{seq:03d}"

        if suggested_art_id is None:
            # Avoid colliding with existing mappings or already-pending suggested IDs
            used = {e["art_id"] for e in self.list_art_mappings()}
            used |= {
                int(r["suggested_art_id"])
                for r in data.get("requests", [])
                if r.get("status") == "pending" and r.get("suggested_art_id") is not None
            }
            lo, hi, subdir, default_sz = ART_CATEGORIES[category]
            for c in range(lo, hi + 1):
                if c not in used:
                    suggested_art_id = c
                    break
        else:
            _, _, subdir, default_sz = ART_CATEGORIES[category]

        if not size:
            size = default_sz
        if not target_path:
            slug = re.sub(r"[^a-zA-Z0-9_\-]", "_", title.lower()).strip("_") or f"asset_{suggested_art_id}"
            target_path = f"{subdir}/{slug}.png"

        ts = now_iso()
        record = {
            "request_id": req_id,
            "title": title.strip(),
            "category": category,
            "size": size.strip(),
            "suggested_art_id": suggested_art_id,
            "target_path": target_path.replace("\\", "/"),
            "prompt": prompt.strip(),
            "notes": notes.strip(),
            "status": "pending",
            "linked_art_id": None,
            "created_at": ts,
            "updated_at": ts,
        }
        data.setdefault("requests", []).append(record)
        data["next_seq"] = seq + 1
        self._write_json(self.art_requests_path, data)
        return record

    def update_art_request(self, request_id: str, updates: Dict[str, Any]) -> Dict[str, Any]:
        data = self._load_art_requests_file()
        rec = next((r for r in data.get("requests", []) if r["request_id"] == request_id), None)
        if not rec:
            raise ValueError(f"Art request not found: {request_id}")
        allowed = {"title", "category", "size", "suggested_art_id", "target_path", "prompt", "notes", "status", "linked_art_id"}
        for k, v in updates.items():
            if k in allowed:
                rec[k] = v
        if rec["status"] not in ("pending", "fulfilled", "cancelled"):
            raise ValueError("status must be pending, fulfilled, or cancelled")
        rec["updated_at"] = now_iso()
        self._write_json(self.art_requests_path, data)
        return rec

    def fulfill_art_request(
        self, request_id: str, art_id: int, final_path: Optional[str] = None
    ) -> Dict[str, Any]:
        updates: Dict[str, Any] = {"status": "fulfilled", "linked_art_id": int(art_id)}
        if final_path:
            updates["target_path"] = final_path.replace("\\", "/")
        return self.update_art_request(request_id, updates)

    def delete_art_request(self, request_id: str) -> Dict[str, Any]:
        data = self._load_art_requests_file()
        before = len(data.get("requests", []))
        data["requests"] = [r for r in data.get("requests", []) if r["request_id"] != request_id]
        if len(data["requests"]) == before:
            raise ValueError(f"Art request not found: {request_id}")
        self._write_json(self.art_requests_path, data)
        return {"deleted": request_id}

    # ==================================================================
    # 4. DESIGN DOCUMENT (JSON) VIEWER, SEARCH & EDITOR
    # ==================================================================
    def list_design_docs(self) -> List[Dict[str, Any]]:
        out = []
        for key, spec in DESIGN_DOCS.items():
            raw = self._read_json(spec["file"])
            mode = spec["mode"]
            if mode == "list":
                count = len(raw.get(spec["collection"], []))
            elif mode == "kv":
                count = len(raw.get(spec["collection"], {}))
            else:
                count = len(raw)
            out.append({
                "doc": key,
                "label": spec["label"],
                "file": spec["file"],
                "mode": mode,
                "count": count,
            })
        return out

    def load_design_doc(self, doc_key: str) -> Dict[str, Any]:
        if doc_key not in DESIGN_DOCS:
            raise ValueError(f"Unknown design doc '{doc_key}'. Valid: {list(DESIGN_DOCS.keys())}")
        spec = DESIGN_DOCS[doc_key]
        raw = self._read_json(spec["file"])
        return {"doc": doc_key, "spec": spec, "raw": raw}

    def save_design_doc(self, doc_key: str, raw: Any) -> Dict[str, Any]:
        if doc_key not in DESIGN_DOCS:
            raise ValueError(f"Unknown design doc '{doc_key}'")
        spec = DESIGN_DOCS[doc_key]
        self._write_json(spec["file"], raw)
        return {"saved": doc_key, "file": spec["file"]}

    def get_doc_records(self, doc_key: str) -> List[Tuple[str, str, Any]]:
        """Returns list of (record_id_str, display_label, record_value)."""
        loaded = self.load_design_doc(doc_key)
        spec = loaded["spec"]
        raw = loaded["raw"]
        mode = spec["mode"]
        records: List[Tuple[str, str, Any]] = []

        if mode == "list":
            id_f = spec["id_field"]
            name_f = spec["name_field"]
            for item in raw.get(spec["collection"], []):
                rid = str(item.get(id_f, ""))
                rname = str(item.get(name_f, "")) if name_f else ""
                label = f"{rid}  —  {rname}" if rname and rname != rid else rid
                records.append((rid, label, item))
        elif mode == "kv":
            for k, v in raw.get(spec["collection"], {}).items():
                preview = str(v).replace("\n", " ")[:36]
                records.append((str(k), f"{k}  =  {preview}", {"key": k, "value": v}))
        else:  # object (player.json)
            records.append(("player", str(raw.get("name", "player")), raw))
        return records

    def get_doc_record(self, doc_key: str, record_id: Optional[str] = None) -> Any:
        records = self.get_doc_records(doc_key)
        if record_id is None:
            return [r[2] for r in records]
        for rid, _, val in records:
            if rid == str(record_id):
                return val
        raise ValueError(f"Record '{record_id}' not found in '{doc_key}'")

    def search_design_docs(self, query: str, doc_key: Optional[str] = None) -> List[Dict[str, Any]]:
        q = query.strip().lower()
        targets = [doc_key] if doc_key else list(DESIGN_DOCS.keys())
        hits: List[Dict[str, Any]] = []

        for dk in targets:
            if dk not in DESIGN_DOCS:
                raise ValueError(f"Unknown design doc '{dk}'")
            for rid, label, val in self.get_doc_records(dk):
                serialized = json.dumps(val, ensure_ascii=False).lower()
                if not q or q in rid.lower() or q in label.lower() or q in serialized:
                    matched_fields = []
                    if isinstance(val, dict):
                        for fk, fv in val.items():
                            if q and (q in str(fk).lower() or q in json.dumps(fv, ensure_ascii=False).lower()):
                                matched_fields.append(fk)
                    hits.append({
                        "doc": dk,
                        "record_id": rid,
                        "label": label,
                        "matched_fields": matched_fields,
                        "record": val,
                    })
        return hits

    @staticmethod
    def _coerce_value(existing_val: Any, new_val_str: Any) -> Any:
        if not isinstance(new_val_str, str):
            return new_val_str
        s = new_val_str.strip()
        # Try JSON parsing for lists/dicts/booleans/null/numbers
        if isinstance(existing_val, bool):
            if s.lower() in ("true", "1", "yes"):
                return True
            if s.lower() in ("false", "0", "no"):
                return False
            raise ValueError(f"Invalid boolean value: {new_val_str}")
        if isinstance(existing_val, int) and not isinstance(existing_val, bool):
            return int(s)
        if isinstance(existing_val, float):
            return float(s)
        if isinstance(existing_val, (list, dict)) or existing_val is None:
            return json.loads(s)
        # If target type is unknown/string, check if JSON literal was explicitly supplied only when existing is None
        return new_val_str

    def update_doc_record(
        self,
        doc_key: str,
        record_id: str,
        updates: Dict[str, Any],
        coerce: bool = True,
    ) -> Dict[str, Any]:
        loaded = self.load_design_doc(doc_key)
        spec = loaded["spec"]
        raw = loaded["raw"]
        mode = spec["mode"]

        if mode == "list":
            id_f = spec["id_field"]
            coll = raw.get(spec["collection"], [])
            target = next((it for it in coll if str(it.get(id_f)) == str(record_id)), None)
            if target is None:
                raise ValueError(f"Record '{record_id}' not found in '{doc_key}'")
            for k, v in updates.items():
                if coerce and k in target:
                    target[k] = self._coerce_value(target[k], v)
                else:
                    target[k] = v
            self.save_design_doc(doc_key, raw)
            return {"doc": doc_key, "record_id": str(target.get(id_f)), "record": target}

        if mode == "kv":
            coll = raw.get(spec["collection"], {})
            if record_id not in coll and "key" not in updates:
                raise ValueError(f"String key '{record_id}' not found in '{doc_key}'")
            new_key = str(updates.get("key", record_id))
            new_val = str(updates.get("value", coll.get(record_id, "")))
            if new_key != record_id and record_id in coll:
                del coll[record_id]
            coll[new_key] = new_val
            self.save_design_doc(doc_key, raw)
            return {"doc": doc_key, "record_id": new_key, "record": {"key": new_key, "value": new_val}}

        # mode == "object" (player.json)
        for k, v in updates.items():
            if coerce and k in raw:
                raw[k] = self._coerce_value(raw[k], v)
            else:
                raw[k] = v
        self.save_design_doc(doc_key, raw)
        return {"doc": doc_key, "record_id": "player", "record": raw}

    def add_doc_record(self, doc_key: str, record: Dict[str, Any]) -> Dict[str, Any]:
        loaded = self.load_design_doc(doc_key)
        spec = loaded["spec"]
        raw = loaded["raw"]
        mode = spec["mode"]

        if mode == "list":
            id_f = spec["id_field"]
            if id_f not in record:
                raise ValueError(f"New record for '{doc_key}' must include '{id_f}'")
            coll = raw.setdefault(spec["collection"], [])
            new_id = str(record[id_f])
            if any(str(it.get(id_f)) == new_id for it in coll):
                raise ValueError(f"Duplicate ID '{new_id}' in '{doc_key}'")
            coll.append(record)
            self.save_design_doc(doc_key, raw)
            return {"doc": doc_key, "record_id": new_id, "record": record}

        if mode == "kv":
            if "key" not in record or "value" not in record:
                raise ValueError("New string entry must include 'key' and 'value'")
            coll = raw.setdefault(spec["collection"], {})
            k = str(record["key"])
            if k in coll:
                raise ValueError(f"Duplicate string key '{k}'")
            coll[k] = str(record["value"])
            self.save_design_doc(doc_key, raw)
            return {"doc": doc_key, "record_id": k, "record": {"key": k, "value": coll[k]}}

        raise ValueError(f"Cannot add multiple records to single-object doc '{doc_key}'")

    def delete_doc_record(self, doc_key: str, record_id: str) -> Dict[str, Any]:
        loaded = self.load_design_doc(doc_key)
        spec = loaded["spec"]
        raw = loaded["raw"]
        mode = spec["mode"]

        if mode == "list":
            id_f = spec["id_field"]
            coll = raw.get(spec["collection"], [])
            before = len(coll)
            raw[spec["collection"]] = [it for it in coll if str(it.get(id_f)) != str(record_id)]
            if len(raw[spec["collection"]]) == before:
                raise ValueError(f"Record '{record_id}' not found in '{doc_key}'")
            self.save_design_doc(doc_key, raw)
            return {"doc": doc_key, "deleted": record_id}

        if mode == "kv":
            coll = raw.get(spec["collection"], {})
            if record_id not in coll:
                raise ValueError(f"String key '{record_id}' not found in '{doc_key}'")
            del coll[record_id]
            self.save_design_doc(doc_key, raw)
            return {"doc": doc_key, "deleted": record_id}

        raise ValueError(f"Cannot delete root object of '{doc_key}'")

    def make_record_template(self, doc_key: str) -> Dict[str, Any]:
        """Creates a sensible default template cloned from existing records for UI 'Add New'."""
        records = self.get_doc_records(doc_key)
        if doc_key == "strings":
            return {"key": "custom.new_key", "value": "새 텍스트"}
        if doc_key == "tiles":
            used = [int(r[2]["id"]) for r in records]
            next_id = max(used) + 1 if used else 0
            return {
                "id": next_id,
                "key": f"tile_{next_id}",
                "name": f"새 타일 {next_id}",
                "art_id": 1,
                "walkable": True,
                "special": "none",
                "concept": "타일 아트 컨셉 설명",
            }
        if records and isinstance(records[0][2], dict):
            tpl = copy.deepcopy(records[0][2])
            if "id" in tpl:
                tpl["id"] = f"new_{doc_key}_{len(records) + 1}"
            if "name" in tpl:
                tpl["name"] = f"신규 {doc_key}"
            if "title" in tpl:
                tpl["title"] = f"신규 퀘스트"
            return tpl
        return {"id": f"new_{doc_key}"}

    def validate_all_docs(self) -> Dict[str, Any]:
        issues: List[str] = []
        art_ids = {e["art_id"] for e in self.list_art_mappings() if e["exists"]}

        items = {it["id"]: it for it in self._read_json("data/items.json").get("items", [])}
        monsters = {m["id"]: m for m in self._read_json("data/monsters.json").get("monsters", [])}
        bosses = {b["id"]: b for b in self._read_json("data/bosses.json").get("bosses", [])}
        npcs = {n["id"]: n for n in self._read_json("data/npcs.json").get("npcs", [])}
        maps = {m["id"]: m for m in self._read_json("data/maps.json").get("maps", [])}
        quests = self._read_json("data/quests.json").get("quests", [])
        tiles = self._read_json("data/tiles.json").get("tiles", [])

        for t in tiles:
            if t.get("art_id") not in art_ids:
                issues.append(f"[tiles.json] Tile {t.get('id')} references missing art_id {t.get('art_id')}")
        for it in items.values():
            if it.get("art_id") not in art_ids:
                issues.append(f"[items.json] Item '{it['id']}' references missing art_id {it.get('art_id')}")
        for m in monsters.values():
            if m.get("art_id") not in art_ids:
                issues.append(f"[monsters.json] Monster '{m['id']}' references missing art_id {m.get('art_id')}")
        for b in bosses.values():
            if b.get("art_id") not in art_ids:
                issues.append(f"[bosses.json] Boss '{b['id']}' references missing art_id {b.get('art_id')}")
            if b.get("drop_item_id") and b["drop_item_id"] not in items:
                issues.append(f"[bosses.json] Boss '{b['id']}' drops unknown item '{b['drop_item_id']}'")
        for n in npcs.values():
            if n.get("art_id") not in art_ids:
                issues.append(f"[npcs.json] NPC '{n['id']}' references missing art_id {n.get('art_id')}")
            if n.get("map_id") not in maps:
                issues.append(f"[npcs.json] NPC '{n['id']}' references unknown map '{n.get('map_id')}'")
            for sid in n.get("shop_items", []):
                if sid not in items:
                    issues.append(f"[npcs.json] NPC '{n['id']}' sells unknown item '{sid}'")
        for q in quests:
            if q.get("giver_npc_id") not in npcs:
                issues.append(f"[quests.json] Quest '{q['id']}' unknown giver_npc_id '{q.get('giver_npc_id')}'")
            for obj in q.get("objectives", []):
                tid = obj.get("target_id")
                if tid not in monsters and tid not in bosses:
                    issues.append(f"[quests.json] Quest '{q['id']}' unknown target_id '{tid}'")
            if q.get("reward_item_id") and q["reward_item_id"] not in items:
                issues.append(f"[quests.json] Quest '{q['id']}' unknown reward_item_id '{q['reward_item_id']}'")

        map_check = self.validate_maps()
        issues.extend(map_check["issues"])

        art_check = self.verify_art_resources()
        for miss in art_check["missing"]:
            issues.append(f"[art_mapping.txt] Missing file for Art ID {miss['art_id']}: {miss['path']}")

        ui_check = self.validate_ui_layout()
        issues.extend(ui_check["issues"])

        return {"ok": len(issues) == 0, "issue_count": len(issues), "issues": issues}

    # ==================================================================
    # 5 & 6. LOBBY SCREEN & IN-PLAY UI (HUD) LAYOUT EDITOR (data/ui_layout.json)
    # ==================================================================
    @staticmethod
    def default_ui_layout() -> Dict[str, Any]:
        return {
            "reference_width": 1280,
            "reference_height": 720,
            "lobby": {
                "bg_top_color": [18, 22, 34, 255],
                "bg_bottom_color": [38, 20, 26, 255],
                "elements": [
                    {"id": "hero_art", "label": "대표 캐릭터/보스 아트", "type": "sprite", "visible": True, "x": 560, "y": 95, "width": 160, "height": 160, "art_id": 301, "frame_w": 80, "frame_h": 80, "color": [255, 255, 255, 255]},
                    {"id": "title_text", "label": "메인 타이틀 텍스트", "type": "text", "visible": True, "x": 240, "y": 270, "width": 800, "height": 52, "text_key": "title.main", "text": "", "font_size": 44, "align": "center", "color": [255, 203, 0, 255]},
                    {"id": "subtitle_text", "label": "서브 타이틀 텍스트", "type": "text", "visible": True, "x": 240, "y": 326, "width": 800, "height": 28, "text_key": "title.subtitle", "text": "", "font_size": 18, "align": "center", "color": [210, 205, 195, 255]},
                    {"id": "guide_card", "label": "조작 안내 카드", "type": "guide_card", "visible": True, "x": 315, "y": 375, "width": 650, "height": 175, "font_size": 16, "bg_color": [15, 18, 28, 210], "border_color": [160, 140, 95, 200], "color": [245, 245, 245, 255], "text_keys": ["title.guide_1", "title.guide_2", "title.guide_3", "title.guide_4", "title.guide_5"]},
                    {"id": "start_button", "label": "새 게임 시작 버튼", "type": "button", "action": "start_game", "visible": True, "x": 370, "y": 585, "width": 250, "height": 52, "text_key": "title.start_btn", "text": "", "font_size": 16, "bg_color": [140, 48, 42, 255], "hover_color": [185, 65, 55, 255], "border_color": [200, 185, 145, 220], "color": [255, 255, 255, 255]},
                    {"id": "continue_button", "label": "이어하기 버튼", "type": "button", "action": "continue_game", "visible": True, "x": 660, "y": 585, "width": 250, "height": 52, "text_key": "title.continue_btn", "text": "", "font_size": 16, "bg_color": [46, 98, 138, 255], "hover_color": [64, 132, 182, 255], "border_color": [200, 185, 145, 220], "color": [255, 255, 255, 255]},
                ],
            },
            "hud": {
                "widgets": [
                    {"id": "player_status", "label": "플레이어 상태창 (HP/EXP/골드/방향)", "group": "hud", "visible": True, "x": 14, "y": 14, "width": 285, "height": 134, "bg_color": [14, 16, 24, 215], "border_color": [130, 118, 92, 220], "accent_color": [255, 203, 0, 255]},
                    {"id": "map_banner", "label": "현재 지역 배너", "group": "hud", "visible": True, "x": 510, "y": 14, "width": 260, "height": 32, "bg_color": [14, 16, 24, 215], "border_color": [130, 118, 92, 220], "accent_color": [255, 255, 255, 255]},
                    {"id": "boss_bar", "label": "보스 체력바", "group": "hud", "visible": True, "x": 360, "y": 52, "width": 560, "height": 48, "bg_color": [18, 10, 12, 225], "border_color": [230, 41, 55, 255], "accent_color": [205, 35, 45, 255]},
                    {"id": "quest_tracker", "label": "현재 퀘스트 트래커", "group": "hud", "visible": True, "x": 1000, "y": 14, "width": 266, "height": 135, "bg_color": [14, 16, 24, 205], "border_color": [130, 118, 92, 200], "accent_color": [255, 203, 0, 255]},
                    {"id": "minimap", "label": "미니맵", "group": "hud", "visible": True, "x": 1100, "y": 158, "width": 166, "height": 126, "bg_color": [10, 12, 18, 210], "border_color": [130, 118, 92, 200], "accent_color": [255, 255, 255, 255], "cell_size": 5},
                    {"id": "action_bar", "label": "하단 스킬/퀵슬롯 액션바", "group": "hud", "visible": True, "x": 325, "y": 656, "width": 630, "height": 52, "bg_color": [14, 16, 24, 220], "border_color": [130, 118, 92, 220], "accent_color": [255, 203, 0, 255]},
                    {"id": "toast_area", "label": "알림(Toast) 표시 영역", "group": "hud", "visible": True, "x": 390, "y": 115, "width": 500, "height": 32, "bg_color": [12, 14, 22, 205], "border_color": [130, 118, 92, 180], "accent_color": [255, 203, 0, 255]},
                    {"id": "dialogue_panel", "label": "NPC 대화창", "group": "popup", "visible": True, "x": 290, "y": 480, "width": 700, "height": 165, "bg_color": [14, 16, 26, 240], "border_color": [255, 203, 0, 255], "accent_color": [255, 203, 0, 255]},
                    {"id": "shop_panel", "label": "상점 팝업창", "group": "popup", "visible": True, "x": 310, "y": 110, "width": 660, "height": 500, "bg_color": [14, 16, 26, 244], "border_color": [255, 203, 0, 255], "accent_color": [255, 203, 0, 255]},
                    {"id": "inventory_panel", "label": "인벤토리/장비 팝업창", "group": "popup", "visible": True, "x": 340, "y": 130, "width": 600, "height": 460, "bg_color": [14, 16, 26, 244], "border_color": [102, 191, 255, 255], "accent_color": [102, 191, 255, 255]},
                    {"id": "quest_log_panel", "label": "퀘스트 일지 팝업창", "group": "popup", "visible": True, "x": 330, "y": 140, "width": 620, "height": 440, "bg_color": [14, 16, 26, 244], "border_color": [255, 203, 0, 255], "accent_color": [255, 203, 0, 255]},
                ],
            },
        }

    @staticmethod
    def parse_rgba(val: Any) -> List[int]:
        if isinstance(val, list) and len(val) in (3, 4):
            nums = [max(0, min(255, int(c))) for c in val]
            return nums if len(nums) == 4 else nums + [255]
        if isinstance(val, str):
            s = val.strip()
            if s.startswith("#") and len(s) in (7, 9):
                r = int(s[1:3], 16)
                g = int(s[3:5], 16)
                b = int(s[5:7], 16)
                a = int(s[7:9], 16) if len(s) == 9 else 255
                return [r, g, b, a]
            parts = [int(p.strip()) for p in s.split(",") if p.strip()]
            if len(parts) in (3, 4):
                nums = [max(0, min(255, c)) for c in parts]
                return nums if len(nums) == 4 else nums + [255]
        raise ValueError(f"Invalid RGBA color: {val} (use [R,G,B,A], 'R,G,B,A', or '#RRGGBB')")

    def load_ui_layout(self) -> Dict[str, Any]:
        if not self.ui_layout_path.exists():
            data = self.default_ui_layout()
            self._write_json(self.ui_layout_path, data)
            return data
        return self._read_json(self.ui_layout_path)

    def save_ui_layout(self, data: Dict[str, Any]) -> Dict[str, Any]:
        self._write_json(self.ui_layout_path, data)
        return {"saved": "data/ui_layout.json"}

    def get_string_map(self) -> Dict[str, str]:
        try:
            return dict(self._read_json("data/strings.json").get("strings", {}))
        except Exception:
            return {}

    def get_lobby_layout(self) -> Dict[str, Any]:
        layout = self.load_ui_layout()
        return {
            "reference_width": layout.get("reference_width", 1280),
            "reference_height": layout.get("reference_height", 720),
            "lobby": layout.get("lobby", {}),
        }

    def update_lobby_bg(self, bg_top: Any = None, bg_bottom: Any = None) -> Dict[str, Any]:
        layout = self.load_ui_layout()
        lob = layout.setdefault("lobby", {})
        if bg_top is not None:
            lob["bg_top_color"] = self.parse_rgba(bg_top)
        if bg_bottom is not None:
            lob["bg_bottom_color"] = self.parse_rgba(bg_bottom)
        self.save_ui_layout(layout)
        return {"bg_top_color": lob.get("bg_top_color"), "bg_bottom_color": lob.get("bg_bottom_color")}

    def update_lobby_element(self, elem_id: str, updates: Dict[str, Any]) -> Dict[str, Any]:
        layout = self.load_ui_layout()
        elems = layout.setdefault("lobby", {}).setdefault("elements", [])
        target = next((e for e in elems if e.get("id") == elem_id), None)
        if target is None:
            raise ValueError(f"Lobby element not found: {elem_id}")

        color_fields = {"color", "bg_color", "hover_color", "border_color"}
        int_fields = {"x", "y", "width", "height", "art_id", "frame_w", "frame_h", "font_size"}
        for k, v in updates.items():
            if v is None:
                continue
            if k in color_fields:
                target[k] = self.parse_rgba(v)
            elif k in int_fields:
                target[k] = int(round(float(v)))
            elif k == "visible":
                target[k] = bool(v) if not isinstance(v, str) else v.lower() in ("true", "1", "yes")
            else:
                target[k] = v

        self.save_ui_layout(layout)
        return target

    def add_lobby_element(self, element: Dict[str, Any]) -> Dict[str, Any]:
        if "id" not in element or not str(element["id"]).strip():
            raise ValueError("Lobby element requires a non-empty 'id'")
        elem_type = element.get("type", "text")
        if elem_type not in ("sprite", "text", "guide_card", "button", "panel"):
            raise ValueError("Lobby element type must be one of: sprite, text, guide_card, button, panel")

        layout = self.load_ui_layout()
        elems = layout.setdefault("lobby", {}).setdefault("elements", [])
        if any(e.get("id") == element["id"] for e in elems):
            raise ValueError(f"Duplicate lobby element ID: {element['id']}")

        rec = {
            "id": str(element["id"]).strip(),
            "label": element.get("label", element["id"]),
            "type": elem_type,
            "visible": bool(element.get("visible", True)),
            "x": int(element.get("x", 440)),
            "y": int(element.get("y", 200)),
            "width": int(element.get("width", 400 if elem_type == "text" else 160)),
            "height": int(element.get("height", 40 if elem_type == "text" else 120)),
        }
        for opt in ("action", "art_id", "frame_w", "frame_h", "text_key", "text", "text_keys", "font_size", "align"):
            if opt in element:
                rec[opt] = element[opt]
        for cf in ("color", "bg_color", "hover_color", "border_color"):
            if cf in element:
                rec[cf] = self.parse_rgba(element[cf])
        if elem_type == "text" and "color" not in rec:
            rec["color"] = [255, 255, 255, 255]
            rec.setdefault("font_size", 22)
            rec.setdefault("align", "center")
            rec.setdefault("text", "새 로비 텍스트")
        if elem_type == "sprite":
            rec.setdefault("art_id", 509)
            rec.setdefault("color", [255, 255, 255, 255])
        if elem_type == "panel":
            rec.setdefault("bg_color", [20, 24, 38, 210])
            rec.setdefault("border_color", [160, 140, 95, 200])

        elems.append(rec)
        self.save_ui_layout(layout)
        return rec

    def delete_lobby_element(self, elem_id: str) -> Dict[str, Any]:
        layout = self.load_ui_layout()
        elems = layout.setdefault("lobby", {}).setdefault("elements", [])
        before = len(elems)
        layout["lobby"]["elements"] = [e for e in elems if e.get("id") != elem_id]
        if len(layout["lobby"]["elements"]) == before:
            raise ValueError(f"Lobby element not found: {elem_id}")
        self.save_ui_layout(layout)
        return {"deleted": elem_id}

    def reset_lobby_layout(self) -> Dict[str, Any]:
        layout = self.load_ui_layout()
        layout["lobby"] = self.default_ui_layout()["lobby"]
        self.save_ui_layout(layout)
        return layout["lobby"]

    def get_hud_layout(self) -> Dict[str, Any]:
        layout = self.load_ui_layout()
        return {
            "reference_width": layout.get("reference_width", 1280),
            "reference_height": layout.get("reference_height", 720),
            "widgets": layout.get("hud", {}).get("widgets", []),
        }

    def get_hud_widget(self, widget_id: str) -> Dict[str, Any]:
        for w in self.get_hud_layout()["widgets"]:
            if w.get("id") == widget_id:
                return w
        raise ValueError(f"HUD widget not found: {widget_id}")

    def update_hud_widget(self, widget_id: str, updates: Dict[str, Any]) -> Dict[str, Any]:
        layout = self.load_ui_layout()
        widgets = layout.setdefault("hud", {}).setdefault("widgets", [])
        target = next((w for w in widgets if w.get("id") == widget_id), None)
        if target is None:
            raise ValueError(f"HUD widget not found: {widget_id}")

        color_fields = {"bg_color", "border_color", "accent_color"}
        int_fields = {"x", "y", "width", "height", "cell_size"}
        for k, v in updates.items():
            if v is None:
                continue
            if k in color_fields:
                target[k] = self.parse_rgba(v)
            elif k in int_fields:
                target[k] = int(round(float(v)))
            elif k == "visible":
                target[k] = bool(v) if not isinstance(v, str) else v.lower() in ("true", "1", "yes")
            else:
                target[k] = v

        self.save_ui_layout(layout)
        return target

    def reset_hud_layout(self) -> Dict[str, Any]:
        layout = self.load_ui_layout()
        layout["hud"] = self.default_ui_layout()["hud"]
        self.save_ui_layout(layout)
        return layout["hud"]

    def validate_ui_layout(self) -> Dict[str, Any]:
        issues: List[str] = []
        if not self.ui_layout_path.exists():
            return {"ok": False, "issues": ["[ui_layout.json] Missing file data/ui_layout.json"]}
        layout = self.load_ui_layout()
        rw = int(layout.get("reference_width", 1280))
        rh = int(layout.get("reference_height", 720))
        art_ids = {e["art_id"] for e in self.list_art_mappings() if e["exists"]}
        strings = self.get_string_map()

        for e in layout.get("lobby", {}).get("elements", []):
            eid = e.get("id", "?")
            if e.get("width", 0) <= 0 or e.get("height", 0) <= 0:
                issues.append(f"[ui_layout.json] Lobby '{eid}' has non-positive size")
            if e.get("type") == "sprite" and e.get("art_id") not in art_ids:
                issues.append(f"[ui_layout.json] Lobby '{eid}' references missing art_id {e.get('art_id')}")
            tk_key = e.get("text_key", "")
            if tk_key and tk_key not in strings:
                issues.append(f"[ui_layout.json] Lobby '{eid}' references unknown text_key '{tk_key}'")
            for gtk in e.get("text_keys", []):
                if gtk not in strings:
                    issues.append(f"[ui_layout.json] Lobby '{eid}' references unknown guide text_key '{gtk}'")

        for w in layout.get("hud", {}).get("widgets", []):
            wid = w.get("id", "?")
            if w.get("width", 0) <= 0 or w.get("height", 0) <= 0:
                issues.append(f"[ui_layout.json] HUD widget '{wid}' has non-positive size")
            if w.get("x", 0) < -rw or w.get("x", 0) > rw * 2 or w.get("y", 0) < -rh or w.get("y", 0) > rh * 2:
                issues.append(f"[ui_layout.json] HUD widget '{wid}' position far outside screen")

        return {"ok": len(issues) == 0, "issues": issues}
