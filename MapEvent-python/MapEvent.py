import sys, os, json
from typing import Optional

from PySide6.QtCore import Qt, QRectF, QPointF, QLineF, QEvent, QRegularExpression
from PySide6.QtGui import (
    QPixmap, QPainter, QPen, QColor, QMouseEvent, QWheelEvent, QTransform,
    QRegularExpressionValidator, QGuiApplication
)
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QSplitter, QScrollArea, QListWidget, QListWidgetItem,
    QPushButton, QLineEdit, QLabel, QCheckBox, QMessageBox,
    QGraphicsView, QGraphicsScene, QGraphicsPixmapItem,
    QGraphicsRectItem, QFrame, QFormLayout, QComboBox,
    QStackedWidget, QGroupBox
)

from events_data import (
    RoofEvent, TileChangeEvent, StairEvent, WarpEvent, NpcEvent, MapEvents,
    load_events, save_events
)

from PySide6.QtWidgets import QToolButton

TILE_SIZE = 48
NPC_SPRITES_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "mapsprites_NPC")

# ─── УТОЛЩЁННЫЕ ПЕРА ─────────────────────────────────
PEN_W            = 4
STAIR_PEN_W      = 5
WARP_ARROW_PEN_W = 3
NPC_PEN_W        = 2

# ─── МУЛЬТИ-ЭКРАН ────────────────────────────────────
SCREEN_PRESETS = [
    (2500, (180, 1200, 220), 1.0),
    (1900, (160, 1000, 200), 1.0),
    (1580, (150,  800, 190), 1.0),
    (1420, (140,  700, 180), 0.75),
    (1340, (130,  600, 170), 0.75),
    (0,    (110,  500, 150), 0.5),
]


def detect_screen_preset():
    """Возвращает (sizes, scale) в зависимости от ширины экрана."""
    screen = QGuiApplication.primaryScreen()
    if screen is None:
        return SCREEN_PRESETS[1][1], SCREEN_PRESETS[1][2]
    w = screen.availableGeometry().width()
    for min_w, sizes, scale in SCREEN_PRESETS:
        if w >= min_w:
            return sizes, scale
    return SCREEN_PRESETS[-1][1], SCREEN_PRESETS[-1][2]

MAIN_STYLE = """
QMainWindow, QWidget { background-color: #2b2b2b; color: #dcdcdc;
    font-family: "Segoe UI", sans-serif; font-size: 11px; }
QLabel { background: transparent; }

QPushButton { background-color: #3c3c3c; border: 1px solid #555;
    padding: 2px 6px; border-radius: 2px; color: #dcdcdc; min-height: 18px; }
QPushButton:hover   { background-color: #4e4e4e; }
QPushButton:pressed { background-color: #2e2e2e; }
QPushButton:checked { background-color: #4a6a9b; border: 1px solid #6a8abb; }

QLineEdit, QComboBox { background-color: #3c3c3c; border: 1px solid #555;
    padding: 1px 3px; border-radius: 2px; color: #dcdcdc; max-height: 20px; }

QListWidget { background-color: #323232; border: 1px solid #555;
    color: #dcdcdc; padding: 0; outline: 0; }
QListWidget::item { padding: 1px 4px; min-height: 16px; }
QListWidget::item:selected { background-color: #4a6a9b; }

QGroupBox { border: 1px solid #444; border-radius: 3px;
    margin-top: 12px; padding-top: 4px; }
QGroupBox::title { subcontrol-origin: margin; left: 6px;
    padding: 0 3px; color: #a0a0a0; }

QCheckBox { spacing: 4px; padding: 0; }
QCheckBox::indicator { width: 12px; height: 12px; }
QCheckBox:disabled { color: #666; }

QToolButton { padding: 0; margin: 0; }
QScrollBar:vertical   { background: #323232; width: 8px; margin: 0; }
QScrollBar::handle:vertical   { background: #4a6a9b; min-height: 16px; border-radius: 4px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal { background: #323232; height: 8px; margin: 0; }
QScrollBar::handle:horizontal { background: #4a6a9b; min-width: 16px; border-radius: 4px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QSplitter::handle { background: #555; }
"""

class MapData:
    def __init__(self):
        self.name = self.folder = ""
        self.width = self.height = 0
        self.tileset_path = ""
        self.tiles, self.rot, self.mirror_x, self.mirror_y = [], [], [], []
        self.tiles2, self.rot2, self.mirror_x2, self.mirror_y2 = [], [], [], []
        self.cell_type = []

def load_map(filename: str) -> Optional[MapData]:
    try:
        with open(filename, 'r', encoding='utf-8') as f:
            data = json.load(f)
    except Exception as e:
        print(f"Map load error: {e}")
        return None

    w, h = data.get('width', 0), data.get('height', 0)
    if w < 1 or h < 1:
        return None

    m = MapData()
    m.width, m.height = w, h
    m.tileset_path = data.get('tileset', '')

    sz = w * h
    tiles_arr = data.get('tiles', [])
    rot_arr = data.get('rot', [])
    mx_arr = data.get('mirror_x', [])
    my_arr = data.get('mirror_y', [])

    for x in range(w):
        for y in range(h):
            idx = x * h + y
            m.tiles.append(tiles_arr[x][y] if x < len(tiles_arr) and y < len(tiles_arr[x]) else 0)
            m.rot.append(rot_arr[x][y] if x < len(rot_arr) and y < len(rot_arr[x]) else 0)
            m.mirror_x.append(bool(mx_arr[x][y]) if x < len(mx_arr) and y < len(mx_arr[x]) else False)
            m.mirror_y.append(bool(my_arr[x][y]) if x < len(my_arr) and y < len(my_arr[x]) else False)

    tiles2_arr = data.get('tiles2')
    if tiles2_arr:
        rot2_arr = data.get('rot2', [])
        mx2_arr = data.get('mirror_x2', [])
        my2_arr = data.get('mirror_y2', [])
        for x in range(w):
            for y in range(h):
                idx = x * h + y
                m.tiles2.append(tiles2_arr[x][y] if x < len(tiles2_arr) and y < len(tiles2_arr[x]) else -1)
                m.rot2.append(rot2_arr[x][y] if x < len(rot2_arr) and y < len(rot2_arr[x]) else 0)
                m.mirror_x2.append(bool(mx2_arr[x][y]) if x < len(mx2_arr) and y < len(mx2_arr[x]) else False)
                m.mirror_y2.append(bool(my2_arr[x][y]) if x < len(my2_arr) and y < len(my2_arr[x]) else False)
    else:
        m.tiles2 = [-1] * sz
        m.rot2 = [0] * sz
        m.mirror_x2 = [False] * sz
        m.mirror_y2 = [False] * sz

    collision = data.get('collision')
    if collision:
        for x in range(w):
            for y in range(h):
                m.cell_type.append(collision[x][y] if x < len(collision) and y < len(collision[x]) else 0)
    else:
        m.cell_type = [0] * sz

    return m


class EditorWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Map Event Editor")
        self.resize(1280, 720)
        self.setStyleSheet(MAIN_STYLE)

        self.current_map: Optional[MapData] = None
        self.tile_pixmaps = []
        self.zoom = 1.0
        self.panning = False
        self.last_pan_pos = QPointF()

        self.events = MapEvents()
        self.selected_idx = {"roof": -1, "tile_change": -1, "stair": -1, "warp": -1, "npc": -1}
        self.show_all = {"roof": False, "tile_change": False, "stair": False, "warp": False, "npc": False}
        self.current_section = "roof"
        self.map_entries = []
        self.current_map_idx = 0
        self._auto_sizes = None
        self._auto_map_scale = 1.0

        self.edit_widget = None
        self.highlight_items = []

        self.show_layer1 = True
        self.show_layer2 = True

        self.npc_sprite_names = []          # имена файлов без расширения
        self.npc_sprites_cache = {}         # sprite_name -> QPixmap (нижний левый кадр)
        self.npc_sprite_preview_label = None  # QLabel для предпросмотра в левой панели

        self._build_ui()
        self._load_initial_data()
        self._load_npc_sprites()

    def showEvent(self, event):
        super().showEvent(event)
        if hasattr(self, "_main_splitter") and self._auto_sizes:
            self._main_splitter.setSizes(list(self._auto_sizes))

    def _load_npc_sprites(self):
        self.npc_sprite_names.clear()
        if os.path.exists(NPC_SPRITES_DIR):
            for f in os.listdir(NPC_SPRITES_DIR):
                if f.lower().endswith('.png'):
                    self.npc_sprite_names.append(os.path.splitext(f)[0])
            self.npc_sprite_names.sort()

    def _get_npc_pixmap(self, sprite_name):
        """Возвращает QPixmap 48x48 — нижний левый кадр спрайта NPC."""
        if sprite_name in self.npc_sprites_cache:
            return self.npc_sprites_cache[sprite_name]
        path = os.path.join(NPC_SPRITES_DIR, sprite_name + ".png")
        if not os.path.exists(path):
            return None
        full = QPixmap(path)
        if full.isNull():
            return None
        # Лист 96x144 = 2×3 клетки, берём левую нижнюю (0, 96)
        pix = full.copy(0, 96, 48, 48)
        self.npc_sprites_cache[sprite_name] = pix
        return pix

    def _update_npc_preview(self, sprite_name):
        """Обновляет QLabel с предпросмотром спрайта в левой панели."""
        if self.npc_sprite_preview_label is None:
            return
        pix = self._get_npc_pixmap(sprite_name)
        if pix:
            self.npc_sprite_preview_label.setPixmap(pix.scaled(32, 32, Qt.KeepAspectRatio, Qt.SmoothTransformation))
        else:
            self.npc_sprite_preview_label.clear()

    # ── ПОСТРОЕНИЕ ИНТЕРФЕЙСА ──────────────────────────
    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QVBoxLayout(central)
        main_layout.setContentsMargins(2, 2, 2, 2)
        main_layout.setSpacing(2)

        # === Верхняя панель ===
        top = QHBoxLayout()
        top.setSpacing(4)
        top.addWidget(QLabel("Folder:"))
        self.folder_edit = QLineEdit("map1")
        self.folder_edit.setMaximumWidth(110)
        top.addWidget(self.folder_edit)
        btn_load = QPushButton("Load")
        btn_load.clicked.connect(self._load_folder)
        top.addWidget(btn_load)
        btn_save = QPushButton("Save")
        btn_save.clicked.connect(self._save_current_events)
        top.addWidget(btn_save)
        self.cb_layer1 = QCheckBox("L1")
        self.cb_layer1.setChecked(True)
        self.cb_layer1.toggled.connect(lambda v: self._toggle_layer(1, v))
        top.addWidget(self.cb_layer1)
        self.cb_layer2 = QCheckBox("L2")
        self.cb_layer2.setChecked(True)
        self.cb_layer2.toggled.connect(lambda v: self._toggle_layer(2, v))
        top.addWidget(self.cb_layer2)
        top.addStretch()
        main_layout.addLayout(top)

        # === Мульти-экран ===
        self._auto_sizes, self._auto_map_scale = detect_screen_preset()
        print(f"[screen] sizes={self._auto_sizes} scale={self._auto_map_scale}")

        # === Главный сплиттер ===
        self._main_splitter = QSplitter(Qt.Horizontal)

        # --- ЛЕВО: кнопки-разделы ---
        self.section_buttons = {}
        self.section_keys = ["roof", "tile_change", "stair", "warp", "npc"]
        self.section_titles = {
            "roof": "Roof Events",
            "tile_change": "Tile Changes",
            "stair": "Stairs",
            "warp": "Warps",
            "npc": "NPC Events",
        }
        left_widget = QWidget()
        left = QVBoxLayout(left_widget)
        left.setContentsMargins(2, 2, 2, 2)
        left.setSpacing(1)
        for key in self.section_keys:
            btn = QPushButton(self.section_titles[key])
            btn.setCheckable(True)
            btn.setStyleSheet("text-align: left; padding: 4px 6px;")
            btn.clicked.connect(lambda _c, k=key: self._select_section(k))
            left.addWidget(btn)
            self.section_buttons[key] = btn
        left.addStretch()
        left_widget.setMinimumWidth(120)
        left_widget.setMaximumWidth(170)
        self._main_splitter.addWidget(left_widget)

        # --- ЦЕНТР: карта + панель деталей ---
        center_splitter = QSplitter(Qt.Vertical)
        self.scene = QGraphicsScene()
        self.view = QGraphicsView(self.scene)
        self.view.setRenderHint(QPainter.Antialiasing, False)
        self.view.setMouseTracking(True)
        self.view.viewport().installEventFilter(self)
        self.view.setTransformationAnchor(QGraphicsView.AnchorUnderMouse)
        center_splitter.addWidget(self.view)

        self.detail_stack = QStackedWidget()
        self.section_widgets = {}
        for key in self.section_keys:
            data = self._build_detail_panel(key)
            self.section_widgets[key] = data
            self.detail_stack.addWidget(data["panel"])
        center_splitter.addWidget(self.detail_stack)
        center_splitter.setSizes([500, 220])
        self._main_splitter.addWidget(center_splitter)

        # --- ПРАВО: одна строка карты + стрелки + View-заглушки ---
        right_widget = QWidget()
        rw = QVBoxLayout(right_widget)
        rw.setContentsMargins(2, 2, 2, 2)
        rw.setSpacing(4)

        map_row = QHBoxLayout()
        map_row.setSpacing(2)
        self.map_label = QLabel("—")
        self.map_label.setStyleSheet("font-weight: bold;")
        self.map_label.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        map_row.addWidget(self.map_label, 1)
        btn_up = QPushButton("▲")
        btn_up.setFixedWidth(24)
        btn_up.clicked.connect(lambda: self._map_step(-1))
        map_row.addWidget(btn_up)
        btn_down = QPushButton("▼")
        btn_down.setFixedWidth(24)
        btn_down.clicked.connect(lambda: self._map_step(+1))
        map_row.addWidget(btn_down)
        self.map_num_label = QLabel("0")
        self.map_num_label.setFixedWidth(36)
        self.map_num_label.setAlignment(Qt.AlignCenter)
        self.map_num_label.setStyleSheet(
            "background: #3c3c3c; border: 1px solid #555; "
            "border-radius: 2px; padding: 1px 0;")
        map_row.addWidget(self.map_num_label)
        rw.addLayout(map_row)

        g_view = QGroupBox("View")
        gv = QVBoxLayout(g_view)
        gv.setSpacing(1)
        for name in ["Show grid", "Show priority", "Exploration flags",
                     "Areas", "Warps", "Triggers", "Items", "Vehicles",
                     "Flag Copies", "Step Copies", "Roof Copies", "Preview anim"]:
            cb = QCheckBox(name)
            cb.setEnabled(False)
            gv.addWidget(cb)
        rw.addWidget(g_view)

        g_areas = QGroupBox("Areas display")
        ga = QVBoxLayout(g_areas)
        ga.setSpacing(1)
        for name in ["Upper layer overlay", "BG underlay",
                     "Simulate parallax and autoscroll"]:
            cb = QCheckBox(name)
            cb.setEnabled(False)
            ga.addWidget(cb)
        rw.addWidget(g_areas)
        rw.addStretch()

        right_widget.setMinimumWidth(150)
        right_widget.setMaximumWidth(200)
        self._main_splitter.addWidget(right_widget)

        self._main_splitter.setStretchFactor(0, 0)
        self._main_splitter.setStretchFactor(1, 1)
        self._main_splitter.setStretchFactor(2, 0)
        self._main_splitter.setSizes(list(self._auto_sizes))

        main_layout.addWidget(self._main_splitter)
        self._select_section("roof")

    def _section_fields(self, etype):
        return {
            "roof": ["Tile ID", "Start X,Y", "End X,Y", "Trig1 X,Y",
                     "Trig2 X,Y", "Exit1 X,Y", "Exit2 X,Y"],
            "tile_change": ["Trigger X,Y", "New Tile", "Close X,Y"],
            "stair": ["Start X,Y", "End X,Y", "Direction (0/1)"],
            "warp": ["Trigger X,Y", "Target Map", "Target X,Y", "Facing (0-3)"],
            "npc": ["ID", "X,Y", "Sprite", "Behavior", "Direction",
                    "Text ID", "Home X,Y", "Radius"],
        }[etype]

    def _build_detail_panel(self, etype):
        """Панель под картой: список + форма + кнопки +/-."""
        panel = QWidget()
        h = QHBoxLayout(panel)
        h.setContentsMargins(2, 2, 2, 2)
        h.setSpacing(4)

        lst = QListWidget()
        lst.setMaximumWidth(280)
        lst.currentRowChanged.connect(
            lambda idx, et=etype: self._on_event_selected(et, idx))
        h.addWidget(lst)

        fields_widget = QWidget()
        fl = QFormLayout(fields_widget)
        fl.setContentsMargins(2, 2, 2, 2)
        fl.setSpacing(2)

        edits = []
        for i, lbl_text in enumerate(self._section_fields(etype)):
            if etype == "npc" and lbl_text in ("Behavior", "Direction"):
                combo = QComboBox()
                if lbl_text == "Behavior":
                    combo.addItems(["static", "wander"])
                else:
                    combo.addItems(["down", "left", "right", "up"])
                combo.currentTextChanged.connect(
                    lambda text, et=etype, fi=i: self._on_field_text_changed(et, fi, text))
                fl.addRow(QLabel(lbl_text + ":"), combo)
                edits.append(combo)
            elif etype == "npc" and lbl_text == "Sprite":
                sprite_widget = QWidget()
                sh = QHBoxLayout(sprite_widget)
                sh.setContentsMargins(0, 0, 0, 0)
                sh.setSpacing(2)
                preview_lbl = QLabel()
                preview_lbl.setFixedSize(32, 32)
                preview_lbl.setStyleSheet(
                    "border: 1px solid #555; background-color: #323232;")
                self.npc_sprite_preview_label = preview_lbl
                le = QLineEdit()
                le.setMaximumWidth(80)
                le.textChanged.connect(
                    lambda text, et=etype, fi=i: self._on_field_text_changed(et, fi, text))
                le.installEventFilter(self)
                btn_prev = QPushButton("<")
                btn_prev.setFixedWidth(24)
                btn_next = QPushButton(">")
                btn_next.setFixedWidth(24)

                def make_step(delta, line_edit=le):
                    def handler():
                        idx = self.selected_idx.get("npc", -1)
                        if idx < 0 or not self.npc_sprite_names:
                            return
                        ev = self.events.npcs[idx]
                        try:
                            cur = self.npc_sprite_names.index(ev.sprite)
                        except ValueError:
                            cur = 0
                        new_idx = (cur + delta) % len(self.npc_sprite_names)
                        line_edit.setText(self.npc_sprite_names[new_idx])
                        self._update_npc_preview(self.npc_sprite_names[new_idx])
                    return handler

                btn_prev.clicked.connect(make_step(-1))
                btn_next.clicked.connect(make_step(1))
                sh.addWidget(preview_lbl)
                sh.addWidget(le)
                sh.addWidget(btn_prev)
                sh.addWidget(btn_next)
                sh.addStretch()
                fl.addRow(QLabel(lbl_text + ":"), sprite_widget)
                edits.append(le)
            else:
                le = QLineEdit()
                le.setMaximumWidth(140)
                if lbl_text not in ("ID", "Sprite"):
                    le.setValidator(QRegularExpressionValidator(
                        QRegularExpression(r"[\d,\-]*")))
                le.textChanged.connect(
                    lambda text, et=etype, fi=i: self._on_field_text_changed(et, fi, text))
                le.installEventFilter(self)
                fl.addRow(QLabel(lbl_text + ":"), le)
                edits.append(le)

        h.addWidget(fields_widget, 1)

        btn_col = QVBoxLayout()
        btn_col.setSpacing(2)
        btn_add = QPushButton("+")
        btn_add.setFixedWidth(28)
        btn_add.clicked.connect(lambda _c, et=etype: self._add_event(et))
        btn_del = QPushButton("−")
        btn_del.setFixedWidth(28)
        btn_del.clicked.connect(lambda _c, et=etype: self._delete_event(et))
        btn_col.addWidget(btn_add)
        btn_col.addWidget(btn_del)
        btn_col.addStretch()
        h.addLayout(btn_col)

        return {
            "panel": panel,
            "list": lst,
            "fields": edits,
            "fields_widget": fields_widget,
        }

    def _select_section(self, key):
        self.current_section = key
        self.detail_stack.setCurrentWidget(self.section_widgets[key]["panel"])
        for k, btn in self.section_buttons.items():
            btn.setChecked(k == key)

    def _map_step(self, delta):
        if not self.map_entries:
            return
        self.current_map_idx = (self.current_map_idx + delta) % len(self.map_entries)
        entry = self.map_entries[self.current_map_idx]
        self.map_label.setText(entry.get("name", ""))
        self.folder_edit.setText(entry.get("folder", ""))
        self.map_num_label.setText(str(self.current_map_idx))
        self._load_map(entry.get("folder", ""))

    # ── ЗАГРУЗКА ДАННЫХ ───────────────────────────────
    def _load_folder(self):
        folder = self.folder_edit.text().strip()
        if not folder:
            QMessageBox.warning(self, "Warning", "Enter folder name")
            return
        self._load_map(folder)

    def _load_map(self, folder):
        path = os.path.join("..", "data", "maps", folder, "layout.json")
        if not os.path.exists(path):
            QMessageBox.warning(self, "Error", f"layout.json not found in {folder}")
            return
        self.current_map = load_map(path)
        if not self.current_map:
            QMessageBox.warning(self, "Error", "Failed to load map")
            return
        self.current_map.folder = folder

        self.events = load_events(folder)
        for key in self.selected_idx:
            self.selected_idx[key] = -1
        for key in self.show_all:
            self.show_all[key] = False

        self._load_tileset(self.current_map.tileset_path)
        self._refresh_event_lists()
        self._redraw_map()
        self._load_npc_sprites()   # обновим список спрайтов

    def _load_tileset(self, tileset_path):
        paths = [tileset_path, f"../{tileset_path}"]
        pix = None
        for p in paths:
            if os.path.exists(p):
                pix = QPixmap(p)
                break
        if pix is None:
            QMessageBox.warning(self, "Error", f"Tileset not found: {tileset_path}")
            return

        self.tile_pixmaps.clear()
        pw, ph = pix.width(), pix.height()
        cols = pw // TILE_SIZE
        rows = ph // TILE_SIZE
        palette_cols = 8
        strips = cols // palette_cols
        for strip in range(strips):
            sc = strip * palette_cols
            ec = sc + palette_cols
            for r in range(rows):
                for c in range(sc, ec):
                    rect = QRectF(c * TILE_SIZE, r * TILE_SIZE, TILE_SIZE, TILE_SIZE)
                    self.tile_pixmaps.append(pix.copy(rect.toRect()))

    def _refresh_event_lists(self):
        mapping = {
            "roof": "roofs",
            "tile_change": "tile_changes",
            "stair": "stairs",
            "warp": "warps",
            "npc": "npcs"
        }
        for etype, data in self.section_widgets.items():
            lst = data['list']
            lst.blockSignals(True)
            lst.clear()
            event_list = getattr(self.events, mapping[etype])
            for ev in event_list:
                lst.addItem(self._event_summary(etype, ev))
            lst.blockSignals(False)
            # data['fields_widget'].setVisible(False)

    def _event_summary(self, etype, ev):
        if etype == "roof":
            return f"Tile {ev.tile_id} ({ev.start_x},{ev.start_y})-({ev.end_x},{ev.end_y})"
        elif etype == "tile_change":
            return f"({ev.trigger_x},{ev.trigger_y}) → {ev.new_tile_id}"
        elif etype == "stair":
            return f"({ev.start_x},{ev.start_y})→({ev.end_x},{ev.end_y}) dir={ev.direction}"
        elif etype == "warp":
            return f"({ev.trigger_x},{ev.trigger_y}) → {ev.target_map}"
        elif etype == "npc":
            return f"{ev.id} ({ev.x},{ev.y}) {ev.behavior} [text:{ev.text_id}]"
        return "???"

    def _on_event_selected(self, etype, idx):
        self.selected_idx[etype] = idx
        data = self.section_widgets[etype]
        if idx < 0:
            return
        mapping = {
            "roof": "roofs",
            "tile_change": "tile_changes",
            "stair": "stairs",
            "warp": "warps",
            "npc": "npcs"
        }
        event_list = getattr(self.events, mapping[etype])
        ev = event_list[idx]
        fields = data['fields']

        if etype == "roof":
            fields[0].setText(str(ev.tile_id))
            fields[1].setText(f"{ev.start_x},{ev.start_y}")
            fields[2].setText(f"{ev.end_x},{ev.end_y}")
            fields[3].setText(f"{ev.trigger_x},{ev.trigger_y}" if ev.trigger_x!=-1 else "-")
            fields[4].setText(f"{ev.trigger2_x},{ev.trigger2_y}" if ev.trigger2_x!=-1 else "-")
            fields[5].setText(f"{ev.exit_x},{ev.exit_y}" if ev.exit_x!=-1 else "-")
            fields[6].setText(f"{ev.exit2_x},{ev.exit2_y}" if ev.exit2_x!=-1 else "-")
        elif etype == "tile_change":
            fields[0].setText(f"{ev.trigger_x},{ev.trigger_y}")
            fields[1].setText(str(ev.new_tile_id))
            fields[2].setText(f"{ev.close_x},{ev.close_y}" if ev.close_x!=-1 else "-")
        elif etype == "stair":
            fields[0].setText(f"{ev.start_x},{ev.start_y}")
            fields[1].setText(f"{ev.end_x},{ev.end_y}")
            fields[2].setText(str(ev.direction))
        elif etype == "warp":
            fields[0].setText(f"{ev.trigger_x},{ev.trigger_y}")
            fields[1].setText(ev.target_map)
            fields[2].setText(f"{ev.target_x},{ev.target_y}")
            fields[3].setText(str(ev.facing))
        elif etype == "npc":
            fields[0].setText(ev.id)
            fields[1].setText(f"{ev.x},{ev.y}")
            fields[2].setText(ev.sprite)
            fields[3].setCurrentText(ev.behavior)
            fields[4].setCurrentText(ev.direction)
            fields[5].setText(ev.text_id)
            if ev.behavior == "wander":
                fields[6].setText(f"{ev.home_x},{ev.home_y}")
                fields[7].setText(str(ev.radius))
            else:
                fields[6].setText("-")
                fields[7].setText("-")
            self._update_npc_preview(ev.sprite)
        self._update_highlights()

    def _add_event(self, etype):
        mapping = {
            "roof": "roofs",
            "tile_change": "tile_changes",
            "stair": "stairs",
            "warp": "warps",
            "npc": "npcs"
        }
        event_list = getattr(self.events, mapping[etype])
        cls_map = {
            "roof": RoofEvent,
            "tile_change": TileChangeEvent,
            "stair": StairEvent,
            "warp": WarpEvent,
            "npc": NpcEvent
        }
        event_list.append(cls_map[etype]())
        self._refresh_event_lists()
        self.section_widgets[etype]['list'].setCurrentRow(len(event_list)-1)

    def _delete_event(self, etype):
        idx = self.selected_idx[etype]
        mapping = {
            "roof": "roofs",
            "tile_change": "tile_changes",
            "stair": "stairs",
            "warp": "warps",
            "npc": "npcs"
        }
        event_list = getattr(self.events, mapping[etype])
        if 0 <= idx < len(event_list):
            del event_list[idx]
            self.selected_idx[etype] = -1
            self._refresh_event_lists()
            self._update_highlights()

    def _toggle_show_all(self, etype, checked):
        self.show_all[etype] = checked
        self._update_highlights()

    def _toggle_layer(self, layer, visible):
        if layer == 1:
            self.show_layer1 = visible
        else:
            self.show_layer2 = visible
        self._redraw_map()

    def _save_current_events(self):
        if not self.current_map:
            QMessageBox.warning(self, "Error", "No map loaded")
            return

        folder = self.current_map.folder
        reply = QMessageBox.question(
            self,
            "Confirm Save",
            f"Overwrite event data for \"{folder}\"?\n\n"
            f"This will replace events.json, NPC_events.json "
            f"and NPC_script.json in that folder.",
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No
        )
        if reply != QMessageBox.Yes:
            return

        save_events(folder, self.events)
        QMessageBox.information(self, "Saved",
                                "Events saved (including NPC_events.json).")

    # ── ЖИВОЕ ОБНОВЛЕНИЕ ПОЛЕЙ ────────────────────────
    def _on_field_text_changed(self, etype, field_idx, text):
        if not self.current_map:
            return

        mapping = {
            "roof": "roofs",
            "tile_change": "tile_changes",
            "stair": "stairs",
            "warp": "warps",
            "npc": "npcs"
        }
        event_list = getattr(self.events, mapping[etype], None)
        idx = self.selected_idx.get(etype, -1)
        if not event_list or idx < 0 or idx >= len(event_list):
            return

        ev = event_list[idx]

        def parse_int(s):
            try:
                return int(s)
            except ValueError:
                return None

        def parse_pair(s):
            if not s:
                return None
            parts = s.split(',')
            if len(parts) == 2:
                x = parse_int(parts[0])
                y = parse_int(parts[1])
                if x is not None and y is not None:
                    return x, y
            return None

        try:
            if etype == "roof":
                if field_idx == 0:
                    v = parse_int(text)
                    if v is not None: ev.tile_id = v
                elif field_idx == 1:
                    pair = parse_pair(text)
                    if pair: ev.start_x, ev.start_y = pair
                elif field_idx == 2:
                    pair = parse_pair(text)
                    if pair: ev.end_x, ev.end_y = pair
                elif field_idx == 3:
                    pair = parse_pair(text)
                    if pair: ev.trigger_x, ev.trigger_y = pair
                elif field_idx == 4:
                    if text.strip() == "-": ev.trigger2_x = ev.trigger2_y = -1
                    else:
                        pair = parse_pair(text)
                        if pair: ev.trigger2_x, ev.trigger2_y = pair
                elif field_idx == 5:
                    pair = parse_pair(text)
                    if pair: ev.exit_x, ev.exit_y = pair
                elif field_idx == 6:
                    if text.strip() == "-": ev.exit2_x = ev.exit2_y = -1
                    else:
                        pair = parse_pair(text)
                        if pair: ev.exit2_x, ev.exit2_y = pair

            elif etype == "tile_change":
                if field_idx == 0:
                    pair = parse_pair(text)
                    if pair: ev.trigger_x, ev.trigger_y = pair
                elif field_idx == 1:
                    v = parse_int(text)
                    if v is not None: ev.new_tile_id = v
                elif field_idx == 2:
                    pair = parse_pair(text)
                    if pair: ev.close_x, ev.close_y = pair

            elif etype == "stair":
                if field_idx == 0:
                    pair = parse_pair(text)
                    if pair: ev.start_x, ev.start_y = pair
                elif field_idx == 1:
                    pair = parse_pair(text)
                    if pair: ev.end_x, ev.end_y = pair
                elif field_idx == 2:
                    v = parse_int(text)
                    if v is not None and v in (0, 1): ev.direction = v

            elif etype == "warp":
                if field_idx == 0:
                    pair = parse_pair(text)
                    if pair: ev.trigger_x, ev.trigger_y = pair
                elif field_idx == 1:
                    ev.target_map = text.strip()
                elif field_idx == 2:
                    pair = parse_pair(text)
                    if pair: ev.target_x, ev.target_y = pair
                elif field_idx == 3:
                    v = parse_int(text)
                    if v is not None and 0 <= v <= 3: ev.facing = v

            elif etype == "npc":
                if field_idx == 0:
                    ev.id = text.strip()
                elif field_idx == 1:
                    pair = parse_pair(text)
                    if pair: ev.x, ev.y = pair
                elif field_idx == 2:
                    ev.sprite = text.strip()
                    self._update_npc_preview(text.strip())
                elif field_idx == 3:
                    ev.behavior = text
                    self._refresh_event_fields("npc", idx)
                elif field_idx == 4:
                    ev.direction = text
                elif field_idx == 5:          # Новое поле Text ID
                    ev.text_id = text.strip()
                elif field_idx == 6:          # Home X,Y (раньше было 5)
                    if text.strip() == "-": ev.home_x = ev.home_y = 0
                    else:
                        pair = parse_pair(text)
                        if pair: ev.home_x, ev.home_y = pair
                elif field_idx == 7:          # Radius (раньше было 6)
                    if text.strip() == "-": ev.radius = 3
                    else:
                        v = parse_int(text)
                        if v is not None: ev.radius = v
        except:
            pass

        self._update_highlights()

    def _refresh_event_fields(self, etype, idx):
        if etype != "npc": return
        data = self.section_widgets["npc"]
        event_list = self.events.npcs
        if 0 <= idx < len(event_list):
            ev = event_list[idx]
            fields = data['fields']
            # Индексы: 0-ID,1-XY,2-Sprite,3-Behavior,4-Direction,5-TextID,6-HomeXY,7-Radius
            if ev.behavior == "wander":
                fields[6].setText(f"{ev.home_x},{ev.home_y}")
                fields[7].setText(str(ev.radius))
            else:
                fields[6].setText("-")
                fields[7].setText("-")

    # ── ОТРИСОВКА КАРТЫ ───────────────────────────────
    def _redraw_map(self):
        self.scene.clear()
        self.highlight_items.clear()
        if not self.current_map or not self.tile_pixmaps:
            return

        m = self.current_map
        w, h = m.width, m.height

        if self.show_layer1:
            for x in range(w):
                for y in range(h):
                    idx = x * h + y
                    tile_id = m.tiles[idx]
                    if 0 <= tile_id < len(self.tile_pixmaps):
                        item = QGraphicsPixmapItem(self.tile_pixmaps[tile_id])
                        item.setPos(x * TILE_SIZE, y * TILE_SIZE)
                        t = QTransform()
                        if m.mirror_x[idx]: t = t.scale(-1, 1)
                        if m.mirror_y[idx]: t = t.scale(1, -1)
                        if m.rot[idx] != 0: t = t.rotate(m.rot[idx] * 90)
                        if not t.isIdentity():
                            item.setTransform(t)
                            item.setTransformOriginPoint(TILE_SIZE/2, TILE_SIZE/2)
                        self.scene.addItem(item)

        if self.show_layer2 and any(tid >= 0 for tid in m.tiles2):
            for x in range(w):
                for y in range(h):
                    idx = x * h + y
                    tid = m.tiles2[idx]
                    if 0 <= tid < len(self.tile_pixmaps):
                        item = QGraphicsPixmapItem(self.tile_pixmaps[tid])
                        item.setPos(x * TILE_SIZE, y * TILE_SIZE)
                        t = QTransform()
                        if m.mirror_x2[idx]: t = t.scale(-1, 1)
                        if m.mirror_y2[idx]: t = t.scale(1, -1)
                        if m.rot2[idx] != 0: t = t.rotate(m.rot2[idx] * 90)
                        if not t.isIdentity():
                            item.setTransform(t)
                            item.setTransformOriginPoint(TILE_SIZE/2, TILE_SIZE/2)
                        if self.show_layer1:
                            item.setOpacity(0.5)
                        self.scene.addItem(item)

        self.scene.setSceneRect(0, 0, w * TILE_SIZE, h * TILE_SIZE)
        self.view.resetTransform()
        self._update_highlights()

    def _update_highlights(self):
        for item in self.highlight_items:
            self.scene.removeItem(item)
        self.highlight_items.clear()
        if not self.current_map:
            return

        mapping = {
            "roof": "roofs",
            "tile_change": "tile_changes",
            "stair": "stairs",
            "warp": "warps",
            "npc": "npcs"
        }
        for etype, sel_idx in self.selected_idx.items():
            event_list = getattr(self.events, mapping[etype])
            show_all = self.show_all[etype]
            for i, ev in enumerate(event_list):
                if i == sel_idx or (show_all and i != sel_idx):
                    self._draw_event_highlight(etype, ev, selected=(i == sel_idx))

    def _draw_event_highlight(self, etype, ev, selected):
        alpha = 255 if selected else 120
        if etype == "npc":
            pix = self._get_npc_pixmap(ev.sprite)
            if pix:
                item = QGraphicsPixmapItem(pix)
                item.setPos(ev.x * TILE_SIZE, ev.y * TILE_SIZE)
                if self.show_layer2 and self.show_layer1:
                    item.setOpacity(0.8)
                self.scene.addItem(item)
                self.highlight_items.append(item)
            else:
                r = QRectF(ev.x * TILE_SIZE, ev.y * TILE_SIZE, TILE_SIZE, TILE_SIZE)
                pen = QPen(QColor(255, 165, 0, alpha), NPC_PEN_W, Qt.DashLine)
                self.highlight_items.append(self.scene.addRect(r, pen))
            return

        if etype == "roof":
            x1 = min(ev.start_x, ev.end_x)
            y1 = min(ev.start_y, ev.end_y)
            x2 = max(ev.start_x, ev.end_x)
            y2 = max(ev.start_y, ev.end_y)
            rect = QRectF(x1*TILE_SIZE, y1*TILE_SIZE, (x2-x1+1)*TILE_SIZE, (y2-y1+1)*TILE_SIZE)
            pen = QPen(QColor(0,255,0,alpha), PEN_W)
            brush = QColor(0,255,0,40) if selected else Qt.NoBrush
            self.highlight_items.append(self.scene.addRect(rect, pen, brush))
            for tx, ty in [(ev.trigger_x, ev.trigger_y), (ev.trigger2_x, ev.trigger2_y)]:
                if tx >= 0 and ty >= 0:
                    r = QRectF(tx*TILE_SIZE, ty*TILE_SIZE, TILE_SIZE, TILE_SIZE)
                    self.highlight_items.append(self.scene.addRect(r, QPen(QColor(255,0,0,alpha), PEN_W)))
            for ex, ey in [(ev.exit_x, ev.exit_y), (ev.exit2_x, ev.exit2_y)]:
                if ex >= 0 and ey >= 0:
                    r = QRectF(ex*TILE_SIZE, ey*TILE_SIZE, TILE_SIZE, TILE_SIZE)
                    self.highlight_items.append(self.scene.addRect(r, QPen(QColor(0,150,255,alpha), PEN_W)))
        elif etype == "tile_change":
            if ev.trigger_x >= 0 and ev.trigger_y >= 0:
                r = QRectF(ev.trigger_x*TILE_SIZE, ev.trigger_y*TILE_SIZE, TILE_SIZE, TILE_SIZE)
                self.highlight_items.append(self.scene.addRect(r, QPen(QColor(255,255,0,alpha), PEN_W)))
            if ev.close_x >= 0 and ev.close_y >= 0:
                r = QRectF(ev.close_x*TILE_SIZE, ev.close_y*TILE_SIZE, TILE_SIZE, TILE_SIZE)
                self.highlight_items.append(self.scene.addRect(r, QPen(QColor(0,200,255,alpha), PEN_W)))
        elif etype == "stair":
            dx = (ev.end_x > ev.start_x) - (ev.end_x < ev.start_x)
            dy = (ev.end_y > ev.start_y) - (ev.end_y < ev.start_y)
            steps = max(abs(ev.end_x - ev.start_x), abs(ev.end_y - ev.start_y))
            for i in range(steps + 1):
                cx = ev.start_x + i * dx
                cy = ev.start_y + i * dy
                center = QPointF(cx*TILE_SIZE + TILE_SIZE/2, cy*TILE_SIZE + TILE_SIZE/2)
                half = TILE_SIZE/2
                if ev.direction == 1:
                    line = QLineF(center.x()-half, center.y()+half, center.x()+half, center.y()-half)
                else:
                    line = QLineF(center.x()-half, center.y()-half, center.x()+half, center.y()+half)
                self.highlight_items.append(self.scene.addLine(line, QPen(QColor(0,120,255,alpha), STAIR_PEN_W)))
        elif etype == "warp":
            if ev.trigger_x >= 0 and ev.trigger_y >= 0:
                r = QRectF(ev.trigger_x*TILE_SIZE, ev.trigger_y*TILE_SIZE, TILE_SIZE, TILE_SIZE)
                self.highlight_items.append(self.scene.addRect(r, QPen(QColor(200,0,200,alpha), PEN_W)))
                cx = r.x() + r.width()/2
                cy = r.y() + r.height()/2
                sz = TILE_SIZE*0.3
                if ev.facing == 0:
                    pts = [QPointF(cx, cy+sz), QPointF(cx-sz, cy-sz/2), QPointF(cx+sz, cy-sz/2)]
                elif ev.facing == 1:
                    pts = [QPointF(cx-sz, cy), QPointF(cx+sz/2, cy-sz), QPointF(cx+sz/2, cy+sz)]
                elif ev.facing == 2:
                    pts = [QPointF(cx+sz, cy), QPointF(cx-sz/2, cy-sz), QPointF(cx-sz/2, cy+sz)]
                else:
                    pts = [QPointF(cx, cy-sz), QPointF(cx-sz, cy+sz/2), QPointF(cx+sz, cy+sz/2)]
                self.highlight_items.append(self.scene.addPolygon(pts, QPen(QColor(255,255,0,alpha), WARP_ARROW_PEN_W), QColor(255,255,0,100)))

    # ── МЫШЬ И КЛАВИШИ ────────────────────────────────
    def eventFilter(self, obj, event):
        if event.type() == QEvent.MouseButtonPress and obj is self.view.viewport():
            return self._map_press(event)
        elif event.type() == QEvent.MouseMove and obj is self.view.viewport():
            return self._map_move(event)
        elif event.type() == QEvent.MouseButtonRelease and obj is self.view.viewport():
            self.panning = False
        elif event.type() == QEvent.Wheel and obj is self.view.viewport():
            return self._map_wheel(event)
        if isinstance(obj, QLineEdit) and event.type() == QEvent.MouseButtonPress:
            self.edit_widget = obj
        return super().eventFilter(obj, event)

    def _map_press(self, event: QMouseEvent):
        if event.button() == Qt.MiddleButton or (event.button() == Qt.RightButton and event.modifiers() & Qt.ControlModifier):
            self.panning = True
            self.last_pan_pos = event.pos()
            return True
        elif event.button() == Qt.LeftButton and self.edit_widget:
            scene_pos = self.view.mapToScene(event.pos())
            tx = int(scene_pos.x() // TILE_SIZE)
            ty = int(scene_pos.y() // TILE_SIZE)
            if self.current_map and 0 <= tx < self.current_map.width and 0 <= ty < self.current_map.height:
                cur = self.edit_widget.text()
                if ',' in cur or cur in ('-', ''):
                    self.edit_widget.setText(f"{tx},{ty}")
                else:
                    self.edit_widget.setText(str(tx))
            return True
        return False

    def _map_move(self, event: QMouseEvent):
        if self.panning:
            delta = event.pos() - self.last_pan_pos
            self.last_pan_pos = event.pos()
            self.view.horizontalScrollBar().setValue(self.view.horizontalScrollBar().value() - delta.x())
            self.view.verticalScrollBar().setValue(self.view.verticalScrollBar().value() - delta.y())
            return True
        return False

    def _map_wheel(self, event: QWheelEvent):
        if event.modifiers() & Qt.ControlModifier:
            factor = 1.1 if event.angleDelta().y() > 0 else 0.9
            self.view.scale(factor, factor)
            self.zoom *= factor
            return True
        return False

    def _load_initial_data(self):
        self.map_entries = []
        self.current_map_idx = 0
        entries_path = os.path.join("..", "data", "maps", "entries.json")
        if os.path.exists(entries_path):
            with open(entries_path, "r", encoding="utf-8") as f:
                self.map_entries = json.load(f)
        if self.map_entries:
            self.current_map_idx = 0
            e = self.map_entries[0]
            self.map_label.setText(e.get("name", ""))
            self.folder_edit.setText(e.get("folder", ""))
            self.map_num_label.setText("0")
            self._load_map(e.get("folder", ""))

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = EditorWindow()
    window.show()
    sys.exit(app.exec())