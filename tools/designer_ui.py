"""
Desktop GUI for Chronicle of the Ashen Crown - Game Designer Tool.
Uses Python standard tkinter + Pillow (PIL.ImageTk) and calls tools/designer_core.py.

Tabs:
1. 맵 제작 및 수정 (Map Editor) - Interactive PNG tilemap painter, flood-fill, resize, new map
2. 아트 리소스 업로드 (Art Upload) - Gallery preview, file upload, art_mapping.txt & docs sync
3. 아트 요구사항 기록 (Art Requests) - Pending art prompt queue for future LLM image generation
4. 기획 문서 편집기 (Design Doc Editor) - JSON parser -> structured form UI + single/global search
"""
from __future__ import annotations

import copy
import json
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple
import tkinter as tk
from tkinter import colorchooser, filedialog, messagebox, simpledialog, ttk

from PIL import Image, ImageTk

from designer_core import ART_CATEGORIES, DESIGN_DOCS, DesignerCore


BG_DARK = "#181a24"
BG_PANEL = "#212432"
BG_INPUT = "#2a2e40"
FG_TEXT = "#eef0f7"
FG_MUTED = "#a6adc8"
ACCENT = "#d4a24c"
ACCENT_BLUE = "#5b9bd5"


class DesignerApp(tk.Tk):
    def __init__(self, core: Optional[DesignerCore] = None):
        super().__init__()
        self.core = core or DesignerCore()
        self.title("잿빛 왕관의 연대기 — 기획자용 통합 툴 (UI + CLI)")
        self.geometry("1360x840")
        self.minsize(1120, 700)
        self.configure(bg=BG_DARK)

        self._setup_styles()
        self._photo_cache: Dict[Tuple[str, int, int], ImageTk.PhotoImage] = {}

        # Status bar at bottom
        self.status_var = tk.StringVar(value=f"준비 완료 | 워크스페이스: {self.core.root}")
        status_bar = tk.Label(
            self, textvariable=self.status_var, anchor="w",
            bg="#12141c", fg=ACCENT, padx=12, pady=5, font=("Malgun Gothic", 9)
        )
        status_bar.pack(side="bottom", fill="x")

        # Main notebook (4 tabs)
        self.notebook = ttk.Notebook(self)
        self.notebook.pack(fill="both", expand=True, padx=8, pady=(8, 2))

        self.tab_map = MapEditorTab(self.notebook, self)
        self.tab_art = ArtUploadTab(self.notebook, self)
        self.tab_req = ArtRequestTab(self.notebook, self)
        self.tab_doc = DesignDocTab(self.notebook, self)
        self.tab_lobby = LobbyEditorTab(self.notebook, self)
        self.tab_hud = HudEditorTab(self.notebook, self)

        self.notebook.add(self.tab_map, text="  1. 맵 제작/수정  ")
        self.notebook.add(self.tab_art, text="  2. 아트 업로드  ")
        self.notebook.add(self.tab_req, text="  3. 아트 요구사항  ")
        self.notebook.add(self.tab_doc, text="  4. 기획 문서(JSON)  ")
        self.notebook.add(self.tab_lobby, text="  5. 로비 화면 편집  ")
        self.notebook.add(self.tab_hud, text="  6. 인플레이 UI 편집  ")

        self.notebook.bind("<<NotebookTabChanged>>", self._on_tab_changed)

    def _setup_styles(self) -> None:
        style = ttk.Style(self)
        try:
            style.theme_use("clam")
        except Exception:
            pass
        style.configure(".", background=BG_PANEL, foreground=FG_TEXT, font=("Malgun Gothic", 10))
        style.configure("TFrame", background=BG_PANEL)
        style.configure("TLabel", background=BG_PANEL, foreground=FG_TEXT)
        style.configure("Header.TLabel", background=BG_PANEL, foreground=ACCENT, font=("Malgun Gothic", 11, "bold"))
        style.configure("Muted.TLabel", background=BG_PANEL, foreground=FG_MUTED, font=("Malgun Gothic", 9))
        style.configure("TButton", background="#343a52", foreground=FG_TEXT, padding=(10, 5))
        style.map("TButton", background=[("active", "#454d6e")])
        style.configure("Accent.TButton", background="#8c3b32", foreground="#ffffff", padding=(10, 5))
        style.map("Accent.TButton", background=[("active", "#b34d42")])
        style.configure("TEntry", fieldbackground=BG_INPUT, foreground=FG_TEXT, insertcolor=FG_TEXT)
        style.configure("TCombobox", fieldbackground=BG_INPUT, foreground=FG_TEXT, background="#343a52")
        style.configure("TNotebook", background=BG_DARK, borderwidth=0)
        style.configure("TNotebook.Tab", background="#242838", foreground=FG_MUTED, padding=(14, 7))
        style.map("TNotebook.Tab", background=[("selected", "#353c56")], foreground=[("selected", ACCENT)])
        style.configure("Treeview", background=BG_INPUT, fieldbackground=BG_INPUT, foreground=FG_TEXT, rowheight=26)
        style.configure("Treeview.Heading", background="#2d3247", foreground=ACCENT, font=("Malgun Gothic", 9, "bold"))
        style.map("Treeview", background=[("selected", "#3f537a")])

    def set_status(self, msg: str) -> None:
        self.status_var.set(msg)

    def get_photo(
        self,
        rel_or_abs_path: str | Path,
        w: int,
        h: int,
        force_reload: bool = False,
        crop_size: Optional[Tuple[int, int]] = None,
    ) -> Optional[ImageTk.PhotoImage]:
        p = Path(rel_or_abs_path)
        if not p.is_absolute():
            p = self.core.root / p
        key = (str(p), w, h, crop_size)
        if not force_reload and key in self._photo_cache:
            return self._photo_cache[key]
        if not p.exists():
            return None
        try:
            with Image.open(p) as im:
                im = im.convert("RGBA")
                if crop_size and crop_size[0] > 0 and crop_size[1] > 0:
                    im = im.crop((0, 0, min(im.width, crop_size[0]), min(im.height, crop_size[1])))
                im = im.resize((w, h), Image.Resampling.NEAREST)
                photo = ImageTk.PhotoImage(im)
                self._photo_cache[key] = photo
                return photo
        except Exception:
            return None

    def _on_tab_changed(self, _event=None) -> None:
        idx = self.notebook.index(self.notebook.select())
        if idx == 0:
            self.tab_map.refresh_map_list()
        elif idx == 1:
            self.tab_art.refresh_all()
        elif idx == 2:
            self.tab_req.refresh_requests()
        elif idx == 3:
            self.tab_doc.refresh_records()
        elif idx == 4:
            self.tab_lobby.reload_layout()
        elif idx == 5:
            self.tab_hud.reload_layout()


# ======================================================================
# TAB 1: MAP EDITOR (맵 제작 및 수정)
# ======================================================================
class MapEditorTab(ttk.Frame):
    def __init__(self, parent, app: DesignerApp):
        super().__init__(parent)
        self.app = app
        self.core = app.core

        self.current_map_id: Optional[str] = None
        self.current_comments: List[str] = []
        self.grid_data: List[List[int]] = []
        self.undo_stack: List[List[List[int]]] = []
        self.selected_tile_id: int = 0
        self.tool_mode = tk.StringVar(value="brush")  # brush | fill
        self.cell_px = tk.IntVar(value=28)
        self.show_grid_lines = tk.BooleanVar(value=True)
        self.dirty = False

        self.tile_specs: List[Dict[str, Any]] = []
        self.art_lookup: Dict[int, str] = {}
        self.palette_buttons: Dict[int, tk.Button] = {}
        self._tile_item_ids: List[List[int]] = []

        self._build_ui()
        self.refresh_tile_palette()
        self.refresh_map_list()

    def _build_ui(self) -> None:
        # Top toolbar
        bar = ttk.Frame(self, padding=(8, 6))
        bar.pack(fill="x")

        ttk.Label(bar, text="맵 선택:").pack(side="left", padx=(0, 4))
        self.map_combo = ttk.Combobox(bar, state="readonly", width=26)
        self.map_combo.pack(side="left", padx=(0, 6))
        self.map_combo.bind("<<ComboboxSelected>>", lambda _e: self.load_selected_map())

        ttk.Button(bar, text="새 맵 생성", command=self.open_new_map_dialog).pack(side="left", padx=3)
        ttk.Button(bar, text="크기 조절", command=self.open_resize_dialog).pack(side="left", padx=3)
        ttk.Button(bar, text="되돌리기 (Undo)", command=self.undo).pack(side="left", padx=3)
        ttk.Button(bar, text="검증", command=self.validate_maps).pack(side="left", padx=3)
        ttk.Button(bar, text="맵 저장", style="Accent.TButton", command=self.save_current_map).pack(side="left", padx=(8, 12))

        ttk.Radiobutton(bar, text="브러시(좌클릭/드래그)", variable=self.tool_mode, value="brush").pack(side="left", padx=4)
        ttk.Radiobutton(bar, text="채우기(Flood Fill)", variable=self.tool_mode, value="fill").pack(side="left", padx=4)
        ttk.Checkbutton(bar, text="격자", variable=self.show_grid_lines, command=self.redraw_canvas).pack(side="left", padx=6)

        ttk.Label(bar, text="줌:").pack(side="left", padx=(8, 2))
        zoom_cb = ttk.Combobox(bar, state="readonly", width=5, values=[16, 24, 28, 32, 40], textvariable=self.cell_px)
        zoom_cb.pack(side="left")
        zoom_cb.bind("<<ComboboxSelected>>", lambda _e: self.redraw_canvas())

        self.coord_label = ttk.Label(bar, text="좌표: -", style="Header.TLabel")
        self.coord_label.pack(side="right", padx=8)

        # Body split: Left tile palette, Right scrollable canvas
        body = ttk.PanedWindow(self, orient="horizontal")
        body.pack(fill="both", expand=True, padx=8, pady=(0, 6))

        # Left Palette
        pal_outer = ttk.Frame(body, width=270)
        body.add(pal_outer, weight=0)
        ttk.Label(pal_outer, text="타일 팔레트 (우클릭: 스포이드)", style="Header.TLabel").pack(anchor="w", padx=6, pady=(4, 4))

        pal_canvas = tk.Canvas(pal_outer, bg=BG_INPUT, highlightthickness=0, width=255)
        pal_scroll = ttk.Scrollbar(pal_outer, orient="vertical", command=pal_canvas.yview)
        self.pal_inner = ttk.Frame(pal_canvas)
        self.pal_inner.bind("<Configure>", lambda _e: pal_canvas.configure(scrollregion=pal_canvas.bbox("all")))
        pal_canvas.create_window((0, 0), window=self.pal_inner, anchor="nw")
        pal_canvas.configure(yscrollcommand=pal_scroll.set)
        pal_canvas.pack(side="left", fill="both", expand=True)
        pal_scroll.pack(side="right", fill="y")

        # Right Map Canvas
        canvas_frame = ttk.Frame(body)
        body.add(canvas_frame, weight=1)

        self.canvas = tk.Canvas(canvas_frame, bg="#11131a", highlightthickness=0)
        h_scroll = ttk.Scrollbar(canvas_frame, orient="horizontal", command=self.canvas.xview)
        v_scroll = ttk.Scrollbar(canvas_frame, orient="vertical", command=self.canvas.yview)
        self.canvas.configure(xscrollcommand=h_scroll.set, yscrollcommand=v_scroll.set)
        self.canvas.grid(row=0, column=0, sticky="nsew")
        v_scroll.grid(row=0, column=1, sticky="ns")
        h_scroll.grid(row=1, column=0, sticky="ew")
        canvas_frame.rowconfigure(0, weight=1)
        canvas_frame.columnconfigure(0, weight=1)

        self.canvas.bind("<Button-1>", self._on_canvas_click)
        self.canvas.bind("<B1-Motion>", self._on_canvas_drag)
        self.canvas.bind("<Button-3>", self._on_canvas_eyedrop)
        self.canvas.bind("<Motion>", self._on_canvas_hover)

    def refresh_tile_palette(self) -> None:
        for child in self.pal_inner.winfo_children():
            child.destroy()
        self.palette_buttons.clear()
        self.tile_specs = self.core.list_tiles()
        self.art_lookup = {e["art_id"]: e["path"] for e in self.core.list_art_mappings()}

        for t in self.tile_specs:
            tid = int(t["id"])
            art_path = self.art_lookup.get(int(t["art_id"]), "")
            photo = self.app.get_photo(art_path, 28, 28) if art_path else None
            walk_tag = "통행" if t.get("walkable", True) else "벽"
            text = f" [{tid:02d}] {t['name']} ({walk_tag})"
            btn = tk.Button(
                self.pal_inner, text=text, image=photo, compound="left", anchor="w",
                bg=BG_INPUT, fg=FG_TEXT, activebackground="#3b425e", activeforeground="#ffffff",
                relief="flat", padx=6, pady=4, width=235, font=("Malgun Gothic", 9),
                command=lambda _id=tid: self.select_tile(_id)
            )
            btn.image = photo  # keep reference
            btn.pack(fill="x", padx=4, pady=2)
            self.palette_buttons[tid] = btn

        self.select_tile(self.selected_tile_id if self.selected_tile_id in self.palette_buttons else 0)

    def select_tile(self, tile_id: int) -> None:
        self.selected_tile_id = tile_id
        for tid, btn in self.palette_buttons.items():
            if tid == tile_id:
                btn.configure(bg="#495782", fg=ACCENT)
            else:
                btn.configure(bg=BG_INPUT, fg=FG_TEXT)

    def refresh_map_list(self) -> None:
        maps = self.core.list_maps()
        labels = [f"{m['id']} — {m['name']} ({m['width']}x{m['height']})" for m in maps]
        self._map_ids = [m["id"] for m in maps]
        self.map_combo["values"] = labels
        if self.current_map_id in self._map_ids:
            idx = self._map_ids.index(self.current_map_id)
            self.map_combo.current(idx)
        elif self._map_ids:
            self.map_combo.current(0)
            self.load_selected_map()

    def load_selected_map(self) -> None:
        idx = self.map_combo.current()
        if idx < 0 or idx >= len(self._map_ids):
            return
        mid = self._map_ids[idx]
        loaded = self.core.load_map(mid)
        self.current_map_id = mid
        self.current_comments = loaded["comments"]
        self.grid_data = loaded["grid"]
        self.undo_stack.clear()
        self.dirty = False
        self.redraw_canvas()
        self.app.set_status(f"맵 로드 완료: {mid} ({loaded['width']}x{loaded['height']})")

    def redraw_canvas(self) -> None:
        self.canvas.delete("all")
        self._tile_item_ids = []
        if not self.grid_data:
            return
        cs = int(self.cell_px.get())
        h = len(self.grid_data)
        w = len(self.grid_data[0])
        tile_map = {int(t["id"]): t for t in self.tile_specs}
        self.art_lookup = {e["art_id"]: e["path"] for e in self.core.list_art_mappings()}

        for y in range(h):
            row_ids = []
            for x in range(w):
                tid = self.grid_data[y][x]
                spec = tile_map.get(tid)
                art_path = self.art_lookup.get(int(spec["art_id"]), "") if spec else ""
                photo = self.app.get_photo(art_path, cs, cs) if art_path else None
                if photo:
                    item_id = self.canvas.create_image(x * cs, y * cs, anchor="nw", image=photo)
                else:
                    item_id = self.canvas.create_rectangle(x * cs, y * cs, (x + 1) * cs, (y + 1) * cs, fill="#553366", outline="")
                row_ids.append(item_id)
            self._tile_item_ids.append(row_ids)

        if self.show_grid_lines.get():
            for x in range(w + 1):
                self.canvas.create_line(x * cs, 0, x * cs, h * cs, fill="#262b3d")
            for y in range(h + 1):
                self.canvas.create_line(0, y * cs, w * cs, y * cs, fill="#262b3d")

        self.canvas.configure(scrollregion=(0, 0, w * cs, h * cs))

    def _event_to_tile(self, event) -> Optional[Tuple[int, int]]:
        if not self.grid_data:
            return None
        cs = int(self.cell_px.get())
        cx = int(self.canvas.canvasx(event.x) // cs)
        cy = int(self.canvas.canvasy(event.y) // cs)
        if 0 <= cy < len(self.grid_data) and 0 <= cx < len(self.grid_data[0]):
            return (cx, cy)
        return None

    def _push_undo(self) -> None:
        self.undo_stack.append(copy.deepcopy(self.grid_data))
        if len(self.undo_stack) > 30:
            self.undo_stack.pop(0)

    def _paint_cell(self, x: int, y: int, tid: int) -> None:
        if self.grid_data[y][x] == tid:
            return
        self.grid_data[y][x] = tid
        self.dirty = True
        cs = int(self.cell_px.get())
        spec = next((t for t in self.tile_specs if int(t["id"]) == tid), None)
        art_path = self.art_lookup.get(int(spec["art_id"]), "") if spec else ""
        photo = self.app.get_photo(art_path, cs, cs) if art_path else None
        if photo and y < len(self._tile_item_ids) and x < len(self._tile_item_ids[y]):
            self.canvas.itemconfigure(self._tile_item_ids[y][x], image=photo)
        else:
            self.redraw_canvas()

    def _on_canvas_click(self, event) -> None:
        pos = self._event_to_tile(event)
        if not pos:
            return
        x, y = pos
        self._push_undo()
        if self.tool_mode.get() == "fill":
            changed = DesignerCore.flood_fill_grid(self.grid_data, x, y, self.selected_tile_id)
            if changed > 0:
                self.dirty = True
                self.redraw_canvas()
                self.app.set_status(f"Flood Fill 완료: {changed}개 타일 변경 (타일 {self.selected_tile_id:02d})")
        else:
            self._paint_cell(x, y, self.selected_tile_id)

    def _on_canvas_drag(self, event) -> None:
        if self.tool_mode.get() != "brush":
            return
        pos = self._event_to_tile(event)
        if pos:
            self._paint_cell(pos[0], pos[1], self.selected_tile_id)

    def _on_canvas_eyedrop(self, event) -> None:
        pos = self._event_to_tile(event)
        if pos:
            tid = self.grid_data[pos[1]][pos[0]]
            self.select_tile(tid)
            self.app.set_status(f"스포이드 선택: 타일 {tid:02d} at ({pos[0]}, {pos[1]})")

    def _on_canvas_hover(self, event) -> None:
        pos = self._event_to_tile(event)
        if pos:
            tid = self.grid_data[pos[1]][pos[0]]
            self.coord_label.configure(text=f"좌표: ({pos[0]}, {pos[1]}) | 타일: {tid:02d}")

    def undo(self) -> None:
        if not self.undo_stack:
            return
        self.grid_data = self.undo_stack.pop()
        self.redraw_canvas()
        self.app.set_status("이전 작업으로 되돌렸습니다.")

    def save_current_map(self) -> None:
        if not self.current_map_id or not self.grid_data:
            return
        try:
            res = self.core.save_map_grid(self.current_map_id, self.grid_data, self.current_comments)
            self.dirty = False
            self.app.set_status(f"맵 저장 완료: {res['txt_path']} ({res['width']}x{res['height']})")
            messagebox.showinfo("맵 저장", f"저장 완료!\n{res['txt_path']} ({res['width']}x{res['height']})")
        except Exception as e:
            messagebox.showerror("저장 오류", str(e))

    def open_new_map_dialog(self) -> None:
        dlg = tk.Toplevel(self)
        dlg.title("새 맵 생성")
        dlg.configure(bg=BG_PANEL)
        dlg.geometry("360x300")
        dlg.transient(self.app)
        dlg.grab_set()

        fields = [
            ("맵 ID (영소문자/숫자/_):", tk.StringVar(value="new_dungeon")),
            ("맵 표시 이름:", tk.StringVar(value="신규 던전")),
            ("맵 유형 (town/field/boss):", tk.StringVar(value="field")),
            ("너비 (타일 수):", tk.StringVar(value="24")),
            ("높이 (타일 수):", tk.StringVar(value="18")),
            ("내부 기본 타일 ID:", tk.StringVar(value="0")),
            ("외곽 테두리 타일 ID:", tk.StringVar(value="6")),
        ]
        for idx, (label, var) in enumerate(fields):
            ttk.Label(dlg, text=label).grid(row=idx, column=0, sticky="w", padx=12, pady=5)
            ttk.Entry(dlg, textvariable=var, width=18).grid(row=idx, column=1, padx=10, pady=5)

        def submit():
            try:
                mid = fields[0][1].get().strip()
                res = self.core.create_map(
                    map_id=mid,
                    name=fields[1][1].get().strip(),
                    map_type=fields[2][1].get().strip(),
                    width=int(fields[3][1].get()),
                    height=int(fields[4][1].get()),
                    fill_tile=int(fields[5][1].get()),
                    border_tile=int(fields[6][1].get()),
                )
                self.current_map_id = mid
                self.refresh_map_list()
                self.load_selected_map()
                dlg.destroy()
                self.app.set_status(f"새 맵 생성 완료: {mid} ({res['width']}x{res['height']})")
            except Exception as e:
                messagebox.showerror("새 맵 생성 오류", str(e), parent=dlg)

        ttk.Button(dlg, text="생성", style="Accent.TButton", command=submit).grid(row=len(fields), column=0, columnspan=2, pady=12)

    def open_resize_dialog(self) -> None:
        if not self.current_map_id:
            return
        cur_w = len(self.grid_data[0]) if self.grid_data else 24
        cur_h = len(self.grid_data)
        nw = simpledialog.askinteger("맵 크기 조절", "새 너비 (4~128):", initialvalue=cur_w, minvalue=4, maxvalue=128, parent=self)
        if not nw:
            return
        nh = simpledialog.askinteger("맵 크기 조절", "새 높이 (4~128):", initialvalue=cur_h, minvalue=4, maxvalue=128, parent=self)
        if not nh:
            return
        try:
            self.core.resize_map(self.current_map_id, nw, nh, fill_tile=self.selected_tile_id)
            self.load_selected_map()
            self.refresh_map_list()
        except Exception as e:
            messagebox.showerror("크기 조절 오류", str(e))

    def validate_maps(self) -> None:
        res = self.core.validate_maps()
        if res["ok"]:
            messagebox.showinfo("맵 검증", f"모든 맵({res['checked_maps']}개) 검증 통과! 이상 없음.")
        else:
            messagebox.showwarning("맵 검증 이슈", "\n".join(res["issues"]))


# ======================================================================
# TAB 2: ART RESOURCE UPLOAD & GALLERY (아트 리소스 업로드)
# ======================================================================
class ArtUploadTab(ttk.Frame):
    def __init__(self, parent, app: DesignerApp):
        super().__init__(parent)
        self.app = app
        self.core = app.core
        self.cat_filter = tk.StringVar(value="all")
        self.src_file_var = tk.StringVar()
        self.up_cat_var = tk.StringVar(value="items")
        self.up_id_var = tk.StringVar()
        self.up_name_var = tk.StringVar()
        self.up_resize_var = tk.StringVar()
        self.up_concept_var = tk.StringVar()
        self.up_req_var = tk.StringVar(value="(연결 안함)")
        self.up_overwrite_var = tk.BooleanVar(value=False)

        self._build_ui()
        self.refresh_all()

    def _build_ui(self) -> None:
        paned = ttk.PanedWindow(self, orient="horizontal")
        paned.pack(fill="both", expand=True, padx=8, pady=8)

        # Left: Mapped Art Gallery
        left = ttk.Frame(paned)
        paned.add(left, weight=3)

        top_l = ttk.Frame(left)
        top_l.pack(fill="x", pady=(0, 6))
        ttk.Label(top_l, text="등록된 아트 리소스 (data/art_mapping.txt)", style="Header.TLabel").pack(side="left")
        ttk.Combobox(
            top_l, state="readonly", width=12,
            values=["all"] + list(ART_CATEGORIES.keys()), textvariable=self.cat_filter
        ).pack(side="right", padx=4)
        self.cat_filter.trace_add("write", lambda *_: self.refresh_tree())
        ttk.Button(top_l, text="무결성 검증", command=self.verify_art).pack(side="right", padx=4)

        cols = ("art_id", "category", "size", "status", "path")
        self.tree = ttk.Treeview(left, columns=cols, show="headings", selectmode="browse")
        for col, title, w in [
            ("art_id", "Art ID", 65), ("category", "카테고리", 90),
            ("size", "규격", 80), ("status", "상태", 65), ("path", "상대 경로", 290)
        ]:
            self.tree.heading(col, text=title)
            self.tree.column(col, width=w, anchor="w" if col == "path" else "center")
        self.tree.pack(fill="both", expand=True)
        self.tree.bind("<<TreeviewSelect>>", self._on_select_existing)

        self.gallery_preview = tk.Label(left, bg="#12141c", fg=FG_MUTED, text="항목을 선택하면 이미지 미리보기가 표시됩니다.", height=9)
        self.gallery_preview.pack(fill="x", pady=(6, 0))

        # Right: Upload Form
        right = ttk.Frame(paned, padding=12)
        paned.add(right, weight=2)

        ttk.Label(right, text="신규 / 교체 아트 리소스 업로드", style="Header.TLabel").pack(anchor="w", pady=(0, 8))

        file_row = ttk.Frame(right)
        file_row.pack(fill="x", pady=4)
        ttk.Entry(file_row, textvariable=self.src_file_var).pack(side="left", fill="x", expand=True, padx=(0, 6))
        ttk.Button(file_row, text="이미지 선택...", command=self.browse_image).pack(side="right")

        self.upload_preview = tk.Label(right, bg="#12141c", fg=FG_MUTED, text="업로드할 이미지 미리보기", height=8)
        self.upload_preview.pack(fill="x", pady=6)

        form = ttk.Frame(right)
        form.pack(fill="x", pady=4)
        form.columnconfigure(1, weight=1)

        ttk.Label(form, text="카테고리:").grid(row=0, column=0, sticky="w", pady=4)
        cat_cb = ttk.Combobox(form, state="readonly", values=list(ART_CATEGORIES.keys()), textvariable=self.up_cat_var)
        cat_cb.grid(row=0, column=1, sticky="ew", padx=6, pady=4)
        cat_cb.bind("<<ComboboxSelected>>", lambda _e: self._suggest_id())

        ttk.Label(form, text="Art ID (비우면 자동):").grid(row=1, column=0, sticky="w", pady=4)
        id_row = ttk.Frame(form)
        id_row.grid(row=1, column=1, sticky="ew", padx=6, pady=4)
        ttk.Entry(id_row, textvariable=self.up_id_var, width=10).pack(side="left")
        ttk.Button(id_row, text="자동 추천", command=self._suggest_id).pack(side="left", padx=6)

        ttk.Label(form, text="저장 파일명 (확장자 생략 가능):").grid(row=2, column=0, sticky="w", pady=4)
        ttk.Entry(form, textvariable=self.up_name_var).grid(row=2, column=1, sticky="ew", padx=6, pady=4)

        ttk.Label(form, text="리사이즈 (선택, 예 32x32):").grid(row=3, column=0, sticky="w", pady=4)
        ttk.Entry(form, textvariable=self.up_resize_var).grid(row=3, column=1, sticky="ew", padx=6, pady=4)

        ttk.Label(form, text="아트 컨셉 설명:").grid(row=4, column=0, sticky="w", pady=4)
        ttk.Entry(form, textvariable=self.up_concept_var).grid(row=4, column=1, sticky="ew", padx=6, pady=4)

        ttk.Label(form, text="요구사항 완료 연결:").grid(row=5, column=0, sticky="w", pady=4)
        self.req_combo = ttk.Combobox(form, state="readonly", textvariable=self.up_req_var)
        self.req_combo.grid(row=5, column=1, sticky="ew", padx=6, pady=4)

        ttk.Checkbutton(right, text="기존 동일 Art ID 덮어쓰기 허용", variable=self.up_overwrite_var).pack(anchor="w", pady=6)
        ttk.Button(right, text="업로드 및 art_mapping.txt 등록", style="Accent.TButton", command=self.do_upload).pack(fill="x", pady=8)

    def refresh_all(self) -> None:
        self.refresh_tree()
        pending = self.core.list_art_requests(status="pending")
        vals = ["(연결 안함)"] + [f"{r['request_id']} — {r['title']} ({r['category']})" for r in pending]
        self.req_combo["values"] = vals
        if self.up_req_var.get() not in vals:
            self.up_req_var.set("(연결 안함)")
        if not self.up_id_var.get():
            self._suggest_id()

    def refresh_tree(self) -> None:
        for row in self.tree.get_children():
            self.tree.delete(row)
        fcat = self.cat_filter.get()
        for e in self.core.list_art_mappings():
            if fcat != "all" and e["category"] != fcat:
                continue
            self.tree.insert("", "end", values=(
                e["art_id"], e["category"], e["size"] or "-",
                "OK" if e["exists"] else "MISSING", e["path"]
            ))

    def _suggest_id(self) -> None:
        try:
            nid = self.core.suggest_next_art_id(self.up_cat_var.get())
            self.up_id_var.set(str(nid))
        except Exception:
            pass

    def _on_select_existing(self, _event=None) -> None:
        sel = self.tree.selection()
        if not sel:
            return
        vals = self.tree.item(sel[0], "values")
        rel_path = vals[4]
        photo = self.app.get_photo(rel_path, 128, 128, force_reload=True)
        if photo:
            self.gallery_preview.configure(image=photo, text="")
            self.gallery_preview.image = photo
        else:
            self.gallery_preview.configure(image="", text=f"파일 없음: {rel_path}")

    def browse_image(self) -> None:
        path = filedialog.askopenfilename(
            title="업로드할 아트 이미지 선택",
            filetypes=[("Image files", "*.png *.jpg *.jpeg *.bmp *.webp"), ("All files", "*.*")]
        )
        if not path:
            return
        self.src_file_var.set(path)
        if not self.up_name_var.get():
            self.up_name_var.set(Path(path).stem)
        photo = self.app.get_photo(path, 128, 128, force_reload=True)
        if photo:
            self.upload_preview.configure(image=photo, text="")
            self.upload_preview.image = photo

    def do_upload(self) -> None:
        src = self.src_file_var.get().strip()
        if not src:
            messagebox.showwarning("입력 필요", "업로드할 이미지 파일을 선택해 주세요.")
            return
        try:
            art_id = int(self.up_id_var.get().strip()) if self.up_id_var.get().strip() else None
            req_val = self.up_req_var.get()
            fulfill_id = req_val.split(" — ")[0] if req_val and req_val.startswith("REQ-") else None
            res = self.core.upload_art_resource(
                source_file=src,
                category=self.up_cat_var.get(),
                art_id=art_id,
                target_name=self.up_name_var.get().strip() or None,
                resize=self.up_resize_var.get().strip() or None,
                concept=self.up_concept_var.get().strip(),
                fulfill_request_id=fulfill_id,
                overwrite=self.up_overwrite_var.get(),
            )
            self.refresh_all()
            self.app.tab_map.refresh_tile_palette()
            self.app.set_status(f"아트 업로드 완료: Art ID {res['art_id']} -> {res['path']} ({res['size']})")
            messagebox.showinfo("업로드 성공", f"Art ID: {res['art_id']}\n경로: {res['path']}\n규격: {res['size']}")
            self.up_name_var.set("")
            self.up_concept_var.set("")
            self._suggest_id()
        except Exception as e:
            messagebox.showerror("업로드 오류", str(e))

    def verify_art(self) -> None:
        res = self.core.verify_art_resources()
        if res["ok"]:
            messagebox.showinfo("아트 무결성 검증", f"총 {res['total']}개 매핑 리소스 모두 정상 존재!")
        else:
            lines = [f"Art ID {m['art_id']}: {m['path']}" for m in res["missing"]]
            messagebox.showwarning("누락된 아트 파일", "\n".join(lines))


# ======================================================================
# TAB 3: ART RESOURCE REQUIREMENTS QUEUE (아트리소스 요구사항 기록)
# ======================================================================
class ArtRequestTab(ttk.Frame):
    def __init__(self, parent, app: DesignerApp):
        super().__init__(parent)
        self.app = app
        self.core = app.core
        self.status_filter = tk.StringVar(value="pending")
        self.search_var = tk.StringVar()
        self.selected_req_id: Optional[str] = None

        self.f_title = tk.StringVar()
        self.f_cat = tk.StringVar(value="monsters")
        self.f_size = tk.StringVar(value="160x40")
        self.f_art_id = tk.StringVar()
        self.f_path = tk.StringVar()
        self.f_status = tk.StringVar(value="pending")

        self._build_ui()
        self.refresh_requests()

    def _build_ui(self) -> None:
        top = ttk.Frame(self, padding=(8, 6))
        top.pack(fill="x")
        ttk.Label(top, text="상태 필터:").pack(side="left")
        for val, label in [("all", "전체"), ("pending", "요구됨 (pending)"), ("fulfilled", "완료 (fulfilled)"), ("cancelled", "취소")]:
            ttk.Radiobutton(top, text=label, variable=self.status_filter, value=val, command=self.refresh_requests).pack(side="left", padx=4)

        ttk.Label(top, text="  검색:").pack(side="left", padx=(12, 4))
        s_ent = ttk.Entry(top, textvariable=self.search_var, width=24)
        s_ent.pack(side="left")
        self.search_var.trace_add("write", lambda *_: self.refresh_requests())

        ttk.Button(top, text="LLM용 Pending JSON 미리보기", command=self.show_pending_json).pack(side="right", padx=4)

        paned = ttk.PanedWindow(self, orient="horizontal")
        paned.pack(fill="both", expand=True, padx=8, pady=(0, 8))

        # Left Table
        left = ttk.Frame(paned)
        paned.add(left, weight=3)
        cols = ("req_id", "status", "category", "title", "art_id", "size")
        self.tree = ttk.Treeview(left, columns=cols, show="headings", selectmode="browse")
        for col, title, w in [
            ("req_id", "요청 ID", 80), ("status", "상태", 80), ("category", "카테고리", 85),
            ("title", "제목", 180), ("art_id", "권장ID", 65), ("size", "규격", 80)
        ]:
            self.tree.heading(col, text=title)
            self.tree.column(col, width=w, anchor="w" if col == "title" else "center")
        self.tree.pack(fill="both", expand=True)
        self.tree.bind("<<TreeviewSelect>>", self._on_select)

        # Right Form
        right = ttk.Frame(paned, padding=12)
        paned.add(right, weight=2)
        ttk.Label(right, text="아트 리소스 요구사항 등록 / 편집", style="Header.TLabel").pack(anchor="w", pady=(0, 8))
        ttk.Label(
            right,
            text="※ 'pending' 상태로 저장된 항목은 차후 LLM이 CLI로 조회해 이미지 생성 모델을 호출할 수 있습니다.",
            style="Muted.TLabel", wraplength=420
        ).pack(anchor="w", pady=(0, 8))

        form = ttk.Frame(right)
        form.pack(fill="x")
        form.columnconfigure(1, weight=1)

        ttk.Label(form, text="제목:").grid(row=0, column=0, sticky="w", pady=4)
        ttk.Entry(form, textvariable=self.f_title).grid(row=0, column=1, sticky="ew", padx=6, pady=4)

        ttk.Label(form, text="카테고리:").grid(row=1, column=0, sticky="w", pady=4)
        cat_cb = ttk.Combobox(form, state="readonly", values=list(ART_CATEGORIES.keys()), textvariable=self.f_cat)
        cat_cb.grid(row=1, column=1, sticky="ew", padx=6, pady=4)
        cat_cb.bind("<<ComboboxSelected>>", self._on_cat_change)

        ttk.Label(form, text="권장 규격:").grid(row=2, column=0, sticky="w", pady=4)
        ttk.Entry(form, textvariable=self.f_size).grid(row=2, column=1, sticky="ew", padx=6, pady=4)

        ttk.Label(form, text="권장 Art ID (선택):").grid(row=3, column=0, sticky="w", pady=4)
        ttk.Entry(form, textvariable=self.f_art_id).grid(row=3, column=1, sticky="ew", padx=6, pady=4)

        ttk.Label(form, text="예정 경로 (선택):").grid(row=4, column=0, sticky="w", pady=4)
        ttk.Entry(form, textvariable=self.f_path).grid(row=4, column=1, sticky="ew", padx=6, pady=4)

        ttk.Label(form, text="상태:").grid(row=5, column=0, sticky="w", pady=4)
        ttk.Combobox(form, state="readonly", values=["pending", "fulfilled", "cancelled"], textvariable=self.f_status).grid(row=5, column=1, sticky="ew", padx=6, pady=4)

        ttk.Label(right, text="필요한 아트 리소스 설명 (이미지 생성용 짧은 글):").pack(anchor="w", pady=(8, 3))
        self.prompt_text = tk.Text(right, height=6, bg=BG_INPUT, fg=FG_TEXT, insertbackground=FG_TEXT, font=("Malgun Gothic", 10))
        self.prompt_text.pack(fill="both", expand=True, pady=(0, 8))

        btns = ttk.Frame(right)
        btns.pack(fill="x")
        ttk.Button(btns, text="신규 등록 (pending)", style="Accent.TButton", command=self.add_new).pack(side="left", padx=3)
        ttk.Button(btns, text="선택 항목 수정 저장", command=self.save_edit).pack(side="left", padx=3)
        ttk.Button(btns, text="입력 초기화", command=self.clear_form).pack(side="left", padx=3)
        ttk.Button(btns, text="삭제", command=self.delete_selected).pack(side="right", padx=3)

    def _on_cat_change(self, _e=None) -> None:
        cat = self.f_cat.get()
        if cat in ART_CATEGORIES:
            self.f_size.set(ART_CATEGORIES[cat][3])

    def refresh_requests(self) -> None:
        for row in self.tree.get_children():
            self.tree.delete(row)
        q = self.search_var.get().strip().lower()
        reqs = self.core.list_art_requests(status=self.status_filter.get())
        for r in reqs:
            if q and q not in json.dumps(r, ensure_ascii=False).lower():
                continue
            self.tree.insert("", "end", iid=r["request_id"], values=(
                r["request_id"], r["status"], r["category"], r["title"],
                r.get("suggested_art_id", ""), r.get("size", "")
            ))

    def _on_select(self, _e=None) -> None:
        sel = self.tree.selection()
        if not sel:
            return
        rid = sel[0]
        rec = next((r for r in self.core.list_art_requests("all") if r["request_id"] == rid), None)
        if not rec:
            return
        self.selected_req_id = rid
        self.f_title.set(rec.get("title", ""))
        self.f_cat.set(rec.get("category", "items"))
        self.f_size.set(rec.get("size", ""))
        self.f_art_id.set("" if rec.get("suggested_art_id") is None else str(rec["suggested_art_id"]))
        self.f_path.set(rec.get("target_path", ""))
        self.f_status.set(rec.get("status", "pending"))
        self.prompt_text.delete("1.0", "end")
        self.prompt_text.insert("1.0", rec.get("prompt", ""))

    def clear_form(self) -> None:
        self.selected_req_id = None
        self.f_title.set("")
        self.f_art_id.set("")
        self.f_path.set("")
        self.f_status.set("pending")
        self.prompt_text.delete("1.0", "end")

    def add_new(self) -> None:
        try:
            sid = int(self.f_art_id.get().strip()) if self.f_art_id.get().strip() else None
            rec = self.core.add_art_request(
                title=self.f_title.get(),
                prompt=self.prompt_text.get("1.0", "end").strip(),
                category=self.f_cat.get(),
                size=self.f_size.get().strip() or None,
                suggested_art_id=sid,
                target_path=self.f_path.get().strip(),
            )
            self.refresh_requests()
            self.clear_form()
            self.app.set_status(f"아트 요구사항 등록 완료: {rec['request_id']} ({rec['title']})")
        except Exception as e:
            messagebox.showerror("등록 오류", str(e))

    def save_edit(self) -> None:
        if not self.selected_req_id:
            messagebox.showwarning("선택 필요", "수정할 요구사항 항목을 왼쪽 목록에서 선택하세요.")
            return
        try:
            sid = int(self.f_art_id.get().strip()) if self.f_art_id.get().strip() else None
            self.core.update_art_request(self.selected_req_id, {
                "title": self.f_title.get().strip(),
                "category": self.f_cat.get(),
                "size": self.f_size.get().strip(),
                "suggested_art_id": sid,
                "target_path": self.f_path.get().strip(),
                "status": self.f_status.get(),
                "prompt": self.prompt_text.get("1.0", "end").strip(),
            })
            self.refresh_requests()
            self.app.set_status(f"요구사항 수정 완료: {self.selected_req_id}")
        except Exception as e:
            messagebox.showerror("수정 오류", str(e))

    def delete_selected(self) -> None:
        if not self.selected_req_id:
            return
        if not messagebox.askyesno("삭제 확인", f"{self.selected_req_id} 요구사항을 삭제할까요?"):
            return
        self.core.delete_art_request(self.selected_req_id)
        self.clear_form()
        self.refresh_requests()

    def show_pending_json(self) -> None:
        pending = self.core.list_art_requests("pending")
        win = tk.Toplevel(self)
        win.title(f"LLM 이미지 생성용 대기 큐 (pending: {len(pending)}건)")
        win.geometry("680x460")
        txt = tk.Text(win, bg=BG_INPUT, fg=FG_TEXT, font=("Consolas", 10))
        txt.pack(fill="both", expand=True)
        txt.insert("1.0", json.dumps(pending, ensure_ascii=False, indent=2))
        txt.configure(state="disabled")


# ======================================================================
# TAB 4: DESIGN DOCUMENT EDITOR (기획 문서 JSON 편집기 + 검색)
# ======================================================================
class DesignDocTab(ttk.Frame):
    def __init__(self, parent, app: DesignerApp):
        super().__init__(parent)
        self.app = app
        self.core = app.core

        self.doc_var = tk.StringVar(value="items")
        self.search_var = tk.StringVar()
        self.global_search_var = tk.BooleanVar(value=False)

        self.current_entries: List[Tuple[str, str, str, Any]] = []  # (doc_key, rec_id, label, record)
        self.selected_doc_key: str = "items"
        self.selected_rec_id: Optional[str] = None
        self.field_widgets: Dict[str, Tuple[str, Any, Any]] = {}  # field -> (kind, widget/var, original_value)

        self._build_ui()
        self.refresh_records()

    def _build_ui(self) -> None:
        top = ttk.Frame(self, padding=(8, 6))
        top.pack(fill="x")

        ttk.Label(top, text="기획 문서(JSON):").pack(side="left", padx=(0, 4))
        doc_keys = list(DESIGN_DOCS.keys())
        self.doc_combo = ttk.Combobox(top, state="readonly", width=18, values=doc_keys, textvariable=self.doc_var)
        self.doc_combo.pack(side="left", padx=(0, 10))
        self.doc_combo.bind("<<ComboboxSelected>>", lambda _e: self.refresh_records())

        ttk.Label(top, text="검색:").pack(side="left", padx=(4, 4))
        search_entry = ttk.Entry(top, textvariable=self.search_var, width=28)
        search_entry.pack(side="left", padx=(0, 6))
        self.search_var.trace_add("write", lambda *_: self.refresh_records())

        ttk.Checkbutton(top, text="전체 JSON 통합 검색", variable=self.global_search_var, command=self.refresh_records).pack(side="left", padx=6)
        ttk.Button(top, text="검색 초기화", command=lambda: self.search_var.set("")).pack(side="left", padx=4)

        ttk.Button(top, text="전체 데이터 무결성 검증", command=self.validate_all).pack(side="right", padx=4)

        paned = ttk.PanedWindow(self, orient="horizontal")
        paned.pack(fill="both", expand=True, padx=8, pady=(0, 8))

        # Left: Filtered Record List
        left = ttk.Frame(paned, width=360)
        paned.add(left, weight=1)

        btn_row = ttk.Frame(left)
        btn_row.pack(fill="x", pady=(0, 6))
        ttk.Button(btn_row, text="+ 신규 추가", command=self.add_new_record).pack(side="left", padx=2)
        ttk.Button(btn_row, text="복제", command=self.duplicate_selected).pack(side="left", padx=2)
        ttk.Button(btn_row, text="삭제", command=self.delete_selected).pack(side="right", padx=2)

        self.record_list = tk.Listbox(
            left, bg=BG_INPUT, fg=FG_TEXT, selectbackground="#455a8c", selectforeground="#ffffff",
            font=("Malgun Gothic", 10), activestyle="none"
        )
        sb = ttk.Scrollbar(left, orient="vertical", command=self.record_list.yview)
        self.record_list.configure(yscrollcommand=sb.set)
        self.record_list.pack(side="left", fill="both", expand=True)
        sb.pack(side="right", fill="y")
        self.record_list.bind("<<ListboxSelect>>", self._on_select_record)

        # Right: Dynamic JSON Form Editor
        right = ttk.Frame(paned)
        paned.add(right, weight=2)

        hdr = ttk.Frame(right, padding=(6, 4))
        hdr.pack(fill="x")
        self.form_header = ttk.Label(hdr, text="왼쪽에서 편집할 항목을 선택하세요.", style="Header.TLabel")
        self.form_header.pack(side="left")
        ttk.Button(hdr, text="변경사항 저장 (JSON)", style="Accent.TButton", command=self.save_form).pack(side="right")

        form_canvas = tk.Canvas(right, bg=BG_PANEL, highlightthickness=0)
        f_scroll = ttk.Scrollbar(right, orient="vertical", command=form_canvas.yview)
        self.form_inner = ttk.Frame(form_canvas, padding=10)
        self.form_inner.bind("<Configure>", lambda _e: form_canvas.configure(scrollregion=form_canvas.bbox("all")))
        form_canvas.create_window((0, 0), window=self.form_inner, anchor="nw")
        form_canvas.configure(yscrollcommand=f_scroll.set)
        form_canvas.pack(side="left", fill="both", expand=True)
        f_scroll.pack(side="right", fill="y")

    def refresh_records(self) -> None:
        self.record_list.delete(0, "end")
        self.current_entries.clear()
        q = self.search_var.get().strip()
        target_doc = None if self.global_search_var.get() else self.doc_var.get()
        hits = self.core.search_design_docs(q, doc_key=target_doc)

        for h in hits:
            prefix = f"[{h['doc']}] " if self.global_search_var.get() else ""
            disp = f"{prefix}{h['label']}"
            self.current_entries.append((h["doc"], h["record_id"], disp, h["record"]))
            self.record_list.insert("end", disp)

        self.app.set_status(f"기획 문서 검색 결과: {len(hits)}건 (문서: {target_doc or '전체'})")
        if self.current_entries:
            self.record_list.selection_set(0)
            self._load_entry_into_form(0)

    def _on_select_record(self, _e=None) -> None:
        sel = self.record_list.curselection()
        if not sel:
            return
        self._load_entry_into_form(sel[0])

    def _load_entry_into_form(self, idx: int) -> None:
        if idx < 0 or idx >= len(self.current_entries):
            return
        doc_key, rec_id, _disp, rec = self.current_entries[idx]
        self.selected_doc_key = doc_key
        self.selected_rec_id = rec_id
        self.form_header.configure(text=f"편집 중: {DESIGN_DOCS[doc_key]['file']}  →  {rec_id}")

        for child in self.form_inner.winfo_children():
            child.destroy()
        self.field_widgets.clear()

        if not isinstance(rec, dict):
            return

        self.form_inner.columnconfigure(1, weight=1)
        for row_i, (k, v) in enumerate(rec.items()):
            ttk.Label(self.form_inner, text=f"{k}:", width=22, anchor="e").grid(row=row_i, column=0, sticky="ne", padx=(4, 10), pady=5)
            if isinstance(v, bool):
                var = tk.BooleanVar(value=v)
                chk = ttk.Checkbutton(self.form_inner, variable=var)
                chk.grid(row=row_i, column=1, sticky="w", pady=5)
                self.field_widgets[k] = ("bool", var, v)
            elif isinstance(v, (dict, list)) or (isinstance(v, str) and ("\n" in v or len(v) > 55)):
                h = 5 if isinstance(v, (dict, list)) else 3
                txt = tk.Text(self.form_inner, height=h, width=62, bg=BG_INPUT, fg=FG_TEXT, insertbackground=FG_TEXT, font=("Consolas" if isinstance(v, (dict, list)) else "Malgun Gothic", 10))
                txt.grid(row=row_i, column=1, sticky="ew", pady=4)
                content = json.dumps(v, ensure_ascii=False, indent=2) if isinstance(v, (dict, list)) else v
                txt.insert("1.0", content)
                kind = "json" if isinstance(v, (dict, list)) else "text"
                self.field_widgets[k] = (kind, txt, v)
            else:
                var = tk.StringVar(value="" if v is None else str(v))
                ent = ttk.Entry(self.form_inner, textvariable=var, width=54)
                ent.grid(row=row_i, column=1, sticky="ew", pady=4)
                self.field_widgets[k] = ("scalar", var, v)

    def save_form(self) -> None:
        if not self.selected_rec_id:
            return
        updates: Dict[str, Any] = {}
        try:
            for k, (kind, widget, orig) in self.field_widgets.items():
                if kind == "bool":
                    updates[k] = bool(widget.get())
                elif kind == "json":
                    raw_s = widget.get("1.0", "end").strip()
                    updates[k] = json.loads(raw_s)
                elif kind == "text":
                    updates[k] = widget.get("1.0", "end").rstrip("\n")
                else:
                    s = widget.get()
                    if orig is None and (s == "" or s.lower() == "null"):
                        updates[k] = None
                    elif orig is None:
                        updates[k] = json.loads(s)
                    elif isinstance(orig, int) and not isinstance(orig, bool):
                        updates[k] = int(s)
                    elif isinstance(orig, float):
                        updates[k] = float(s)
                    else:
                        updates[k] = s

            res = self.core.update_doc_record(self.selected_doc_key, self.selected_rec_id, updates, coerce=False)
            self.app.set_status(f"JSON 저장 완료: {self.selected_doc_key} / {res['record_id']}")
            self.refresh_records()
            messagebox.showinfo("저장 완료", f"{DESIGN_DOCS[self.selected_doc_key]['file']} 저장이 완료되었습니다.")
        except Exception as e:
            messagebox.showerror("저장 오류", f"필드 형식을 확인하세요:\n{e}")

    def add_new_record(self) -> None:
        dk = self.doc_var.get()
        if DESIGN_DOCS[dk]["mode"] == "object":
            messagebox.showinfo("안내", "player.json은 단일 스펙 문서입니다. 오른쪽 폼에서 직접 편집 후 저장하세요.")
            return
        try:
            tpl = self.core.make_record_template(dk)
            res = self.core.add_doc_record(dk, tpl)
            self.refresh_records()
            self.app.set_status(f"신규 항목 추가됨: {dk} / {res['record_id']}")
        except Exception as e:
            messagebox.showerror("추가 오류", str(e))

    def duplicate_selected(self) -> None:
        if not self.selected_rec_id or DESIGN_DOCS[self.selected_doc_key]["mode"] == "object":
            return
        try:
            orig = copy.deepcopy(self.core.get_doc_record(self.selected_doc_key, self.selected_rec_id))
            if self.selected_doc_key == "strings":
                orig["key"] = f"{orig['key']}_copy"
            elif "id" in orig:
                if isinstance(orig["id"], int):
                    used = [int(r[2]["id"]) for r in self.core.get_doc_records(self.selected_doc_key)]
                    orig["id"] = max(used) + 1
                else:
                    orig["id"] = f"{orig['id']}_copy"
            res = self.core.add_doc_record(self.selected_doc_key, orig)
            self.refresh_records()
            self.app.set_status(f"복제 완료: {self.selected_doc_key} / {res['record_id']}")
        except Exception as e:
            messagebox.showerror("복제 오류", str(e))

    def delete_selected(self) -> None:
        if not self.selected_rec_id or DESIGN_DOCS[self.selected_doc_key]["mode"] == "object":
            return
        if not messagebox.askyesno("삭제 확인", f"[{self.selected_doc_key}] '{self.selected_rec_id}' 항목을 삭제할까요?"):
            return
        try:
            self.core.delete_doc_record(self.selected_doc_key, self.selected_rec_id)
            self.selected_rec_id = None
            self.refresh_records()
            self.app.set_status("항목이 삭제되었습니다.")
        except Exception as e:
            messagebox.showerror("삭제 오류", str(e))

    def validate_all(self) -> None:
        res = self.core.validate_all_docs()
        if res["ok"]:
            messagebox.showinfo("전체 무결성 검증", "모든 기획 JSON, 타일맵, 아트 매핑, UI 레이아웃 교차 참조 검증 통과!")
        else:
            messagebox.showwarning("검증 이슈 발견", "\n".join(res["issues"]))


# ======================================================================
# SHARED RGBA / CANVAS HELPERS FOR TABS 5 & 6
# ======================================================================
def rgba_to_hex(rgba: Optional[List[int]], fallback: str = "#222638") -> str:
    if not rgba or len(rgba) < 3:
        return fallback
    return f"#{int(rgba[0]):02x}{int(rgba[1]):02x}{int(rgba[2]):02x}"


def rgba_to_str(rgba: Optional[List[int]]) -> str:
    if not rgba:
        return "255,255,255,255"
    return ",".join(str(int(c)) for c in rgba)


# ======================================================================
# TAB 5: LOBBY SCREEN EDITOR (로비 화면 편집 — WYSIWYG 드래그/리사이즈)
# ======================================================================
class LobbyEditorTab(ttk.Frame):
    SCALE = 0.62  # 1280x720 -> 794x446

    def __init__(self, parent, app: DesignerApp):
        super().__init__(parent)
        self.app = app
        self.core = app.core

        self.layout_data: Dict[str, Any] = {}
        self.strings_map: Dict[str, str] = {}
        self.art_lookup: Dict[int, str] = {}
        self.selected_id: Optional[str] = None
        self.snap_grid = tk.BooleanVar(value=True)

        self._drag_mode: Optional[str] = None  # "move" | "resize"
        self._drag_start_mouse: Tuple[int, int] = (0, 0)
        self._drag_start_rect: Tuple[int, int, int, int] = (0, 0, 0, 0)

        self.v_bg_top = tk.StringVar()
        self.v_bg_bot = tk.StringVar()
        self.v_label = tk.StringVar()
        self.v_visible = tk.BooleanVar(value=True)
        self.v_x = tk.StringVar()
        self.v_y = tk.StringVar()
        self.v_w = tk.StringVar()
        self.v_h = tk.StringVar()
        self.v_font = tk.StringVar()
        self.v_art_id = tk.StringVar()
        self.v_text_key = tk.StringVar()
        self.v_text = tk.StringVar()
        self.v_align = tk.StringVar(value="center")
        self.v_color = tk.StringVar()
        self.v_bg_color = tk.StringVar()
        self.v_border_color = tk.StringVar()

        self._build_ui()
        self.reload_layout()

    def _build_ui(self) -> None:
        bar = ttk.Frame(self, padding=(8, 6))
        bar.pack(fill="x")
        ttk.Label(bar, text="로비(타이틀) 화면 WYSIWYG 편집기 (1280×720 기준)", style="Header.TLabel").pack(side="left")
        ttk.Checkbutton(bar, text="8px 격자 스냅", variable=self.snap_grid).pack(side="left", padx=12)
        ttk.Button(bar, text="+ 텍스트 추가", command=lambda: self.add_element("text")).pack(side="left", padx=3)
        ttk.Button(bar, text="+ 스프라이트 추가", command=lambda: self.add_element("sprite")).pack(side="left", padx=3)
        ttk.Button(bar, text="+ 패널 추가", command=lambda: self.add_element("panel")).pack(side="left", padx=3)
        ttk.Button(bar, text="기본값 복원", command=self.reset_layout).pack(side="right", padx=4)
        ttk.Button(bar, text="로비 레이아웃 저장", style="Accent.TButton", command=self.save_layout).pack(side="right", padx=6)

        paned = ttk.PanedWindow(self, orient="horizontal")
        paned.pack(fill="both", expand=True, padx=8, pady=(0, 8))

        # Left: WYSIWYG Canvas + Element list
        left = ttk.Frame(paned)
        paned.add(left, weight=3)

        cw, ch = int(1280 * self.SCALE), int(720 * self.SCALE)
        self.canvas = tk.Canvas(left, width=cw, height=ch, bg="#121622", highlightthickness=1, highlightbackground=ACCENT)
        self.canvas.pack(padx=6, pady=6)
        self.canvas.bind("<Button-1>", self._on_mouse_down)
        self.canvas.bind("<B1-Motion>", self._on_mouse_drag)
        self.canvas.bind("<ButtonRelease-1>", self._on_mouse_up)

        ttk.Label(left, text="※ 캔버스에서 요소를 클릭·드래그해 이동하고, 우하단 노란 핸들을 드래그해 크기를 조절하세요.", style="Muted.TLabel").pack(anchor="w", padx=8)

        self.elem_list = tk.Listbox(left, height=6, bg=BG_INPUT, fg=FG_TEXT, selectbackground="#455a8c", font=("Malgun Gothic", 9))
        self.elem_list.pack(fill="x", padx=6, pady=(6, 2))
        self.elem_list.bind("<<ListboxSelect>>", self._on_list_select)

        # Right: Inspector
        right = ttk.Frame(paned, padding=10)
        paned.add(right, weight=2)

        ttk.Label(right, text="배경 그라데이션 색상", style="Header.TLabel").pack(anchor="w", pady=(0, 4))
        bg_row = ttk.Frame(right)
        bg_row.pack(fill="x", pady=(0, 10))
        ttk.Label(bg_row, text="상단:").pack(side="left")
        ttk.Entry(bg_row, textvariable=self.v_bg_top, width=14).pack(side="left", padx=3)
        ttk.Button(bg_row, text="색상", command=lambda: self._pick_color(self.v_bg_top, True)).pack(side="left", padx=2)
        ttk.Label(bg_row, text="하단:").pack(side="left", padx=(8, 0))
        ttk.Entry(bg_row, textvariable=self.v_bg_bot, width=14).pack(side="left", padx=3)
        ttk.Button(bg_row, text="색상", command=lambda: self._pick_color(self.v_bg_bot, True)).pack(side="left", padx=2)

        ttk.Separator(right, orient="horizontal").pack(fill="x", pady=6)
        self.sel_header = ttk.Label(right, text="선택된 요소 속성", style="Header.TLabel")
        self.sel_header.pack(anchor="w", pady=(0, 6))

        form = ttk.Frame(right)
        form.pack(fill="x")
        form.columnconfigure(1, weight=1)

        rows = [
            ("라벨:", self.v_label),
            ("X / Y:", None),
            ("너비 / 높이:", None),
            ("폰트 크기:", self.v_font),
            ("Art ID (스프라이트):", self.v_art_id),
            ("strings.json 키:", self.v_text_key),
            ("직접 텍스트(우선):", self.v_text),
            ("정렬(left/center/right):", self.v_align),
            ("글자/틴트 RGBA:", self.v_color),
            ("배경 RGBA:", self.v_bg_color),
            ("테두리 RGBA:", self.v_border_color),
        ]
        for i, (lbl, var) in enumerate(rows):
            ttk.Label(form, text=lbl).grid(row=i, column=0, sticky="w", pady=3)
            if lbl == "X / Y:":
                sub = ttk.Frame(form)
                sub.grid(row=i, column=1, sticky="ew", padx=4, pady=3)
                ttk.Entry(sub, textvariable=self.v_x, width=8).pack(side="left")
                ttk.Entry(sub, textvariable=self.v_y, width=8).pack(side="left", padx=6)
                ttk.Checkbutton(sub, text="표시(Visible)", variable=self.v_visible).pack(side="left", padx=6)
            elif lbl == "너비 / 높이:":
                sub = ttk.Frame(form)
                sub.grid(row=i, column=1, sticky="ew", padx=4, pady=3)
                ttk.Entry(sub, textvariable=self.v_w, width=8).pack(side="left")
                ttk.Entry(sub, textvariable=self.v_h, width=8).pack(side="left", padx=6)
            elif "RGBA" in lbl:
                sub = ttk.Frame(form)
                sub.grid(row=i, column=1, sticky="ew", padx=4, pady=3)
                ttk.Entry(sub, textvariable=var).pack(side="left", fill="x", expand=True)
                ttk.Button(sub, text="선택", command=lambda _v=var: self._pick_color(_v, False)).pack(side="right", padx=3)
            else:
                ttk.Entry(form, textvariable=var).grid(row=i, column=1, sticky="ew", padx=4, pady=3)

        btn_f = ttk.Frame(right)
        btn_f.pack(fill="x", pady=10)
        ttk.Button(btn_f, text="속성 적용 (미리보기)", command=self.apply_inspector_to_selected).pack(side="left", padx=3)
        ttk.Button(btn_f, text="선택 요소 삭제", command=self.delete_selected).pack(side="right", padx=3)

    def reload_layout(self) -> None:
        self.layout_data = self.core.load_ui_layout()
        self.strings_map = self.core.get_string_map()
        self.art_lookup = {e["art_id"]: e["path"] for e in self.core.list_art_mappings()}
        lob = self.layout_data.get("lobby", {})
        self.v_bg_top.set(rgba_to_str(lob.get("bg_top_color", [18, 22, 34, 255])))
        self.v_bg_bot.set(rgba_to_str(lob.get("bg_bottom_color", [38, 20, 26, 255])))
        self._refresh_list()
        self.redraw_canvas()

    def _elements(self) -> List[Dict[str, Any]]:
        return self.layout_data.setdefault("lobby", {}).setdefault("elements", [])

    def _get_selected(self) -> Optional[Dict[str, Any]]:
        return next((e for e in self._elements() if e.get("id") == self.selected_id), None)

    def _refresh_list(self) -> None:
        self.elem_list.delete(0, "end")
        elems = self._elements()
        for e in elems:
            vis = "ON" if e.get("visible", True) else "OFF"
            self.elem_list.insert("end", f"[{vis}] {e['id']} ({e.get('type')}) — {e.get('label', '')}  [{e['x']},{e['y']} {e['width']}x{e['height']}]")
        if elems and (self.selected_id is None or not any(e["id"] == self.selected_id for e in elems)):
            self.selected_id = elems[0]["id"]
        self._load_selected_to_inspector()

    def _on_list_select(self, _e=None) -> None:
        sel = self.elem_list.curselection()
        if not sel:
            return
        elems = self._elements()
        if 0 <= sel[0] < len(elems):
            self.selected_id = elems[sel[0]]["id"]
            self._load_selected_to_inspector()
            self.redraw_canvas()

    def _load_selected_to_inspector(self) -> None:
        e = self._get_selected()
        if not e:
            return
        self.sel_header.configure(text=f"선택 요소: {e['id']} ({e.get('type')})")
        self.v_label.set(e.get("label", ""))
        self.v_visible.set(bool(e.get("visible", True)))
        self.v_x.set(str(e.get("x", 0)))
        self.v_y.set(str(e.get("y", 0)))
        self.v_w.set(str(e.get("width", 100)))
        self.v_h.set(str(e.get("height", 40)))
        self.v_font.set(str(e.get("font_size", 18)))
        self.v_art_id.set(str(e.get("art_id", 0)))
        self.v_text_key.set(e.get("text_key", ""))
        self.v_text.set(e.get("text", ""))
        self.v_align.set(e.get("align", "center"))
        self.v_color.set(rgba_to_str(e.get("color", [255, 255, 255, 255])))
        self.v_bg_color.set(rgba_to_str(e.get("bg_color", [15, 18, 28, 210])))
        self.v_border_color.set(rgba_to_str(e.get("border_color", [160, 140, 95, 200])))

    def _pick_color(self, var: tk.StringVar, is_bg: bool) -> None:
        init_hex = "#ffffff"
        try:
            rgba = DesignerCore.parse_rgba(var.get())
            init_hex = rgba_to_hex(rgba)
            alpha = rgba[3]
        except Exception:
            alpha = 255
        chosen = colorchooser.askcolor(color=init_hex, parent=self)
        if chosen and chosen[0]:
            r, g, b = [int(round(c)) for c in chosen[0]]
            var.set(f"{r},{g},{b},{alpha}")
            if is_bg:
                self._apply_bg_vars()
                self.redraw_canvas()
            else:
                self.apply_inspector_to_selected()

    def _apply_bg_vars(self) -> None:
        lob = self.layout_data.setdefault("lobby", {})
        try:
            lob["bg_top_color"] = DesignerCore.parse_rgba(self.v_bg_top.get())
            lob["bg_bottom_color"] = DesignerCore.parse_rgba(self.v_bg_bot.get())
        except Exception:
            pass

    def apply_inspector_to_selected(self) -> None:
        self._apply_bg_vars()
        e = self._get_selected()
        if not e:
            return
        try:
            e["label"] = self.v_label.get().strip()
            e["visible"] = bool(self.v_visible.get())
            e["x"] = int(float(self.v_x.get()))
            e["y"] = int(float(self.v_y.get()))
            e["width"] = max(16, int(float(self.v_w.get())))
            e["height"] = max(16, int(float(self.v_h.get())))
            if self.v_font.get().strip():
                e["font_size"] = int(float(self.v_font.get()))
            if self.v_art_id.get().strip():
                e["art_id"] = int(self.v_art_id.get())
            e["text_key"] = self.v_text_key.get().strip()
            e["text"] = self.v_text.get()
            e["align"] = self.v_align.get().strip() or "center"
            e["color"] = DesignerCore.parse_rgba(self.v_color.get())
            e["bg_color"] = DesignerCore.parse_rgba(self.v_bg_color.get())
            e["border_color"] = DesignerCore.parse_rgba(self.v_border_color.get())
            self._refresh_list()
            self.redraw_canvas()
        except Exception as ex:
            messagebox.showerror("속성 오류", str(ex))

    def _resolve_text(self, e: Dict[str, Any]) -> str:
        if e.get("text"):
            return str(e["text"])
        tk_key = e.get("text_key", "")
        return self.strings_map.get(tk_key, tk_key)

    def redraw_canvas(self) -> None:
        self.canvas.delete("all")
        s = self.SCALE
        cw, ch = int(1280 * s), int(720 * s)
        lob = self.layout_data.get("lobby", {})
        c_top = lob.get("bg_top_color", [18, 22, 34, 255])
        c_bot = lob.get("bg_bottom_color", [38, 20, 26, 255])

        # Draw vertical gradient bands
        bands = 24
        for i in range(bands):
            t = i / max(1, bands - 1)
            r = int(c_top[0] + (c_bot[0] - c_top[0]) * t)
            g = int(c_top[1] + (c_bot[1] - c_top[1]) * t)
            b = int(c_top[2] + (c_bot[2] - c_top[2]) * t)
            y0 = int(ch * i / bands)
            y1 = int(ch * (i + 1) / bands) + 1
            self.canvas.create_rectangle(0, y0, cw, y1, fill=f"#{r:02x}{g:02x}{b:02x}", outline="")

        for e in self._elements():
            if not e.get("visible", True):
                continue
            x0, y0 = e["x"] * s, e["y"] * s
            x1, y1 = (e["x"] + e["width"]) * s, (e["y"] + e["height"]) * s
            etype = e.get("type", "text")

            if etype == "sprite":
                art_path = self.art_lookup.get(int(e.get("art_id", 0)), "")
                fw, fh = int(e.get("frame_w", 0)), int(e.get("frame_h", 0))
                crop = (fw, fh) if fw > 0 and fh > 0 else None
                photo = self.app.get_photo(art_path, max(12, int(x1 - x0)), max(12, int(y1 - y0)), crop_size=crop) if art_path else None
                if photo:
                    self.canvas.create_image(x0, y0, anchor="nw", image=photo)
                else:
                    self.canvas.create_rectangle(x0, y0, x1, y1, fill="#332244", outline="#aa66cc")
            elif etype == "panel":
                self.canvas.create_rectangle(x0, y0, x1, y1, fill=rgba_to_hex(e.get("bg_color")), outline=rgba_to_hex(e.get("border_color"), ACCENT), width=2)
            elif etype == "guide_card":
                self.canvas.create_rectangle(x0, y0, x1, y1, fill=rgba_to_hex(e.get("bg_color")), outline=rgba_to_hex(e.get("border_color"), ACCENT), width=2)
                lines = [self.strings_map.get(k, k) for k in e.get("text_keys", [])]
                preview_txt = "\n".join(lines)
                fs = max(7, int(e.get("font_size", 16) * s * 0.85))
                self.canvas.create_text(x0 + 12, y0 + 10, anchor="nw", text=preview_txt, fill=rgba_to_hex(e.get("color"), "#eeeeee"), font=("Malgun Gothic", fs))
            elif etype == "button":
                self.canvas.create_rectangle(x0, y0, x1, y1, fill=rgba_to_hex(e.get("bg_color"), "#8c302a"), outline=rgba_to_hex(e.get("border_color"), ACCENT), width=2)
                fs = max(8, int(e.get("font_size", 16) * s))
                self.canvas.create_text((x0 + x1) / 2, (y0 + y1) / 2, text=self._resolve_text(e), fill=rgba_to_hex(e.get("color"), "#ffffff"), font=("Malgun Gothic", fs, "bold"))
            elif etype == "text":
                fs = max(8, int(e.get("font_size", 20) * s * 0.85))
                align = e.get("align", "center")
                tx = (x0 + x1) / 2 if align == "center" else (x1 if align == "right" else x0)
                anc = "n" if align == "center" else ("ne" if align == "right" else "nw")
                self.canvas.create_text(tx, y0, anchor=anc, text=self._resolve_text(e), fill=rgba_to_hex(e.get("color"), "#ffd700"), font=("Malgun Gothic", fs, "bold"))

            # Selection box + resize handle
            if e.get("id") == self.selected_id:
                self.canvas.create_rectangle(x0, y0, x1, y1, outline="#00e5ff", width=2, dash=(4, 2))
                self.canvas.create_rectangle(x1 - 8, y1 - 8, x1 + 2, y1 + 2, fill="#ffd700", outline="#111111")

    def _on_mouse_down(self, event) -> None:
        s = self.SCALE
        # Check selected resize handle first
        cur = self._get_selected()
        if cur and cur.get("visible", True):
            rx, ry = (cur["x"] + cur["width"]) * s, (cur["y"] + cur["height"]) * s
            if abs(event.x - rx) <= 10 and abs(event.y - ry) <= 10:
                self._drag_mode = "resize"
                self._drag_start_mouse = (event.x, event.y)
                self._drag_start_rect = (cur["x"], cur["y"], cur["width"], cur["height"])
                return

        # Pick topmost element under cursor
        for e in reversed(self._elements()):
            if not e.get("visible", True):
                continue
            x0, y0 = e["x"] * s, e["y"] * s
            x1, y1 = (e["x"] + e["width"]) * s, (e["y"] + e["height"]) * s
            if x0 <= event.x <= x1 and y0 <= event.y <= y1:
                self.selected_id = e["id"]
                self._drag_mode = "move"
                self._drag_start_mouse = (event.x, event.y)
                self._drag_start_rect = (e["x"], e["y"], e["width"], e["height"])
                self._load_selected_to_inspector()
                self.redraw_canvas()
                return

    def _on_mouse_drag(self, event) -> None:
        if not self._drag_mode:
            return
        e = self._get_selected()
        if not e:
            return
        dx = int(round((event.x - self._drag_start_mouse[0]) / self.SCALE))
        dy = int(round((event.y - self._drag_start_mouse[1]) / self.SCALE))
        ox, oy, ow, oh = self._drag_start_rect
        snap = 8 if self.snap_grid.get() else 1

        if self._drag_mode == "move":
            e["x"] = round((ox + dx) / snap) * snap
            e["y"] = round((oy + dy) / snap) * snap
        elif self._drag_mode == "resize":
            e["width"] = max(24, round((ow + dx) / snap) * snap)
            e["height"] = max(20, round((oh + dy) / snap) * snap)

        self._load_selected_to_inspector()
        self.redraw_canvas()

    def _on_mouse_up(self, _event) -> None:
        if self._drag_mode:
            self._drag_mode = None
            self._refresh_list()

    def add_element(self, elem_type: str) -> None:
        eid = simpledialog.askstring("요소 추가", f"새 {elem_type} 요소 ID (영문/숫자/_):", initialvalue=f"custom_{elem_type}_{len(self._elements())+1}", parent=self)
        if not eid:
            return
        try:
            self.save_layout(silent=True)
            rec = self.core.add_lobby_element({"id": eid.strip(), "type": elem_type, "label": f"커스텀 {elem_type}"})
            self.selected_id = rec["id"]
            self.reload_layout()
            self.app.set_status(f"로비 요소 추가됨: {rec['id']}")
        except Exception as e:
            messagebox.showerror("추가 오류", str(e))

    def delete_selected(self) -> None:
        if not self.selected_id:
            return
        if not messagebox.askyesno("삭제 확인", f"로비 요소 '{self.selected_id}'를 삭제할까요?"):
            return
        self.layout_data["lobby"]["elements"] = [e for e in self._elements() if e["id"] != self.selected_id]
        self.selected_id = None
        self._refresh_list()
        self.redraw_canvas()

    def reset_layout(self) -> None:
        if not messagebox.askyesno("초기화 확인", "로비 화면 레이아웃을 기본값으로 되돌릴까요?"):
            return
        self.core.reset_lobby_layout()
        self.reload_layout()
        self.app.set_status("로비 화면 레이아웃을 기본값으로 복원했습니다.")

    def save_layout(self, silent: bool = False) -> None:
        self._apply_bg_vars()
        self.core.save_ui_layout(self.layout_data)
        self.app.set_status("로비 화면 레이아웃 저장 완료 (data/ui_layout.json)")
        if not silent:
            messagebox.showinfo("저장 완료", "로비 화면 레이아웃이 data/ui_layout.json에 저장되었습니다.")


# ======================================================================
# TAB 6: IN-PLAY UI (HUD) EDITOR (인플레이 UI 화면 편집 — WYSIWYG)
# ======================================================================
class HudEditorTab(ttk.Frame):
    SCALE = 0.62  # 1280x720 -> 794x446

    def __init__(self, parent, app: DesignerApp):
        super().__init__(parent)
        self.app = app
        self.core = app.core

        self.layout_data: Dict[str, Any] = {}
        self.selected_id: Optional[str] = None
        self.snap_grid = tk.BooleanVar(value=True)
        self.show_popups = tk.BooleanVar(value=False)

        self._drag_mode: Optional[str] = None
        self._drag_start_mouse: Tuple[int, int] = (0, 0)
        self._drag_start_rect: Tuple[int, int, int, int] = (0, 0, 0, 0)

        self.v_label = tk.StringVar()
        self.v_visible = tk.BooleanVar(value=True)
        self.v_x = tk.StringVar()
        self.v_y = tk.StringVar()
        self.v_w = tk.StringVar()
        self.v_h = tk.StringVar()
        self.v_cell = tk.StringVar()
        self.v_bg = tk.StringVar()
        self.v_border = tk.StringVar()
        self.v_accent = tk.StringVar()

        self._build_ui()
        self.reload_layout()

    def _build_ui(self) -> None:
        bar = ttk.Frame(self, padding=(8, 6))
        bar.pack(fill="x")
        ttk.Label(bar, text="인플레이 UI (HUD & 팝업창) WYSIWYG 편집기 (1280×720 기준)", style="Header.TLabel").pack(side="left")
        ttk.Checkbutton(bar, text="8px 격자 스냅", variable=self.snap_grid).pack(side="left", padx=10)
        ttk.Checkbutton(bar, text="팝업창(대화/상점/인벤/퀘스트) 함께 표시", variable=self.show_popups, command=self.redraw_canvas).pack(side="left", padx=8)
        ttk.Button(bar, text="기본값 복원", command=self.reset_layout).pack(side="right", padx=4)
        ttk.Button(bar, text="인플레이 UI 저장", style="Accent.TButton", command=self.save_layout).pack(side="right", padx=6)

        paned = ttk.PanedWindow(self, orient="horizontal")
        paned.pack(fill="both", expand=True, padx=8, pady=(0, 8))

        left = ttk.Frame(paned)
        paned.add(left, weight=3)

        cw, ch = int(1280 * self.SCALE), int(720 * self.SCALE)
        self.canvas = tk.Canvas(left, width=cw, height=ch, bg="#1d2c22", highlightthickness=1, highlightbackground=ACCENT_BLUE)
        self.canvas.pack(padx=6, pady=6)
        self.canvas.bind("<Button-1>", self._on_mouse_down)
        self.canvas.bind("<B1-Motion>", self._on_mouse_drag)
        self.canvas.bind("<ButtonRelease-1>", self._on_mouse_up)

        ttk.Label(left, text="※ 상시 HUD 위젯(상태창·미니맵·액션바·보스바 등)과 팝업창의 위치/크기/색상/표시 여부를 자유롭게 배치하세요.", style="Muted.TLabel").pack(anchor="w", padx=8)

        self.widget_list = tk.Listbox(left, height=7, bg=BG_INPUT, fg=FG_TEXT, selectbackground="#455a8c", font=("Malgun Gothic", 9))
        self.widget_list.pack(fill="x", padx=6, pady=(6, 2))
        self.widget_list.bind("<<ListboxSelect>>", self._on_list_select)

        right = ttk.Frame(paned, padding=12)
        paned.add(right, weight=2)

        self.sel_header = ttk.Label(right, text="선택된 UI 위젯 속성", style="Header.TLabel")
        self.sel_header.pack(anchor="w", pady=(0, 8))

        form = ttk.Frame(right)
        form.pack(fill="x")
        form.columnconfigure(1, weight=1)

        specs = [
            ("위젯 이름:", self.v_label),
            ("X / Y:", None),
            ("너비 / 높이:", None),
            ("배경 RGBA:", self.v_bg),
            ("테두리 RGBA:", self.v_border),
            ("강조(Accent) RGBA:", self.v_accent),
            ("미니맵 셀 크기(px):", self.v_cell),
        ]
        for i, (lbl, var) in enumerate(specs):
            ttk.Label(form, text=lbl).grid(row=i, column=0, sticky="w", pady=5)
            if lbl == "X / Y:":
                sub = ttk.Frame(form)
                sub.grid(row=i, column=1, sticky="ew", padx=4, pady=5)
                ttk.Entry(sub, textvariable=self.v_x, width=8).pack(side="left")
                ttk.Entry(sub, textvariable=self.v_y, width=8).pack(side="left", padx=6)
                ttk.Checkbutton(sub, text="표시(Visible)", variable=self.v_visible).pack(side="left", padx=6)
            elif lbl == "너비 / 높이:":
                sub = ttk.Frame(form)
                sub.grid(row=i, column=1, sticky="ew", padx=4, pady=5)
                ttk.Entry(sub, textvariable=self.v_w, width=8).pack(side="left")
                ttk.Entry(sub, textvariable=self.v_h, width=8).pack(side="left", padx=6)
            elif "RGBA" in lbl:
                sub = ttk.Frame(form)
                sub.grid(row=i, column=1, sticky="ew", padx=4, pady=5)
                ttk.Entry(sub, textvariable=var).pack(side="left", fill="x", expand=True)
                ttk.Button(sub, text="선택", command=lambda _v=var: self._pick_color(_v)).pack(side="right", padx=3)
            else:
                ttk.Entry(form, textvariable=var).grid(row=i, column=1, sticky="ew", padx=4, pady=5)

        ttk.Button(right, text="속성 적용 (미리보기)", command=self.apply_inspector).pack(fill="x", pady=12)

    def _widgets(self) -> List[Dict[str, Any]]:
        return self.layout_data.setdefault("hud", {}).setdefault("widgets", [])

    def _get_selected(self) -> Optional[Dict[str, Any]]:
        return next((w for w in self._widgets() if w.get("id") == self.selected_id), None)

    def reload_layout(self) -> None:
        self.layout_data = self.core.load_ui_layout()
        self._refresh_list()
        self.redraw_canvas()

    def _refresh_list(self) -> None:
        self.widget_list.delete(0, "end")
        ws = self._widgets()
        for w in ws:
            vis = "ON" if w.get("visible", True) else "OFF"
            grp = "HUD" if w.get("group") == "hud" else "POPUP"
            self.widget_list.insert("end", f"[{vis}][{grp}] {w['id']} — {w.get('label', '')}  [{w['x']},{w['y']} {w['width']}x{w['height']}]")
        if ws and (self.selected_id is None or not any(w["id"] == self.selected_id for w in ws)):
            self.selected_id = ws[0]["id"]
        self._load_selected()

    def _on_list_select(self, _e=None) -> None:
        sel = self.widget_list.curselection()
        if not sel:
            return
        ws = self._widgets()
        if 0 <= sel[0] < len(ws):
            self.selected_id = ws[sel[0]]["id"]
            if ws[sel[0]].get("group") == "popup":
                self.show_popups.set(True)
            self._load_selected()
            self.redraw_canvas()

    def _load_selected(self) -> None:
        w = self._get_selected()
        if not w:
            return
        self.sel_header.configure(text=f"선택 위젯: {w['id']} ({w.get('group', 'hud')})")
        self.v_label.set(w.get("label", ""))
        self.v_visible.set(bool(w.get("visible", True)))
        self.v_x.set(str(w.get("x", 0)))
        self.v_y.set(str(w.get("y", 0)))
        self.v_w.set(str(w.get("width", 200)))
        self.v_h.set(str(w.get("height", 100)))
        self.v_bg.set(rgba_to_str(w.get("bg_color", [14, 16, 24, 215])))
        self.v_border.set(rgba_to_str(w.get("border_color", [130, 118, 92, 220])))
        self.v_accent.set(rgba_to_str(w.get("accent_color", [255, 203, 0, 255])))
        self.v_cell.set(str(w.get("cell_size", 5)))

    def _pick_color(self, var: tk.StringVar) -> None:
        init_hex, alpha = "#222638", 220
        try:
            rgba = DesignerCore.parse_rgba(var.get())
            init_hex, alpha = rgba_to_hex(rgba), rgba[3]
        except Exception:
            pass
        chosen = colorchooser.askcolor(color=init_hex, parent=self)
        if chosen and chosen[0]:
            r, g, b = [int(round(c)) for c in chosen[0]]
            var.set(f"{r},{g},{b},{alpha}")
            self.apply_inspector()

    def apply_inspector(self) -> None:
        w = self._get_selected()
        if not w:
            return
        try:
            w["label"] = self.v_label.get().strip()
            w["visible"] = bool(self.v_visible.get())
            w["x"] = int(float(self.v_x.get()))
            w["y"] = int(float(self.v_y.get()))
            w["width"] = max(40, int(float(self.v_w.get())))
            w["height"] = max(24, int(float(self.v_h.get())))
            w["bg_color"] = DesignerCore.parse_rgba(self.v_bg.get())
            w["border_color"] = DesignerCore.parse_rgba(self.v_border.get())
            w["accent_color"] = DesignerCore.parse_rgba(self.v_accent.get())
            if w["id"] == "minimap" and self.v_cell.get().strip():
                w["cell_size"] = max(2, int(float(self.v_cell.get())))
            self._refresh_list()
            self.redraw_canvas()
        except Exception as e:
            messagebox.showerror("속성 오류", str(e))

    def _visible_widgets_for_canvas(self) -> List[Dict[str, Any]]:
        out = []
        for w in self._widgets():
            if w.get("group") == "popup" and not self.show_popups.get() and w.get("id") != self.selected_id:
                continue
            out.append(w)
        return out

    def redraw_canvas(self) -> None:
        self.canvas.delete("all")
        s = self.SCALE
        cw, ch = int(1280 * s), int(720 * s)

        # Subtle mock game viewport grid
        step = int(64 * s)
        for gx in range(0, cw, step):
            self.canvas.create_line(gx, 0, gx, ch, fill="#24382b")
        for gy in range(0, ch, step):
            self.canvas.create_line(0, gy, cw, gy, fill="#24382b")
        self.canvas.create_oval(cw / 2 - 12, ch / 2 - 12, cw / 2 + 12, ch / 2 + 12, fill="#4e80d8", outline="#ffffff", width=2)
        self.canvas.create_text(cw / 2, ch / 2 + 22, text="플레이어 시점(중앙)", fill="#a8ccb4", font=("Malgun Gothic", 8))

        for w in self._visible_widgets_for_canvas():
            x0, y0 = w["x"] * s, w["y"] * s
            x1, y1 = (w["x"] + w["width"]) * s, (w["y"] + w["height"]) * s
            vis = w.get("visible", True)
            bg = rgba_to_hex(w.get("bg_color"), "#121520")
            bd = rgba_to_hex(w.get("border_color"), ACCENT)
            ac = rgba_to_hex(w.get("accent_color"), ACCENT)

            self.canvas.create_rectangle(x0, y0, x1, y1, fill=bg if vis else "#222222", outline=bd, width=2, dash=() if vis else (3, 3))
            title = f"{w['id']} ({'' if vis else '숨김 '}{w['width']}x{w['height']})"
            self.canvas.create_text(x0 + 6, y0 + 5, anchor="nw", text=title, fill=ac if vis else "#777777", font=("Malgun Gothic", 8, "bold"))

            # Mini interior mockup cues
            wid = w["id"]
            if wid == "player_status" and vis:
                self.canvas.create_rectangle(x0 + 8, y0 + 22, x1 - 8, y0 + 32, fill="#d73034", outline="")
                self.canvas.create_rectangle(x0 + 8, y0 + 36, x1 - 8, y0 + 44, fill="#4b9bf5", outline="")
            elif wid == "boss_bar" and vis:
                self.canvas.create_rectangle(x0 + 8, y0 + 18, x1 - 8, y1 - 6, fill="#cd232d", outline="")
            elif wid == "action_bar" and vis:
                for k in range(7):
                    sx = x0 + 8 + k * ((x1 - x0 - 16) / 7)
                    self.canvas.create_rectangle(sx, y0 + 6, sx + ((x1 - x0 - 24) / 7) - 3, y1 - 5, fill="#24293b", outline="#666052")

            if w["id"] == self.selected_id:
                self.canvas.create_rectangle(x0, y0, x1, y1, outline="#00e5ff", width=2, dash=(4, 2))
                self.canvas.create_rectangle(x1 - 8, y1 - 8, x1 + 2, y1 + 2, fill="#ffd700", outline="#111111")

    def _on_mouse_down(self, event) -> None:
        s = self.SCALE
        cur = self._get_selected()
        if cur:
            rx, ry = (cur["x"] + cur["width"]) * s, (cur["y"] + cur["height"]) * s
            if abs(event.x - rx) <= 10 and abs(event.y - ry) <= 10:
                self._drag_mode = "resize"
                self._drag_start_mouse = (event.x, event.y)
                self._drag_start_rect = (cur["x"], cur["y"], cur["width"], cur["height"])
                return

        for w in reversed(self._visible_widgets_for_canvas()):
            x0, y0 = w["x"] * s, w["y"] * s
            x1, y1 = (w["x"] + w["width"]) * s, (w["y"] + w["height"]) * s
            if x0 <= event.x <= x1 and y0 <= event.y <= y1:
                self.selected_id = w["id"]
                self._drag_mode = "move"
                self._drag_start_mouse = (event.x, event.y)
                self._drag_start_rect = (w["x"], w["y"], w["width"], w["height"])
                self._load_selected()
                self.redraw_canvas()
                return

    def _on_mouse_drag(self, event) -> None:
        if not self._drag_mode:
            return
        w = self._get_selected()
        if not w:
            return
        dx = int(round((event.x - self._drag_start_mouse[0]) / self.SCALE))
        dy = int(round((event.y - self._drag_start_mouse[1]) / self.SCALE))
        ox, oy, ow, oh = self._drag_start_rect
        snap = 8 if self.snap_grid.get() else 1

        if self._drag_mode == "move":
            w["x"] = round((ox + dx) / snap) * snap
            w["y"] = round((oy + dy) / snap) * snap
        elif self._drag_mode == "resize":
            w["width"] = max(48, round((ow + dx) / snap) * snap)
            w["height"] = max(24, round((oh + dy) / snap) * snap)

        self._load_selected()
        self.redraw_canvas()

    def _on_mouse_up(self, _event) -> None:
        if self._drag_mode:
            self._drag_mode = None
            self._refresh_list()

    def reset_layout(self) -> None:
        if not messagebox.askyesno("초기화 확인", "인플레이 UI 레이아웃을 기본값으로 복원할까요?"):
            return
        self.core.reset_hud_layout()
        self.reload_layout()
        self.app.set_status("인플레이 UI 레이아웃을 기본값으로 복원했습니다.")

    def save_layout(self) -> None:
        self.core.save_ui_layout(self.layout_data)
        self.app.set_status("인플레이 UI 레이아웃 저장 완료 (data/ui_layout.json)")
        messagebox.showinfo("저장 완료", "인플레이 UI 레이아웃이 data/ui_layout.json에 저장되었습니다.")


def launch_gui(core: Optional[DesignerCore] = None) -> int:
    app = DesignerApp(core)
    app.mainloop()
    return 0
