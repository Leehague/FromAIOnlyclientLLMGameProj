"""
Unified CLI + Desktop UI entrypoint for Chronicle of the Ashen Crown Game Designer Tool.

Usage:
  python tools/designer_tool.py ui                          # Launch Desktop GUI
  python tools/designer_tool.py map <subcommand> ...        # Map creation & editing
  python tools/designer_tool.py art <subcommand> ...        # Art upload & verification
  python tools/designer_tool.py art-req <subcommand> ...    # Art requirement queue (for LLM)
  python tools/designer_tool.py doc <subcommand> ...        # Design JSON viewer, search & editor
  python tools/designer_tool.py validate [--json]           # Full cross-reference validation
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

# Ensure local imports work regardless of working directory
THIS_DIR = Path(__file__).resolve().parent
if str(THIS_DIR) not in sys.path:
    sys.path.insert(0, str(THIS_DIR))

from designer_core import ART_CATEGORIES, DESIGN_DOCS, SKILL_COST_TYPES, SKILL_EFFECT_CATALOG, DesignerCore


def _configure_utf8_io() -> None:
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            try:
                stream.reconfigure(encoding="utf-8")
            except Exception:
                pass


def _emit(data: Any, as_json: bool, human_fn=None) -> None:
    if as_json or human_fn is None:
        print(json.dumps(data, ensure_ascii=False, indent=2))
    else:
        human_fn(data)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="designer_tool.py",
        description="Chronicle of the Ashen Crown — Game Designer Tool (UI + CLI)",
    )
    parser.add_argument("--root", default=None, help="Optional project root directory override")
    parser.add_argument("--json", action="store_true", dest="as_json", help="Output structured JSON (recommended for LLM/CLI automation)")

    sub = parser.add_subparsers(dest="command")

    # --- UI ---
    p_ui = sub.add_parser("ui", help="Launch the Desktop GUI (tkinter)")
    p_ui.add_argument("--self-test", action="store_true", help="Instantiate all GUI tabs headlessly and exit (for verification)")

    # --- VALIDATE ALL ---
    p_val = sub.add_parser("validate", help="Run full cross-reference validation across maps, art, and all design JSONs")
    p_val.add_argument("--json", action="store_true", dest="as_json")

    # --- 1. MAP ---
    p_map = sub.add_parser("map", help="Map creation, tile editing, flood fill, resize, validation")
    map_sub = p_map.add_subparsers(dest="subcmd", required=True)

    p = map_sub.add_parser("list", help="List all maps and dimensions")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = map_sub.add_parser("show", help="Print a map's tile grid and metadata")
    p.add_argument("map_id")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = map_sub.add_parser("create", help="Create a new tilemap (.txt) and register in data/maps.json")
    p.add_argument("--id", required=True, dest="map_id")
    p.add_argument("--name", required=True)
    p.add_argument("--type", default="field", choices=["town", "field", "boss"], dest="map_type")
    p.add_argument("--width", type=int, default=24)
    p.add_argument("--height", type=int, default=18)
    p.add_argument("--fill-tile", type=int, default=0)
    p.add_argument("--border-tile", type=int, default=6)
    p.add_argument("--json", action="store_true", dest="as_json")

    p = map_sub.add_parser("delete", help="Delete a map from data/maps.json (and its .txt file)")
    p.add_argument("--id", required=True, dest="map_id")
    p.add_argument("--keep-txt", action="store_true")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = map_sub.add_parser("set-tile", help="Set a single tile coordinate (x, y) to tile_id")
    p.add_argument("--id", required=True, dest="map_id")
    p.add_argument("--x", type=int, required=True)
    p.add_argument("--y", type=int, required=True)
    p.add_argument("--tile", type=int, required=True)
    p.add_argument("--json", action="store_true", dest="as_json")

    p = map_sub.add_parser("fill", help="Fill a rectangular region (x0,y0)-(x1,y1) with tile_id")
    p.add_argument("--id", required=True, dest="map_id")
    p.add_argument("--x0", type=int, required=True)
    p.add_argument("--y0", type=int, required=True)
    p.add_argument("--x1", type=int, required=True)
    p.add_argument("--y1", type=int, required=True)
    p.add_argument("--tile", type=int, required=True)
    p.add_argument("--json", action="store_true", dest="as_json")

    p = map_sub.add_parser("flood-fill", help="Flood fill contiguous region starting at (x, y)")
    p.add_argument("--id", required=True, dest="map_id")
    p.add_argument("--x", type=int, required=True)
    p.add_argument("--y", type=int, required=True)
    p.add_argument("--tile", type=int, required=True)
    p.add_argument("--json", action="store_true", dest="as_json")

    p = map_sub.add_parser("resize", help="Resize a map grid")
    p.add_argument("--id", required=True, dest="map_id")
    p.add_argument("--width", type=int, required=True)
    p.add_argument("--height", type=int, required=True)
    p.add_argument("--fill-tile", type=int, default=0)
    p.add_argument("--json", action="store_true", dest="as_json")

    p = map_sub.add_parser("validate", help="Validate all maps, warps, and entity spawns")
    p.add_argument("--json", action="store_true", dest="as_json")

    # --- 2. ART UPLOAD & MAPPING ---
    p_art = sub.add_parser("art", help="Art resource upload, listing, removal, verification")
    art_sub = p_art.add_subparsers(dest="subcmd", required=True)

    p = art_sub.add_parser("list", help="List all art resources mapped in data/art_mapping.txt")
    p.add_argument("--category", default="all", choices=["all"] + list(ART_CATEGORIES.keys()))
    p.add_argument("--json", action="store_true", dest="as_json")

    p = art_sub.add_parser("upload", help="Upload/convert image to PNG, place in assets/, register in art_mapping.txt")
    p.add_argument("--file", required=True, dest="source_file")
    p.add_argument("--category", required=True, choices=list(ART_CATEGORIES.keys()))
    p.add_argument("--id", type=int, default=None, dest="art_id")
    p.add_argument("--name", default=None, dest="target_name")
    p.add_argument("--resize", default=None, help="Optional WxH resize (nearest-neighbor), e.g. 32x32")
    p.add_argument("--concept", default="", help="Art concept note to append to docs/art_resources.md")
    p.add_argument("--fulfill-request", default=None, help="Optional REQ-XXX ID to mark fulfilled")
    p.add_argument("--overwrite", action="store_true", help="Allow replacing an existing Art ID")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = art_sub.add_parser("remove", help="Remove an Art ID from data/art_mapping.txt")
    p.add_argument("--id", type=int, required=True, dest="art_id")
    p.add_argument("--delete-file", action="store_true")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = art_sub.add_parser("verify", help="Verify all mapped art files exist on disk")
    p.add_argument("--json", action="store_true", dest="as_json")

    # --- 3. ART RESOURCE REQUIREMENTS (LLM QUEUE) ---
    p_req = sub.add_parser("art-req", help="Manage pending art resource requirements for LLM image generation")
    req_sub = p_req.add_subparsers(dest="subcmd", required=True)

    p = req_sub.add_parser("list", help="List art resource requests (default: pending)")
    p.add_argument("--status", default="pending", choices=["pending", "fulfilled", "cancelled", "all"])
    p.add_argument("--json", action="store_true", dest="as_json")

    p = req_sub.add_parser("add", help="Register a new 'pending' art resource requirement")
    p.add_argument("--title", required=True)
    p.add_argument("--prompt", required=True, help="Short description / prompt for the required art asset")
    p.add_argument("--category", default="items", choices=list(ART_CATEGORIES.keys()))
    p.add_argument("--size", default=None, help="Target pixel dimensions, e.g. 32x32 or 160x40")
    p.add_argument("--art-id", type=int, default=None, dest="suggested_art_id")
    p.add_argument("--target-path", default="")
    p.add_argument("--notes", default="")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = req_sub.add_parser("update", help="Update an existing art requirement")
    p.add_argument("--id", required=True, dest="request_id")
    p.add_argument("--title", default=None)
    p.add_argument("--prompt", default=None)
    p.add_argument("--category", default=None, choices=list(ART_CATEGORIES.keys()))
    p.add_argument("--size", default=None)
    p.add_argument("--status", default=None, choices=["pending", "fulfilled", "cancelled"])
    p.add_argument("--notes", default=None)
    p.add_argument("--json", action="store_true", dest="as_json")

    p = req_sub.add_parser("fulfill", help="Mark an art requirement as fulfilled and link to an Art ID")
    p.add_argument("--id", required=True, dest="request_id")
    p.add_argument("--art-id", type=int, required=True, dest="art_id")
    p.add_argument("--path", default=None, dest="final_path")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = req_sub.add_parser("delete", help="Delete an art requirement")
    p.add_argument("--id", required=True, dest="request_id")
    p.add_argument("--json", action="store_true", dest="as_json")

    # --- 4. DESIGN DOCUMENT (JSON) EDITOR & SEARCH ---
    p_doc = sub.add_parser("doc", help="Inspect, search, and edit game design JSON documents")
    doc_sub = p_doc.add_subparsers(dest="subcmd", required=True)

    p = doc_sub.add_parser("list", help="List all supported design JSON documents and record counts")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = doc_sub.add_parser("search", help="Search across one or all design JSON documents")
    p.add_argument("--query", required=True)
    p.add_argument("--doc", default=None, choices=list(DESIGN_DOCS.keys()))
    p.add_argument("--json", action="store_true", dest="as_json")

    p = doc_sub.add_parser("get", help="Get all records or a single record by ID from a design JSON")
    p.add_argument("--doc", required=True, choices=list(DESIGN_DOCS.keys()))
    p.add_argument("--id", default=None, dest="record_id")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = doc_sub.add_parser("set", help="Edit field(s) of a record in a design JSON")
    p.add_argument("--doc", required=True, choices=list(DESIGN_DOCS.keys()))
    p.add_argument("--id", required=True, dest="record_id")
    p.add_argument("--field", default=None, help="Single field name to modify")
    p.add_argument("--value", default=None, help="New value for --field (auto-coerced to field type)")
    p.add_argument("--patch-json", default=None, help="JSON object of multiple field updates")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = doc_sub.add_parser("add", help="Add a new record to a design JSON")
    p.add_argument("--doc", required=True, choices=list(DESIGN_DOCS.keys()))
    p.add_argument("--json-data", required=True, help="JSON object string for the new record")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = doc_sub.add_parser("delete", help="Delete a record by ID from a design JSON")
    p.add_argument("--doc", required=True, choices=list(DESIGN_DOCS.keys()))
    p.add_argument("--id", required=True, dest="record_id")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = doc_sub.add_parser("validate", help="Validate all design JSON schemas & cross-references")
    p.add_argument("--json", action="store_true", dest="as_json")

    # --- 5. LOBBY SCREEN EDITOR ---
    p_lob = sub.add_parser("lobby", help="Edit lobby/title screen layout, background, and elements (data/ui_layout.json)")
    lob_sub = p_lob.add_subparsers(dest="subcmd", required=True)

    p = lob_sub.add_parser("show", help="Show lobby screen layout and all elements")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = lob_sub.add_parser("set-bg", help="Set lobby vertical gradient background colors (R,G,B,A or #RRGGBB)")
    p.add_argument("--top", default=None, help="Top RGBA color, e.g. 18,22,34,255 or #121622")
    p.add_argument("--bottom", default=None, help="Bottom RGBA color, e.g. 38,20,26,255 or #26141a")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = lob_sub.add_parser("set", help="Edit a lobby screen element's position, size, style, art, or text")
    p.add_argument("--id", required=True, dest="elem_id")
    p.add_argument("--x", type=int, default=None)
    p.add_argument("--y", type=int, default=None)
    p.add_argument("--width", type=int, default=None)
    p.add_argument("--height", type=int, default=None)
    p.add_argument("--visible", default=None, choices=["true", "false"])
    p.add_argument("--art-id", type=int, default=None, dest="art_id")
    p.add_argument("--font-size", type=int, default=None, dest="font_size")
    p.add_argument("--text-key", default=None, dest="text_key")
    p.add_argument("--text", default=None)
    p.add_argument("--align", default=None, choices=["left", "center", "right"])
    p.add_argument("--color", default=None)
    p.add_argument("--bg-color", default=None, dest="bg_color")
    p.add_argument("--hover-color", default=None, dest="hover_color")
    p.add_argument("--border-color", default=None, dest="border_color")
    p.add_argument("--patch-json", default=None)
    p.add_argument("--json", action="store_true", dest="as_json")

    p = lob_sub.add_parser("add", help="Add a custom element (text/sprite/panel/button) to the lobby screen")
    p.add_argument("--json-data", required=True)
    p.add_argument("--json", action="store_true", dest="as_json")

    p = lob_sub.add_parser("delete", help="Delete an element from the lobby screen")
    p.add_argument("--id", required=True, dest="elem_id")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = lob_sub.add_parser("reset", help="Reset lobby screen layout to defaults")
    p.add_argument("--json", action="store_true", dest="as_json")

    # --- 6. IN-PLAY UI (HUD & POPUPS) EDITOR ---
    p_hud = sub.add_parser("ui-hud", help="Edit in-play HUD & popup UI layout (data/ui_layout.json)")
    hud_sub = p_hud.add_subparsers(dest="subcmd", required=True)

    p = hud_sub.add_parser("list", help="List all in-play UI widgets and their geometry")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = hud_sub.add_parser("get", help="Show a single in-play UI widget spec")
    p.add_argument("--id", required=True, dest="widget_id")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = hud_sub.add_parser("set", help="Edit an in-play UI widget's position, size, visibility, or colors")
    p.add_argument("--id", required=True, dest="widget_id")
    p.add_argument("--x", type=int, default=None)
    p.add_argument("--y", type=int, default=None)
    p.add_argument("--width", type=int, default=None)
    p.add_argument("--height", type=int, default=None)
    p.add_argument("--visible", default=None, choices=["true", "false"])
    p.add_argument("--cell-size", type=int, default=None, dest="cell_size")
    p.add_argument("--bg-color", default=None, dest="bg_color")
    p.add_argument("--border-color", default=None, dest="border_color")
    p.add_argument("--accent-color", default=None, dest="accent_color")
    p.add_argument("--patch-json", default=None)
    p.add_argument("--json", action="store_true", dest="as_json")

    p = hud_sub.add_parser("reset", help="Reset in-play UI layout to defaults")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = hud_sub.add_parser("validate", help="Validate data/ui_layout.json references and bounds")
    p.add_argument("--json", action="store_true", dest="as_json")

    # --- 7. SKILL PIPELINE COMPOSER ---
    p_sk = sub.add_parser("skill", help="Compose and edit data-driven skill pipelines (data/skills.json)")
    sk_sub = p_sk.add_subparsers(dest="subcmd", required=True)

    p = sk_sub.add_parser("effects", help="List all registered atomic skill effect handlers and parameters")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = sk_sub.add_parser("config", help="View or update global skill slot config (default 3 .. max 10)")
    p.add_argument("--default-slots", type=int, default=None, dest="default_slots")
    p.add_argument("--max-slots", type=int, default=None, dest="max_slots")
    p.add_argument("--expand-cost", type=int, default=None, dest="expand_cost")
    p.add_argument("--default-equipped", default=None, help="Comma-separated default equipped skill IDs")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = sk_sub.add_parser("list", help="List all skills and their composed effect pipelines")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = sk_sub.add_parser("show", help="Show a skill's full spec and nested effect pipeline tree")
    p.add_argument("--id", required=True, dest="skill_id")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = sk_sub.add_parser("create", help="Create a new skill in data/skills.json")
    p.add_argument("--id", required=True, dest="skill_id")
    p.add_argument("--name", required=True)
    p.add_argument("--desc", default="", dest="description")
    p.add_argument("--icon", type=int, default=602, dest="icon_art_id")
    p.add_argument("--cost-type", default="mp", choices=list(SKILL_COST_TYPES), dest="cost_type")
    p.add_argument("--cost", type=int, default=20, dest="cost_amount")
    p.add_argument("--cooldown", type=float, default=4.0)
    p.add_argument("--unlock-level", type=int, default=1, dest="unlock_level")
    p.add_argument("--effects-json", default=None, dest="effects_json", help="Optional JSON array of effects")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = sk_sub.add_parser("duplicate", help="Duplicate an existing skill with its entire effect pipeline")
    p.add_argument("--id", required=True, dest="skill_id")
    p.add_argument("--new-id", required=True, dest="new_id")
    p.add_argument("--name", default=None)
    p.add_argument("--json", action="store_true", dest="as_json")

    p = sk_sub.add_parser("set-meta", help="Edit a skill's metadata (name, description, cost, cooldown, unlock_level)")
    p.add_argument("--id", required=True, dest="skill_id")
    p.add_argument("--name", default=None)
    p.add_argument("--desc", default=None, dest="description")
    p.add_argument("--icon", type=int, default=None, dest="icon_art_id")
    p.add_argument("--cost-type", default=None, choices=list(SKILL_COST_TYPES), dest="cost_type")
    p.add_argument("--cost", type=int, default=None, dest="cost_amount")
    p.add_argument("--cooldown", type=float, default=None)
    p.add_argument("--unlock-level", type=int, default=None, dest="unlock_level")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = sk_sub.add_parser("delete", help="Delete a skill from data/skills.json")
    p.add_argument("--id", required=True, dest="skill_id")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = sk_sub.add_parser("add-effect", help="Add an atomic effect node to root or nested on_hit/on_explode branch")
    p.add_argument("--id", required=True, dest="skill_id")
    p.add_argument("--type", required=True, choices=list(SKILL_EFFECT_CATALOG.keys()), dest="effect_type")
    p.add_argument("--parent", default=None, dest="parent_path", help="Parent node path e.g. '0' or '0.on_hit.0'")
    p.add_argument("--branch", default=None, choices=["on_hit", "on_explode"])
    p.add_argument("--index", type=int, default=None)
    p.add_argument("--params-json", default=None, dest="params_json", help="JSON object of effect parameters")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = sk_sub.add_parser("set-effect", help="Update parameter(s) of a pipeline effect node by path")
    p.add_argument("--id", required=True, dest="skill_id")
    p.add_argument("--path", required=True, dest="node_path", help="Node path e.g. '0' or '0.on_hit.0.on_explode.1'")
    p.add_argument("--field", default=None)
    p.add_argument("--value", default=None)
    p.add_argument("--params-json", default=None, dest="params_json")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = sk_sub.add_parser("remove-effect", help="Remove a pipeline effect node by path")
    p.add_argument("--id", required=True, dest="skill_id")
    p.add_argument("--path", required=True, dest="node_path")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = sk_sub.add_parser("move-effect", help="Move an effect node up or down within its sibling list")
    p.add_argument("--id", required=True, dest="skill_id")
    p.add_argument("--path", required=True, dest="node_path")
    p.add_argument("--dir", required=True, choices=["up", "down"], dest="direction")
    p.add_argument("--json", action="store_true", dest="as_json")

    p = sk_sub.add_parser("validate", help="Validate data/skills.json effect types, art IDs, costs, and slot bounds")
    p.add_argument("--json", action="store_true", dest="as_json")

    return parser


def main(argv=None) -> int:
    _configure_utf8_io()
    parser = build_parser()
    args = parser.parse_args(argv)
    core = DesignerCore(args.root)
    as_json = bool(getattr(args, "as_json", False))

    # Default with no subcommand -> launch UI
    if args.command is None or args.command == "ui":
        from designer_ui import DesignerApp, launch_gui
        if getattr(args, "self_test", False):
            app = DesignerApp(core)
            app.withdraw()
            app.update_idletasks()
            tabs = [app.notebook.tab(i, "text").strip() for i in range(app.notebook.index("end"))]
            app.destroy()
            print(json.dumps({"ui_self_test": "PASS", "tabs": tabs}, ensure_ascii=False))
            return 0
        return launch_gui(core)

    try:
        if args.command == "validate":
            res = core.validate_all_docs()
            _emit(res, as_json, lambda d: print(
                f"[VALIDATE] {'PASS' if d['ok'] else 'FAIL'} (issues: {d['issue_count']})"
                + ("" if d["ok"] else "\n" + "\n".join(" - " + i for i in d["issues"]))
            ))
            return 0 if res["ok"] else 1

        # ---------- MAP ----------
        if args.command == "map":
            sc = args.subcmd
            if sc == "list":
                data = core.list_maps()
                _emit(data, as_json, lambda rows: [
                    print(f"{r['id']:12s} | {r['name']:16s} | {r['type']:5s} | {r['width']}x{r['height']} | {r['txt_path']}")
                    for r in rows
                ])
            elif sc == "show":
                data = core.load_map(args.map_id)
                def _show(d):
                    print(f"Map: {d['meta']['id']} ({d['meta']['name']}) | {d['width']}x{d['height']}")
                    for row in d["grid"]:
                        print(" ".join(f"{t:02d}" for t in row))
                _emit(data, as_json, _show)
            elif sc == "create":
                res = core.create_map(
                    args.map_id, args.name, args.map_type, args.width, args.height,
                    args.fill_tile, args.border_tile
                )
                _emit(res, as_json)
            elif sc == "delete":
                res = core.delete_map(args.map_id, remove_txt=not args.keep_txt)
                _emit(res, as_json)
            elif sc == "set-tile":
                res = core.set_map_tile(args.map_id, args.x, args.y, args.tile)
                _emit(res, as_json)
            elif sc == "fill":
                res = core.fill_map_rect(args.map_id, args.x0, args.y0, args.x1, args.y1, args.tile)
                _emit(res, as_json)
            elif sc == "flood-fill":
                m = core.load_map(args.map_id)
                changed = DesignerCore.flood_fill_grid(m["grid"], args.x, args.y, args.tile)
                core.save_map_grid(args.map_id, m["grid"], m["comments"])
                _emit({"map_id": args.map_id, "changed": changed, "tile": args.tile}, as_json)
            elif sc == "resize":
                res = core.resize_map(args.map_id, args.width, args.height, args.fill_tile)
                _emit(res, as_json)
            elif sc == "validate":
                res = core.validate_maps()
                _emit(res, as_json)
                return 0 if res["ok"] else 1
            return 0

        # ---------- ART ----------
        if args.command == "art":
            sc = args.subcmd
            if sc == "list":
                data = core.list_art_mappings()
                if args.category != "all":
                    data = [e for e in data if e["category"] == args.category]
                _emit(data, as_json, lambda rows: [
                    print(f"{r['art_id']:4d} | {r['category']:10s} | {r['size']:8s} | {'OK' if r['exists'] else 'MISS':4s} | {r['path']}")
                    for r in rows
                ])
            elif sc == "upload":
                res = core.upload_art_resource(
                    source_file=args.source_file,
                    category=args.category,
                    art_id=args.art_id,
                    target_name=args.target_name,
                    resize=args.resize,
                    concept=args.concept,
                    fulfill_request_id=args.fulfill_request,
                    overwrite=args.overwrite,
                )
                _emit(res, as_json)
            elif sc == "remove":
                res = core.remove_art_mapping(args.art_id, delete_file=args.delete_file)
                _emit(res, as_json)
            elif sc == "verify":
                res = core.verify_art_resources()
                _emit(res, as_json)
                return 0 if res["ok"] else 1
            return 0

        # ---------- ART REQUESTS ----------
        if args.command == "art-req":
            sc = args.subcmd
            if sc == "list":
                data = core.list_art_requests(status=args.status)
                _emit(data, as_json, lambda rows: (
                    print("(항목 없음)") if not rows else [
                        print(f"{r['request_id']} | {r['status']:9s} | {r['category']:10s} | ID:{str(r['suggested_art_id']):4s} | {r['title']} — {r['prompt']}")
                        for r in rows
                    ]
                ))
            elif sc == "add":
                res = core.add_art_request(
                    title=args.title, prompt=args.prompt, category=args.category,
                    size=args.size, suggested_art_id=args.suggested_art_id,
                    target_path=args.target_path, notes=args.notes,
                )
                _emit(res, as_json)
            elif sc == "update":
                updates = {}
                for k in ("title", "prompt", "category", "size", "status", "notes"):
                    v = getattr(args, k, None)
                    if v is not None:
                        updates[k] = v
                res = core.update_art_request(args.request_id, updates)
                _emit(res, as_json)
            elif sc == "fulfill":
                res = core.fulfill_art_request(args.request_id, args.art_id, args.final_path)
                _emit(res, as_json)
            elif sc == "delete":
                res = core.delete_art_request(args.request_id)
                _emit(res, as_json)
            return 0

        # ---------- DESIGN DOC (JSON) ----------
        if args.command == "doc":
            sc = args.subcmd
            if sc == "list":
                data = core.list_design_docs()
                _emit(data, as_json, lambda rows: [
                    print(f"{r['doc']:10s} | {r['count']:3d} entries | {r['file']:20s} | {r['label']}")
                    for r in rows
                ])
            elif sc == "search":
                hits = core.search_design_docs(args.query, doc_key=args.doc)
                _emit(hits, as_json, lambda rows: (
                    print("(검색 결과 없음)") if not rows else [
                        print(f"[{r['doc']}] {r['label']}  (fields: {', '.join(r['matched_fields'])})")
                        for r in rows
                    ]
                ))
            elif sc == "get":
                data = core.get_doc_record(args.doc, args.record_id)
                _emit(data, True)
            elif sc == "set":
                updates = {}
                coerce = True
                if args.patch_json:
                    updates = json.loads(args.patch_json)
                    coerce = False
                elif args.field is not None and args.value is not None:
                    updates = {args.field: args.value}
                    coerce = True
                else:
                    raise ValueError("Provide either (--field and --value) or --patch-json")
                res = core.update_doc_record(args.doc, args.record_id, updates, coerce=coerce)
                _emit(res, as_json)
            elif sc == "add":
                rec = json.loads(args.json_data)
                res = core.add_doc_record(args.doc, rec)
                _emit(res, as_json)
            elif sc == "delete":
                res = core.delete_doc_record(args.doc, args.record_id)
                _emit(res, as_json)
            elif sc == "validate":
                res = core.validate_all_docs()
                _emit(res, as_json)
                return 0 if res["ok"] else 1
            return 0

        # ---------- LOBBY SCREEN ----------
        if args.command == "lobby":
            sc = args.subcmd
            if sc == "show":
                data = core.get_lobby_layout()
                def _show_lob(d):
                    lob = d["lobby"]
                    print(f"Lobby Canvas: {d['reference_width']}x{d['reference_height']} | BG: {lob.get('bg_top_color')} -> {lob.get('bg_bottom_color')}")
                    for e in lob.get("elements", []):
                        print(f"  {e['id']:16s} | {e['type']:10s} | vis={str(e.get('visible',True)):5s} | ({e['x']},{e['y']}) {e['width']}x{e['height']} | {e.get('label','')}")
                _emit(data, as_json, _show_lob)
            elif sc == "set-bg":
                res = core.update_lobby_bg(args.top, args.bottom)
                _emit(res, as_json)
            elif sc == "set":
                updates = json.loads(args.patch_json) if args.patch_json else {}
                for k in ("x", "y", "width", "height", "visible", "art_id", "font_size", "text_key", "text", "align", "color", "bg_color", "hover_color", "border_color"):
                    v = getattr(args, k, None)
                    if v is not None:
                        updates[k] = v
                res = core.update_lobby_element(args.elem_id, updates)
                _emit(res, as_json)
            elif sc == "add":
                res = core.add_lobby_element(json.loads(args.json_data))
                _emit(res, as_json)
            elif sc == "delete":
                res = core.delete_lobby_element(args.elem_id)
                _emit(res, as_json)
            elif sc == "reset":
                res = core.reset_lobby_layout()
                _emit(res, as_json)
            return 0

        # ---------- IN-PLAY UI (HUD) ----------
        if args.command == "ui-hud":
            sc = args.subcmd
            if sc == "list":
                data = core.get_hud_layout()
                _emit(data, as_json, lambda d: [
                    print(f"{w['id']:18s} | {w.get('group','hud'):5s} | vis={str(w.get('visible',True)):5s} | ({w['x']:4d},{w['y']:4d}) {w['width']:3d}x{w['height']:3d} | {w.get('label','')}")
                    for w in d["widgets"]
                ])
            elif sc == "get":
                _emit(core.get_hud_widget(args.widget_id), True)
            elif sc == "set":
                updates = json.loads(args.patch_json) if args.patch_json else {}
                for k in ("x", "y", "width", "height", "visible", "cell_size", "bg_color", "border_color", "accent_color"):
                    v = getattr(args, k, None)
                    if v is not None:
                        updates[k] = v
                res = core.update_hud_widget(args.widget_id, updates)
                _emit(res, as_json)
            elif sc == "reset":
                res = core.reset_hud_layout()
                _emit(res, as_json)
            elif sc == "validate":
                res = core.validate_ui_layout()
                _emit(res, as_json)
                return 0 if res["ok"] else 1
            return 0

        # ---------- SKILL PIPELINE ----------
        if args.command == "skill":
            sc = args.subcmd
            if sc == "effects":
                data = core.list_effect_types()
                _emit(data, as_json, lambda rows: [
                    print(f"{r['type']:18s} | child={str(r['child_branch'] or '-'):10s} | {r['label']} — {r['description']}")
                    for r in rows
                ])
            elif sc == "config":
                if any(v is not None for v in (args.default_slots, args.max_slots, args.expand_cost, args.default_equipped)):
                    deq = [s.strip() for s in args.default_equipped.split(",") if s.strip()] if args.default_equipped is not None else None
                    res = core.update_skill_config(args.default_slots, args.max_slots, args.expand_cost, deq)
                else:
                    res = core.get_skill_config()
                _emit(res, as_json)
            elif sc == "list":
                data = core.list_skills()
                _emit(data, as_json, lambda rows: [
                    print(f"{r['id']:18s} | {r['name']:14s} | {r['cost_type']}:{r['cost_amount']:<3d} | CD {r['cooldown']:4.1f}s | Lv.{r['unlock_level']} | {r['pipeline']}")
                    for r in rows
                ])
            elif sc == "show":
                sk = core.get_skill(args.skill_id)
                def _show_skill(s):
                    print(f"[{s['id']}] {s['name']} | {s.get('cost_type','none')}:{s.get('cost_amount',0)} | CD {s.get('cooldown',0)}s | Lv.{s.get('unlock_level',1)}")
                    print(f"  설명: {s.get('description','')}")
                    def _print_tree(efs, prefix="", indent="  "):
                        for idx, ef in enumerate(efs):
                            path = f"{prefix}{idx}"
                            summary = ", ".join(f"{k}={v}" for k, v in ef.items() if k not in ("type", "on_hit", "on_explode"))
                            print(f"{indent}[{path}] {ef.get('type')} ({summary})")
                            if isinstance(ef.get("on_hit"), list) and ef["on_hit"]:
                                print(f"{indent}  ↳ on_hit:")
                                _print_tree(ef["on_hit"], f"{path}.on_hit.", indent + "    ")
                            if isinstance(ef.get("on_explode"), list) and ef["on_explode"]:
                                print(f"{indent}  ↳ on_explode:")
                                _print_tree(ef["on_explode"], f"{path}.on_explode.", indent + "    ")
                    _print_tree(s.get("effects", []))
                _emit(sk, as_json, _show_skill)
            elif sc == "create":
                efs = json.loads(args.effects_json) if args.effects_json else []
                res = core.create_skill(
                    args.skill_id, args.name, args.description, args.icon_art_id,
                    args.cost_type, args.cost_amount, args.cooldown, args.unlock_level, efs
                )
                _emit(res, as_json)
            elif sc == "duplicate":
                res = core.duplicate_skill(args.skill_id, args.new_id, args.name)
                _emit(res, as_json)
            elif sc == "set-meta":
                updates = {}
                for k in ("name", "description", "icon_art_id", "cost_type", "cost_amount", "cooldown", "unlock_level"):
                    v = getattr(args, k, None)
                    if v is not None:
                        updates[k] = v
                res = core.update_skill_meta(args.skill_id, updates)
                _emit(res, as_json)
            elif sc == "delete":
                res = core.delete_skill(args.skill_id)
                _emit(res, as_json)
            elif sc == "add-effect":
                params = json.loads(args.params_json) if args.params_json else {}
                res = core.add_skill_effect(args.skill_id, args.effect_type, params, args.parent_path, args.branch, args.index)
                _emit(res, as_json)
            elif sc == "set-effect":
                updates = json.loads(args.params_json) if args.params_json else {}
                if args.field is not None and args.value is not None:
                    updates[args.field] = args.value
                if not updates:
                    raise ValueError("Provide either (--field and --value) or --params-json")
                res = core.update_skill_effect(args.skill_id, args.node_path, updates)
                _emit(res, as_json)
            elif sc == "remove-effect":
                res = core.remove_skill_effect(args.skill_id, args.node_path)
                _emit(res, as_json)
            elif sc == "move-effect":
                res = core.move_skill_effect(args.skill_id, args.node_path, args.direction)
                _emit(res, as_json)
            elif sc == "validate":
                res = core.validate_skills()
                _emit(res, as_json)
                return 0 if res["ok"] else 1
            return 0

    except Exception as e:
        if as_json:
            print(json.dumps({"error": str(e)}, ensure_ascii=False), file=sys.stderr)
        else:
            print(f"[ERROR] {e}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
