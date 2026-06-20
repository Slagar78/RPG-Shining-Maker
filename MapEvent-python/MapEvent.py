import sys, os, json
from typing import Optional

from PySide6.QtCore import Qt, QRectF, QPointF, QLineF, QEvent, QRegularExpression
from PySide6.QtGui import (
    QPixmap, QPainter, QPen, QColor, QMouseEvent, QWheelEvent, QTransform,
    QRegularExpressionValidator
)
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QSplitter, QScrollArea, QListWidget, QListWidgetItem,
    QPushButton, QLineEdit, QLabel, QCheckBox, QMessageBox,
    QGraphicsView, QGraphicsScene, QGraphicsPixmapItem,
    QGraphicsRectItem, QFrame, QFormLayout
)

from events_data import (
    RoofEvent, TileChangeEvent, StairEvent, WarpEvent, MapEvents,
    load_events, save_events
)

TILE_SIZE = 48

# ─── УТОЛЩЁННЫЕ ПЕРА ─────────────────────────────────
PEN_W            = 4    # рамки зон/триггеров/выходов
STAIR_PEN_W      = 5    # линии лестниц
WARP_ARROW_PEN_W = 3    # стрелки варпов

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

        self.current_map: Optional[MapData] = None
        self.tile_pixmaps = []
        self.zoom = 1.0
        self.panning = False
        self.last_pan_pos = QPointF()

        self.events = MapEvents()
        self.selected_idx = {"roof": -1, "tile_change": -1, "stair": -1, "warp": -1}

        self.edit_widget = None
        self.show_all = {"roof": False, "tile_change": False, "stair": False, "warp": False}
        self.highlight_items = []

        self.show_layer1 = True
        self.show_layer2 = True

        self._build_ui()
        self._load_initial_data()

    # ── ПОСТРОЕНИЕ ИНТЕРФЕЙСА ──────────────────────────
    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QVBoxLayout(central)

        # Верхняя панель
        top = QHBoxLayout()
        top.addWidget(QLabel("Folder (../data/maps/):"))
        self.folder_edit = QLineEdit("map1")
        top.addWidget(self.folder_edit)
        btn_load = QPushButton("Load Map && Events")
        btn_load.clicked.connect(self._load_folder)
        top.addWidget(btn_load)
        btn_save = QPushButton("Save Events")
        btn_save.clicked.connect(self._save_current_events)
        top.addWidget(btn_save)

        self.cb_layer1 = QCheckBox("Layer 1")
        self.cb_layer1.setChecked(True)
        self.cb_layer1.toggled.connect(lambda v: self._toggle_layer(1, v))
        top.addWidget(self.cb_layer1)
        self.cb_layer2 = QCheckBox("Layer 2")
        self.cb_layer2.setChecked(True)
        self.cb_layer2.toggled.connect(lambda v: self._toggle_layer(2, v))
        top.addWidget(self.cb_layer2)

        main_layout.addLayout(top)

        splitter = QSplitter(Qt.Horizontal)

        # Левая панель (компактнее)
        left_scroll = QScrollArea()
        left_scroll.setWidgetResizable(True)
        left_widget = QWidget()
        left = QVBoxLayout(left_widget)
        left.setContentsMargins(2, 2, 2, 2)
        left.setSpacing(0)

        self._create_event_section(left, "Roof Events", "roof",
            ["Tile ID", "Start X,Y", "End X,Y", "Trig1 X,Y", "Trig2 X,Y", "Exit1 X,Y", "Exit2 X,Y"])
        self._create_event_section(left, "Tile Changes", "tile_change",
            ["Trigger X,Y", "New Tile", "Close X,Y"])
        self._create_event_section(left, "Stairs", "stair",
            ["Start X,Y", "End X,Y", "Direction (0/1)"])
        self._create_event_section(left, "Warps", "warp",
            ["Trigger X,Y", "Target Map", "Target X,Y", "Facing (0-3)"])

        left_scroll.setWidget(left_widget)

        # Карта
        self.scene = QGraphicsScene()
        self.view = QGraphicsView(self.scene)
        self.view.setRenderHint(QPainter.Antialiasing, False)
        self.view.setMouseTracking(True)
        self.view.viewport().installEventFilter(self)
        self.view.setTransformationAnchor(QGraphicsView.AnchorUnderMouse)

        # Правая панель
        right_panel = QWidget()
        right = QVBoxLayout(right_panel)
        right.addWidget(QLabel("Maps (entries.json):"))
        self.map_list_widget = QListWidget()
        self.map_list_widget.itemClicked.connect(self._on_map_selected)
        right.addWidget(self.map_list_widget)

        splitter.addWidget(left_scroll)
        splitter.addWidget(self.view)
        splitter.addWidget(right_panel)
        splitter.setSizes([240, 790, 200])
        main_layout.addWidget(splitter)

    def _create_event_section(self, parent_layout, title, etype, field_labels):
        section = QWidget()
        layout = QVBoxLayout(section)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(1)

        header = QWidget()
        hl = QHBoxLayout(header)
        hl.setContentsMargins(2, 0, 2, 0)

        toggle_btn = QPushButton("+")
        toggle_btn.setFixedSize(18, 18)
        toggle_btn.setStyleSheet("font-weight: bold; padding: 0px;")
        hl.addWidget(toggle_btn)

        lbl = QLabel(title)
        lbl.setStyleSheet("font-weight: bold;")
        hl.addWidget(lbl)
        hl.addStretch()

        cb_show = QCheckBox("All")
        cb_show.setFixedWidth(40)
        cb_show.toggled.connect(lambda checked, et=etype: self._toggle_show_all(et, checked))
        hl.addWidget(cb_show)

        layout.addWidget(header)

        collapsible = QWidget()
        cl = QVBoxLayout(collapsible)
        cl.setContentsMargins(12, 1, 1, 1)
        cl.setSpacing(1)

        lst = QListWidget()
        lst.setMaximumHeight(60)
        lst.currentRowChanged.connect(lambda idx, et=etype: self._on_event_selected(et, idx))
        cl.addWidget(lst)

        fields_widget = QWidget()
        fl = QFormLayout(fields_widget)
        fl.setContentsMargins(0, 1, 0, 1)
        fl.setSpacing(1)
        edits = []
        for i, lbl_text in enumerate(field_labels):
            le = QLineEdit()
            le.setMaximumWidth(120)
            # ── ВАЛИДАТОР: только цифры, запятая, минус ──
            validator = QRegularExpressionValidator(QRegularExpression(r"[\d,\-]*"))
            le.setValidator(validator)
            # ── ЖИВОЕ ОБНОВЛЕНИЕ ПРИ ВВОДЕ ──
            le.textChanged.connect(lambda text, et=etype, fi=i: self._on_field_text_changed(et, fi, text))
            le.installEventFilter(self)
            fl.addRow(QLabel(lbl_text + ":"), le)
            edits.append(le)
        fields_widget.setVisible(False)
        cl.addWidget(fields_widget)

        btn_layout = QHBoxLayout()
        btn_add = QPushButton("+")
        btn_add.setFixedWidth(24)
        btn_add.clicked.connect(lambda: self._add_event(etype))
        btn_del = QPushButton("-")
        btn_del.setFixedWidth(24)
        btn_del.clicked.connect(lambda: self._delete_event(etype))
        btn_layout.addWidget(btn_add)
        btn_layout.addWidget(btn_del)
        btn_layout.addStretch()
        cl.addLayout(btn_layout)

        collapsible.setVisible(False)
        layout.addWidget(collapsible)

        line = QFrame()
        line.setFrameShape(QFrame.HLine)
        line.setFrameShadow(QFrame.Sunken)
        layout.addWidget(line)

        parent_layout.addWidget(section)

        if not hasattr(self, 'section_widgets'):
            self.section_widgets = {}
        self.section_widgets[etype] = {
            'collapsible': collapsible,
            'list': lst,
            'fields': edits,
            'fields_widget': fields_widget,
            'toggle_btn': toggle_btn
        }

        def toggle():
            state = not collapsible.isVisible()
            collapsible.setVisible(state)
            toggle_btn.setText("-" if state else "+")
        toggle_btn.clicked.connect(toggle)

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
        # ← СБРОС ЧЕКБОКСОВ «SHOW ALL»
        for key in self.show_all:
            self.show_all[key] = False

        self._load_tileset(self.current_map.tileset_path)
        self._refresh_event_lists()
        self._redraw_map()

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
            "warp": "warps"
        }
        for etype, data in self.section_widgets.items():
            lst = data['list']
            lst.blockSignals(True)
            lst.clear()
            event_list = getattr(self.events, mapping[etype])
            for ev in event_list:
                lst.addItem(self._event_summary(etype, ev))
            lst.blockSignals(False)
            data['fields_widget'].setVisible(False)

    def _event_summary(self, etype, ev):
        if etype == "roof":
            return f"Tile {ev.tile_id} ({ev.start_x},{ev.start_y})-({ev.end_x},{ev.end_y})"
        elif etype == "tile_change":
            return f"({ev.trigger_x},{ev.trigger_y}) -> {ev.new_tile_id}"
        elif etype == "stair":
            return f"({ev.start_x},{ev.start_y})->({ev.end_x},{ev.end_y}) dir={ev.direction}"
        elif etype == "warp":
            return f"({ev.trigger_x},{ev.trigger_y}) -> {ev.target_map}"
        return "???"

    def _on_event_selected(self, etype, idx):
        self.selected_idx[etype] = idx
        data = self.section_widgets[etype]
        if idx < 0:
            data['fields_widget'].setVisible(False)
        else:
            mapping = {
                "roof": "roofs",
                "tile_change": "tile_changes",
                "stair": "stairs",
                "warp": "warps"
            }
            event_list = getattr(self.events, mapping[etype])
            ev = event_list[idx]
            fields = data['fields']
            data['fields_widget'].setVisible(True)

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
        self._update_highlights()

    def _add_event(self, etype):
        mapping = {
            "roof": "roofs",
            "tile_change": "tile_changes",
            "stair": "stairs",
            "warp": "warps"
        }
        event_list = getattr(self.events, mapping[etype])
        cls_map = {"roof": RoofEvent, "tile_change": TileChangeEvent,
                   "stair": StairEvent, "warp": WarpEvent}
        event_list.append(cls_map[etype]())
        self._refresh_event_lists()
        self.section_widgets[etype]['list'].setCurrentRow(len(event_list)-1)

    def _delete_event(self, etype):
        idx = self.selected_idx[etype]
        mapping = {
            "roof": "roofs",
            "tile_change": "tile_changes",
            "stair": "stairs",
            "warp": "warps"
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
        save_events(self.current_map.folder, self.events)
        QMessageBox.information(self, "Saved", "Events saved.")

    # ── ЖИВОЕ ОБНОВЛЕНИЕ ПОЛЕЙ ────────────────────────
    def _on_field_text_changed(self, etype, field_idx, text):
        if not self.current_map:
            return

        mapping = {
            "roof": "roofs",
            "tile_change": "tile_changes",
            "stair": "stairs",
            "warp": "warps"
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
        except:
            pass

        self._update_highlights()

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
            "warp": "warps"
        }
        for etype, sel_idx in self.selected_idx.items():
            event_list = getattr(self.events, mapping[etype])
            show_all = self.show_all[etype]
            for i, ev in enumerate(event_list):
                if i == sel_idx or (show_all and i != sel_idx):
                    self._draw_event_highlight(etype, ev, selected=(i == sel_idx))

    def _draw_event_highlight(self, etype, ev, selected):
        alpha = 255 if selected else 120
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

    def _on_map_selected(self, item):
        folder = item.data(Qt.UserRole)
        if folder:
            self._load_map(folder)

    def _load_initial_data(self):
        entries_path = os.path.join("..", "data", "maps", "entries.json")
        if os.path.exists(entries_path):
            with open(entries_path, 'r', encoding='utf-8') as f:
                entries = json.load(f)
            for entry in entries:
                item = QListWidgetItem(entry.get('name', ''))
                item.setData(Qt.UserRole, entry.get('folder', ''))
                self.map_list_widget.addItem(item)
        if self.map_list_widget.count() > 0:
            self.map_list_widget.setCurrentRow(0)
            self._on_map_selected(self.map_list_widget.item(0))

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = EditorWindow()
    window.show()
    sys.exit(app.exec())