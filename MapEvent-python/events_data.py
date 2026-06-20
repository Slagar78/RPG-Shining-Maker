import json
import os
from dataclasses import dataclass, field
from typing import List

# ---------- Модели событий ----------

@dataclass
class RoofEvent:
    tile_id: int = 0
    start_x: int = 0
    start_y: int = 0
    end_x: int = 1
    end_y: int = 1
    trigger_x: int = -1
    trigger_y: int = -1
    trigger2_x: int = -1
    trigger2_y: int = -1
    exit_x: int = -1
    exit_y: int = -1
    exit2_x: int = -1
    exit2_y: int = -1

    def to_dict(self) -> dict:
        d = {
            "type": "roof",
            "tile_id": self.tile_id,
            "start_x": self.start_x, "start_y": self.start_y,
            "end_x": self.end_x, "end_y": self.end_y,
            "triggers": [], "exits": []
        }
        if self.trigger_x != -1 or self.trigger_y != -1:
            d["triggers"].append([self.trigger_x, self.trigger_y])
        if self.trigger2_x != -1 or self.trigger2_y != -1:
            d["triggers"].append([self.trigger2_x, self.trigger2_y])
        if self.exit_x != -1 or self.exit_y != -1:
            d["exits"].append([self.exit_x, self.exit_y])
        if self.exit2_x != -1 or self.exit2_y != -1:
            d["exits"].append([self.exit2_x, self.exit2_y])
        return d


@dataclass
class TileChangeEvent:
    trigger_x: int = -1
    trigger_y: int = -1
    new_tile_id: int = 0
    sample_x: int = -1
    sample_y: int = -1
    close_x: int = -1
    close_y: int = -1

    def to_dict(self) -> dict:
        return {
            "type": "tile_change",
            "trigger_x": self.trigger_x, "trigger_y": self.trigger_y,
            "new_tile_id": self.new_tile_id,
            "sample_x": self.sample_x, "sample_y": self.sample_y,
            "close_x": self.close_x, "close_y": self.close_y
        }


@dataclass
class StairEvent:
    start_x: int = 0
    start_y: int = 0
    end_x: int = 1
    end_y: int = 1
    direction: int = 0   # 0='\', 1='/'

    def to_dict(self) -> dict:
        return {
            "type": "stairs",
            "start_x": self.start_x, "start_y": self.start_y,
            "end_x": self.end_x, "end_y": self.end_y,
            "direction": self.direction
        }


@dataclass
class WarpEvent:
    trigger_x: int = -1
    trigger_y: int = -1
    target_map: str = ""
    target_x: int = 0
    target_y: int = 0
    facing: int = 0   # 0=Down,1=Left,2=Right,3=Up

    def to_dict(self) -> dict:
        return {
            "type": "warp",
            "trigger_x": self.trigger_x, "trigger_y": self.trigger_y,
            "target_map": self.target_map,
            "target_x": self.target_x, "target_y": self.target_y,
            "facing": self.facing
        }


@dataclass
class NpcEvent:
    id: str = ""
    x: int = 0
    y: int = 0
    sprite: str = ""
    behavior: str = "static"   # "static" или "wander"
    direction: str = "down"    # "up","down","left","right"
    home_x: int = 0
    home_y: int = 0
    radius: int = 3

    def to_dict(self) -> dict:
        d = {
            "id": self.id,
            "x": self.x,
            "y": self.y,
            "sprite": self.sprite,
            "behavior": self.behavior,
            "direction": self.direction
        }
        if self.behavior == "wander":
            d["home_x"] = self.home_x
            d["home_y"] = self.home_y
            d["radius"] = self.radius
        return d


@dataclass
class MapEvents:
    roofs: List[RoofEvent] = field(default_factory=list)
    tile_changes: List[TileChangeEvent] = field(default_factory=list)
    stairs: List[StairEvent] = field(default_factory=list)
    warps: List[WarpEvent] = field(default_factory=list)
    npcs: List[NpcEvent] = field(default_factory=list)


# ---------- Загрузка / Сохранение ----------

def load_events(folder: str) -> MapEvents:
    path = os.path.join("..", "data", "maps", folder, "events.json")
    events = MapEvents()
    if os.path.exists(path):
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
        for item in data:
            t = item.get("type", "")
            if t == "roof":
                re = RoofEvent(
                    tile_id=item.get("tile_id", 0),
                    start_x=item.get("start_x", 0), start_y=item.get("start_y", 0),
                    end_x=item.get("end_x", 1), end_y=item.get("end_y", 1)
                )
                triggers = item.get("triggers", [])
                if len(triggers) > 0:
                    re.trigger_x, re.trigger_y = triggers[0]
                if len(triggers) > 1:
                    re.trigger2_x, re.trigger2_y = triggers[1]
                exits = item.get("exits", [])
                if len(exits) > 0:
                    re.exit_x, re.exit_y = exits[0]
                if len(exits) > 1:
                    re.exit2_x, re.exit2_y = exits[1]
                events.roofs.append(re)

            elif t == "tile_change":
                tc = TileChangeEvent(
                    trigger_x=item.get("trigger_x", -1), trigger_y=item.get("trigger_y", -1),
                    new_tile_id=item.get("new_tile_id", 0),
                    sample_x=item.get("sample_x", -1), sample_y=item.get("sample_y", -1),
                    close_x=item.get("close_x", -1), close_y=item.get("close_y", -1)
                )
                events.tile_changes.append(tc)

            elif t == "stairs":
                st = StairEvent(
                    start_x=item.get("start_x", 0), start_y=item.get("start_y", 0),
                    end_x=item.get("end_x", 1), end_y=item.get("end_y", 1),
                    direction=item.get("direction", 0)
                )
                events.stairs.append(st)

            elif t == "warp":
                facing = item.get("facing", 0)
                if facing == 2: facing = 0
                elif facing == 4: facing = 1
                elif facing == 6: facing = 2
                elif facing == 8: facing = 3
                wp = WarpEvent(
                    trigger_x=item.get("trigger_x", -1), trigger_y=item.get("trigger_y", -1),
                    target_map=item.get("target_map", ""),
                    target_x=item.get("target_x", 0), target_y=item.get("target_y", 0),
                    facing=facing
                )
                events.warps.append(wp)

    # NPC
    npc_path = os.path.join("..", "data", "maps", folder, "NPC_events.json")
    if os.path.exists(npc_path):
        with open(npc_path, "r", encoding="utf-8") as f:
            npc_data = json.load(f)
        for nd in npc_data:
            npc = NpcEvent(
                id=nd.get("id", ""),
                x=nd.get("x", 0),
                y=nd.get("y", 0),
                sprite=nd.get("sprite", ""),
                behavior=nd.get("behavior", "static"),
                direction=nd.get("direction", "down"),
                home_x=nd.get("home_x", 0),
                home_y=nd.get("home_y", 0),
                radius=nd.get("radius", 3)
            )
            events.npcs.append(npc)

    return events


def save_events(folder: str, events: MapEvents):
    dir_path = os.path.join("..", "data", "maps", folder)
    os.makedirs(dir_path, exist_ok=True)

    # events.json (без NPC)
    result = []
    for re in events.roofs:
        result.append(re.to_dict())
    for tc in events.tile_changes:
        result.append(tc.to_dict())
    for st in events.stairs:
        result.append(st.to_dict())
    for wp in events.warps:
        result.append(wp.to_dict())

    filepath = os.path.join(dir_path, "events.json")
    with open(filepath, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2)

    # NPC_events.json
    npc_list = [npc.to_dict() for npc in events.npcs]
    npc_filepath = os.path.join(dir_path, "NPC_events.json")
    with open(npc_filepath, "w", encoding="utf-8") as f:
        json.dump(npc_list, f, indent=2)