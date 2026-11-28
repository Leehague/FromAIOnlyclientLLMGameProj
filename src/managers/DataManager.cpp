#include "managers/DataManager.h"
#include "core/GameManager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <sstream>

using json = nlohmann::json;

static ItemType ParseItemType(const std::string& s) {
    if (s == "weapon") return ItemType::Weapon;
    if (s == "armor") return ItemType::Armor;
    if (s == "quest") return ItemType::Quest;
    return ItemType::Consumable;
}

static SkillCostType ParseSkillCostType(const std::string& s) {
    if (s == "mp") return SkillCostType::Mp;
    if (s == "energy") return SkillCostType::Energy;
    if (s == "hp") return SkillCostType::Hp;
    return SkillCostType::None;
}

bool DataManager::ReadFileText(const std::string& relPath, std::string& outText) {
    std::string fullPath = GameManager::Get().ResolvePath(relPath);
    std::ifstream file(fullPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[DataManager] Cannot open file: " << fullPath << "\n";
        return false;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    outText = ss.str();
    m_combinedJsonText += outText;
    m_combinedJsonText.push_back('\n');
    return true;
}

static Color ParseColorRGBA(const json& obj, const char* key, Color fallback) {
    if (!obj.contains(key) || !obj[key].is_array() || obj[key].size() < 3) {
        return fallback;
    }
    const auto& arr = obj[key];
    unsigned char r = static_cast<unsigned char>(arr[0].get<int>());
    unsigned char g = static_cast<unsigned char>(arr[1].get<int>());
    unsigned char b = static_cast<unsigned char>(arr[2].get<int>());
    unsigned char a = arr.size() >= 4 ? static_cast<unsigned char>(arr[3].get<int>()) : 255;
    return Color{r, g, b, a};
}

bool DataManager::Init() {
    m_combinedJsonText.clear();
    return LoadStrings() &&
           LoadUILayout() &&
           LoadTiles() &&
           LoadPlayer() &&
           LoadItems() &&
           LoadMonsters() &&
           LoadBosses() &&
           LoadNpcs() &&
           LoadQuests() &&
           LoadMaps() &&
           LoadSkills();
}

void DataManager::Shutdown() {
    m_strings.clear();
    m_fontCandidates.clear();
    m_fontExtraGlyphs.clear();
    m_uiLayout = UILayoutSpec{};
    m_tileMap.clear();
    m_items.clear();
    m_itemMap.clear();
    m_monsterMap.clear();
    m_bossMap.clear();
    m_npcs.clear();
    m_npcMap.clear();
    m_quests.clear();
    m_questMap.clear();
    m_mapMap.clear();
    m_skillConfig = SkillSystemConfig{};
    m_skills.clear();
    m_skillMap.clear();
    m_combinedJsonText.clear();
}

bool DataManager::LoadStrings() {
    std::string text;
    if (!ReadFileText("data/strings.json", text)) return false;
    try {
        json root = json::parse(text);
        m_strings.clear();
        m_fontCandidates.clear();

        if (root.contains("font")) {
            const auto& f = root.at("font");
            m_fontSize = f.value("size", 28);
            m_fontExtraGlyphs = f.value("extra_glyphs", "");
            if (f.contains("candidates")) {
                for (const auto& c : f.at("candidates")) {
                    m_fontCandidates.push_back(c.get<std::string>());
                }
            }
        }
        if (root.contains("strings")) {
            for (auto it = root.at("strings").begin(); it != root.at("strings").end(); ++it) {
                m_strings[it.key()] = it.value().get<std::string>();
            }
        }
        return !m_strings.empty();
    } catch (const std::exception& e) {
        std::cerr << "[DataManager] Error parsing strings.json: " << e.what() << "\n";
        return false;
    }
}

const std::string& DataManager::GetText(const std::string& key) const {
    auto it = m_strings.find(key);
    if (it != m_strings.end()) {
        return it->second;
    }
    return key;
}

std::string DataManager::FormatText(const std::string& key, const std::vector<std::string>& args) const {
    std::string result = GetText(key);
    for (size_t i = 0; i < args.size(); ++i) {
        std::string token = "{" + std::to_string(i) + "}";
        size_t pos = 0;
        while ((pos = result.find(token, pos)) != std::string::npos) {
            result.replace(pos, token.size(), args[i]);
            pos += args[i].size();
        }
    }
    return result;
}

const std::string& DataManager::GetDirectionName(Direction8 dir) const {
    return GetText(Direction8Key(dir));
}

bool DataManager::LoadUILayout() {
    std::string text;
    if (!ReadFileText("data/ui_layout.json", text)) return false;
    try {
        json root = json::parse(text);
        m_uiLayout = UILayoutSpec{};
        m_uiLayout.refWidth = root.value("reference_width", 1280);
        m_uiLayout.refHeight = root.value("reference_height", 720);

        if (root.contains("lobby")) {
            const auto& lob = root.at("lobby");
            m_uiLayout.lobby.bgTopColor = ParseColorRGBA(lob, "bg_top_color", Color{18, 22, 34, 255});
            m_uiLayout.lobby.bgBottomColor = ParseColorRGBA(lob, "bg_bottom_color", Color{38, 20, 26, 255});
            if (lob.contains("elements")) {
                for (const auto& e : lob.at("elements")) {
                    LobbyElementSpec es;
                    es.id = e.value("id", "");
                    es.label = e.value("label", es.id);
                    es.type = e.value("type", "text");
                    es.action = e.value("action", "");
                    es.visible = e.value("visible", true);
                    es.x = e.value("x", 0.0f);
                    es.y = e.value("y", 0.0f);
                    es.width = e.value("width", 100.0f);
                    es.height = e.value("height", 40.0f);
                    es.artId = e.value("art_id", 0);
                    es.frameW = e.value("frame_w", 0);
                    es.frameH = e.value("frame_h", 0);
                    es.textKey = e.value("text_key", "");
                    es.text = e.value("text", "");
                    if (e.contains("text_keys")) {
                        for (const auto& tk : e.at("text_keys")) {
                            es.textKeys.push_back(tk.get<std::string>());
                        }
                    }
                    es.fontSize = e.value("font_size", 18.0f);
                    es.align = e.value("align", "center");
                    es.color = ParseColorRGBA(e, "color", WHITE);
                    es.bgColor = ParseColorRGBA(e, "bg_color", Color{15, 18, 28, 210});
                    es.hoverColor = ParseColorRGBA(e, "hover_color", Color{185, 65, 55, 255});
                    es.borderColor = ParseColorRGBA(e, "border_color", Color{160, 140, 95, 200});
                    m_uiLayout.lobby.elements.push_back(std::move(es));
                }
            }
        }

        if (root.contains("hud") && root.at("hud").contains("widgets")) {
            for (const auto& w : root.at("hud").at("widgets")) {
                HudWidgetSpec ws;
                ws.id = w.value("id", "");
                ws.label = w.value("label", ws.id);
                ws.group = w.value("group", "hud");
                ws.visible = w.value("visible", true);
                ws.x = w.value("x", 0.0f);
                ws.y = w.value("y", 0.0f);
                ws.width = w.value("width", 200.0f);
                ws.height = w.value("height", 100.0f);
                ws.bgColor = ParseColorRGBA(w, "bg_color", Color{14, 16, 24, 215});
                ws.borderColor = ParseColorRGBA(w, "border_color", Color{130, 118, 92, 220});
                ws.accentColor = ParseColorRGBA(w, "accent_color", GOLD);
                ws.cellSize = w.value("cell_size", 5.0f);
                m_uiLayout.hudWidgetIndex[ws.id] = m_uiLayout.hudWidgets.size();
                m_uiLayout.hudWidgets.push_back(std::move(ws));
            }
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[DataManager] Error parsing ui_layout.json: " << e.what() << "\n";
        return false;
    }
}

const HudWidgetSpec* DataManager::GetHudWidget(const std::string& id) const {
    auto it = m_uiLayout.hudWidgetIndex.find(id);
    if (it != m_uiLayout.hudWidgetIndex.end()) {
        return &m_uiLayout.hudWidgets[it->second];
    }
    return nullptr;
}

bool DataManager::LoadTiles() {
    std::string text;
    if (!ReadFileText("data/tiles.json", text)) return false;
    try {
        json root = json::parse(text);
        m_tileSize = root.value("tile_size", 32);
        for (const auto& t : root.at("tiles")) {
            TileSpec spec;
            spec.id = t.at("id").get<int>();
            spec.key = t.value("key", "");
            spec.name = t.value("name", "");
            spec.artId = t.at("art_id").get<int>();
            spec.walkable = t.value("walkable", true);
            spec.special = t.value("special", "none");
            spec.conceptDesc = t.value("concept", "");
            m_tileMap[spec.id] = spec;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[DataManager] Error parsing tiles.json: " << e.what() << "\n";
        return false;
    }
}

bool DataManager::LoadPlayer() {
    std::string text;
    if (!ReadFileText("data/player.json", text)) return false;
    try {
        json r = json::parse(text);
        m_playerSpec.name = r.value("name", "Hero");
        m_playerSpec.startMap = r.value("start_map", "town");
        auto st = r.at("start_tile");
        m_playerSpec.startTileX = st[0].get<int>();
        m_playerSpec.startTileY = st[1].get<int>();
        m_playerSpec.startLevel = r.value("start_level", 1);
        m_playerSpec.maxLevel = r.value("max_level", 10);
        m_playerSpec.baseMaxHp = r.value("base_max_hp", 120);
        m_playerSpec.baseMaxMp = r.value("base_max_mp", 100);
        m_playerSpec.baseMaxEnergy = r.value("base_max_energy", 100);
        m_playerSpec.baseAtk = r.value("base_atk", 16);
        m_playerSpec.baseDef = r.value("base_def", 4);
        m_playerSpec.hpPerLevel = r.value("hp_per_level", 25);
        m_playerSpec.mpPerLevel = r.value("mp_per_level", 15);
        m_playerSpec.energyPerLevel = r.value("energy_per_level", 10);
        m_playerSpec.mpRegenPerSec = r.value("mp_regen_per_sec", 5.5f);
        m_playerSpec.energyRegenPerSec = r.value("energy_regen_per_sec", 18.0f);
        m_playerSpec.atkPerLevel = r.value("atk_per_level", 4);
        m_playerSpec.defPerLevel = r.value("def_per_level", 2);
        m_playerSpec.moveSpeed = r.value("move_speed", 145.0f);
        m_playerSpec.dashSpeed = r.value("dash_speed", 390.0f);
        m_playerSpec.dashDuration = r.value("dash_duration", 0.22f);
        m_playerSpec.dashCooldown = r.value("dash_cooldown", 1.6f);
        m_playerSpec.attackRange = r.value("attack_range", 46.0f);
        m_playerSpec.attackCooldown = r.value("attack_cooldown", 0.34f);
        m_playerSpec.attackDuration = r.value("attack_duration", 0.26f);
        m_playerSpec.skillName = r.value("skill_name", "");
        m_playerSpec.skillRadius = r.value("skill_radius", 72.0f);
        m_playerSpec.skillMultiplier = r.value("skill_multiplier", 1.85f);
        m_playerSpec.skillCooldown = r.value("skill_cooldown", 3.8f);
        m_playerSpec.startGold = r.value("start_gold", 60);

        m_playerSpec.startItems.clear();
        if (r.contains("start_items")) {
            for (const auto& entry : r.at("start_items")) {
                StartItemEntry se;
                se.itemId = entry.at("item_id").get<std::string>();
                se.count = entry.value("count", 1);
                m_playerSpec.startItems.push_back(se);
            }
        }
        const auto& a = r.at("animation");
        m_playerSpec.anim.idleArtId = a.value("idle_art_id", 101);
        m_playerSpec.anim.walkArtId = a.value("walk_art_id", 102);
        m_playerSpec.anim.attackArtId = a.value("attack_art_id", 103);
        m_playerSpec.anim.frameWidth = a.value("frame_width", 48);
        m_playerSpec.anim.frameHeight = a.value("frame_height", 48);
        m_playerSpec.anim.framesPerRow = a.value("frames_per_row", 4);
        m_playerSpec.anim.directions = a.value("directions", 8);
        m_playerSpec.anim.idleFps = a.value("idle_fps", 4.5f);
        m_playerSpec.anim.walkFps = a.value("walk_fps", 10.0f);
        m_playerSpec.anim.attackFps = a.value("attack_fps", 15.0f);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[DataManager] Error parsing player.json: " << e.what() << "\n";
        return false;
    }
}

bool DataManager::LoadItems() {
    std::string text;
    if (!ReadFileText("data/items.json", text)) return false;
    try {
        json root = json::parse(text);
        m_items.clear();
        m_itemMap.clear();
        for (const auto& it : root.at("items")) {
            ItemSpec spec;
            spec.id = it.at("id").get<std::string>();
            spec.name = it.value("name", spec.id);
            spec.type = ParseItemType(it.value("type", "consumable"));
            spec.price = it.value("price", 0);
            spec.healHp = it.value("heal_hp", 0);
            spec.atkBonus = it.value("atk_bonus", 0);
            spec.defBonus = it.value("def_bonus", 0);
            spec.hpBonus = it.value("hp_bonus", 0);
            spec.artId = it.value("art_id", 501);
            spec.description = it.value("description", "");
            m_itemMap[spec.id] = m_items.size();
            m_items.push_back(spec);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[DataManager] Error parsing items.json: " << e.what() << "\n";
        return false;
    }
}

bool DataManager::LoadMonsters() {
    std::string text;
    if (!ReadFileText("data/monsters.json", text)) return false;
    try {
        json root = json::parse(text);
        for (const auto& m : root.at("monsters")) {
            MonsterSpec spec;
            spec.id = m.at("id").get<std::string>();
            spec.name = m.value("name", spec.id);
            spec.maxHp = m.value("max_hp", 50);
            spec.atk = m.value("atk", 10);
            spec.def = m.value("def", 2);
            spec.moveSpeed = m.value("move_speed", 65.0f);
            spec.detectRange = m.value("detect_range", 160.0f);
            spec.attackRange = m.value("attack_range", 30.0f);
            spec.attackCooldown = m.value("attack_cooldown", 1.2f);
            spec.expReward = m.value("exp_reward", 30);
            spec.goldReward = m.value("gold_reward", 20);
            spec.respawnTime = m.value("respawn_time", 12.0f);
            spec.artId = m.value("art_id", 201);
            spec.frameSize = m.value("frame_size", 40);
            spec.frames = m.value("frames", 4);
            spec.ranged = m.value("ranged", false);
            m_monsterMap[spec.id] = spec;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[DataManager] Error parsing monsters.json: " << e.what() << "\n";
        return false;
    }
}

bool DataManager::LoadBosses() {
    std::string text;
    if (!ReadFileText("data/bosses.json", text)) return false;
    try {
        json root = json::parse(text);
        for (const auto& b : root.at("bosses")) {
            BossSpec spec;
            spec.id = b.at("id").get<std::string>();
            spec.name = b.value("name", spec.id);
            spec.title = b.value("title", "");
            spec.maxHp = b.value("max_hp", 750);
            spec.atk = b.value("atk", 30);
            spec.def = b.value("def", 9);
            spec.moveSpeed = b.value("move_speed", 76.0f);
            spec.enrageSpeed = b.value("enrage_speed", 106.0f);
            spec.enrageHpRatio = b.value("enrage_hp_ratio", 0.5f);
            spec.attackRange = b.value("attack_range", 52.0f);
            spec.attackCooldown = b.value("attack_cooldown", 1.15f);
            spec.fireballCooldown = b.value("fireball_cooldown", 4.0f);
            spec.fireballCountP1 = b.value("fireball_count_p1", 8);
            spec.fireballCountP2 = b.value("fireball_count_p2", 12);
            spec.fireballSpeed = b.value("fireball_speed", 155.0f);
            spec.aoeCooldown = b.value("aoe_cooldown", 5.2f);
            spec.aoeRadius = b.value("aoe_radius", 54.0f);
            spec.aoeTelegraph = b.value("aoe_telegraph", 1.15f);
            spec.chargeCooldown = b.value("charge_cooldown", 6.5f);
            spec.chargeSpeed = b.value("charge_speed", 310.0f);
            spec.expReward = b.value("exp_reward", 500);
            spec.goldReward = b.value("gold_reward", 500);
            spec.dropItemId = b.value("drop_item_id", "");
            spec.artId = b.value("art_id", 301);
            spec.frameSize = b.value("frame_size", 80);
            spec.frames = b.value("frames", 4);
            m_bossMap[spec.id] = spec;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[DataManager] Error parsing bosses.json: " << e.what() << "\n";
        return false;
    }
}

bool DataManager::LoadNpcs() {
    std::string text;
    if (!ReadFileText("data/npcs.json", text)) return false;
    try {
        json root = json::parse(text);
        m_npcs.clear();
        m_npcMap.clear();
        for (const auto& n : root.at("npcs")) {
            NpcSpec spec;
            spec.id = n.at("id").get<std::string>();
            spec.name = n.value("name", spec.id);
            spec.role = n.value("role", "quest");
            spec.mapId = n.value("map_id", "town");
            spec.tileX = n.at("tile")[0].get<int>();
            spec.tileY = n.at("tile")[1].get<int>();
            spec.artId = n.value("art_id", 401);
            spec.frameSize = n.value("frame_size", 40);
            spec.frames = n.value("frames", 4);
            spec.greeting = n.value("greeting", "");
            if (n.contains("shop_items")) {
                for (const auto& itemId : n.at("shop_items")) {
                    spec.shopItems.push_back(itemId.get<std::string>());
                }
            }
            m_npcMap[spec.id] = m_npcs.size();
            m_npcs.push_back(spec);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[DataManager] Error parsing npcs.json: " << e.what() << "\n";
        return false;
    }
}

bool DataManager::LoadQuests() {
    std::string text;
    if (!ReadFileText("data/quests.json", text)) return false;
    try {
        json root = json::parse(text);
        m_quests.clear();
        m_questMap.clear();
        for (const auto& q : root.at("quests")) {
            QuestSpec spec;
            spec.id = q.at("id").get<std::string>();
            spec.title = q.value("title", spec.id);
            spec.description = q.value("description", "");
            spec.prereqQuestId = q.value("prereq_quest_id", "");
            spec.giverNpcId = q.value("giver_npc_id", "npc_elder");
            for (const auto& obj : q.at("objectives")) {
                QuestObjectiveSpec os;
                os.targetId = obj.at("target_id").get<std::string>();
                os.targetName = obj.value("target_name", os.targetId);
                os.requiredCount = obj.value("required_count", 1);
                spec.objectives.push_back(os);
            }
            spec.rewardGold = q.value("reward_gold", 0);
            spec.rewardExp = q.value("reward_exp", 0);
            spec.rewardItemId = q.value("reward_item_id", "");
            spec.rewardItemCount = q.value("reward_item_count", 0);
            spec.dialogueOffer = q.value("dialogue_offer", "");
            spec.dialogueProgress = q.value("dialogue_progress", "");
            spec.dialogueComplete = q.value("dialogue_complete", "");
            m_questMap[spec.id] = m_quests.size();
            m_quests.push_back(spec);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[DataManager] Error parsing quests.json: " << e.what() << "\n";
        return false;
    }
}

bool DataManager::LoadMaps() {
    std::string text;
    if (!ReadFileText("data/maps.json", text)) return false;
    try {
        json root = json::parse(text);
        for (const auto& m : root.at("maps")) {
            MapSpec spec;
            spec.id = m.at("id").get<std::string>();
            spec.name = m.value("name", spec.id);
            spec.type = m.value("type", "field");
            spec.txtPath = m.at("txt_path").get<std::string>();
            spec.safeZone = m.value("safe_zone", false);
            if (m.contains("ambient_tint") && m["ambient_tint"].size() >= 3) {
                spec.ambientTint = Color{
                    static_cast<unsigned char>(m["ambient_tint"][0].get<int>()),
                    static_cast<unsigned char>(m["ambient_tint"][1].get<int>()),
                    static_cast<unsigned char>(m["ambient_tint"][2].get<int>()),
                    255
                };
            }
            if (m.contains("warps")) {
                for (const auto& w : m.at("warps")) {
                    WarpSpec ws;
                    ws.tileX = w.at("tile")[0].get<int>();
                    ws.tileY = w.at("tile")[1].get<int>();
                    ws.targetMap = w.at("target_map").get<std::string>();
                    ws.targetTileX = w.at("target_tile")[0].get<int>();
                    ws.targetTileY = w.at("target_tile")[1].get<int>();
                    ws.label = w.value("label", "");
                    spec.warps.push_back(ws);
                }
            }
            if (m.contains("monster_spawns")) {
                for (const auto& ms : m.at("monster_spawns")) {
                    MonsterSpawnSpec s;
                    s.monsterId = ms.at("monster_id").get<std::string>();
                    s.tileX = ms.at("tile")[0].get<int>();
                    s.tileY = ms.at("tile")[1].get<int>();
                    spec.monsterSpawns.push_back(s);
                }
            }
            if (m.contains("boss_spawn") && !m["boss_spawn"].is_null()) {
                spec.bossSpawn.hasBoss = true;
                spec.bossSpawn.bossId = m["boss_spawn"].at("boss_id").get<std::string>();
                spec.bossSpawn.tileX = m["boss_spawn"].at("tile")[0].get<int>();
                spec.bossSpawn.tileY = m["boss_spawn"].at("tile")[1].get<int>();
            }
            m_mapMap[spec.id] = spec;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[DataManager] Error parsing maps.json: " << e.what() << "\n";
        return false;
    }
}

const TileSpec* DataManager::GetTileSpec(int tileId) const {
    auto it = m_tileMap.find(tileId);
    return it != m_tileMap.end() ? &it->second : nullptr;
}

const ItemSpec* DataManager::GetItemSpec(const std::string& id) const {
    auto it = m_itemMap.find(id);
    return it != m_itemMap.end() ? &m_items[it->second] : nullptr;
}

const MonsterSpec* DataManager::GetMonsterSpec(const std::string& id) const {
    auto it = m_monsterMap.find(id);
    return it != m_monsterMap.end() ? &it->second : nullptr;
}

const BossSpec* DataManager::GetBossSpec(const std::string& id) const {
    auto it = m_bossMap.find(id);
    return it != m_bossMap.end() ? &it->second : nullptr;
}

const NpcSpec* DataManager::GetNpcSpec(const std::string& id) const {
    auto it = m_npcMap.find(id);
    return it != m_npcMap.end() ? &m_npcs[it->second] : nullptr;
}

const QuestSpec* DataManager::GetQuestSpec(const std::string& id) const {
    auto it = m_questMap.find(id);
    return it != m_questMap.end() ? &m_quests[it->second] : nullptr;
}

static SkillEffectSpec ParseSkillEffect(const json& e) {
    SkillEffectSpec ef;
    ef.type = e.value("type", "");
    ef.target = e.value("target", "self");
    ef.resourceType = e.value("resource_type", "mp");
    ef.radius = e.value("radius", 64.0f);
    ef.coneAngleDeg = e.value("cone_angle_deg", 95.0f);
    ef.distance = e.value("distance", 70.0f);
    ef.atkMultiplier = e.value("atk_multiplier", 1.0f);
    ef.flatAmount = e.value("flat_amount", 0);
    ef.maxHpRatio = e.value("max_hp_ratio", 0.0f);
    ef.knockback = e.value("knockback", 180.0f);
    ef.duration = e.value("duration", 1.0f);
    ef.speed = e.value("speed", 240.0f);
    ef.count = e.value("count", 1);
    ef.spreadDeg = e.value("spread_deg", 20.0f);
    ef.telegraphSec = e.value("telegraph_sec", 0.6f);
    ef.atkBuffMult = e.value("atk_buff_mult", 1.0f);
    ef.speedBuffMult = e.value("speed_buff_mult", 1.0f);
    ef.grantInvuln = e.value("grant_invuln", false);
    ef.artId = e.value("art_id", 602);
    ef.scale = e.value("scale", 1.6f);
    ef.tint = ParseColorRGBA(e, "tint", WHITE);

    if (e.contains("on_hit") && e.at("on_hit").is_array()) {
        for (const auto& sub : e.at("on_hit")) {
            ef.onHitEffects.push_back(ParseSkillEffect(sub));
        }
    }
    if (e.contains("on_explode") && e.at("on_explode").is_array()) {
        for (const auto& sub : e.at("on_explode")) {
            ef.onExplodeEffects.push_back(ParseSkillEffect(sub));
        }
    }
    return ef;
}

bool DataManager::LoadSkills() {
    std::string text;
    if (!ReadFileText("data/skills.json", text)) return false;
    try {
        json root = json::parse(text);
        m_skillConfig = SkillSystemConfig{};
        m_skillConfig.maxEquipSlots = std::clamp(root.value("max_equip_slots", 10), 1, 10);
        m_skillConfig.defaultEquipSlots = std::clamp(root.value("default_equip_slots", 3), 1, m_skillConfig.maxEquipSlots);
        m_skillConfig.slotExpandGoldCost = std::max(0, root.value("slot_expand_gold_cost", 100));
        if (root.contains("default_equipped") && root.at("default_equipped").is_array()) {
            for (const auto& sid : root.at("default_equipped")) {
                m_skillConfig.defaultEquipped.push_back(sid.get<std::string>());
            }
        }

        m_skills.clear();
        m_skillMap.clear();
        for (const auto& s : root.at("skills")) {
            SkillSpec spec;
            spec.id = s.at("id").get<std::string>();
            spec.name = s.value("name", spec.id);
            spec.description = s.value("description", "");
            spec.iconArtId = s.value("icon_art_id", 602);
            spec.costType = ParseSkillCostType(s.value("cost_type", "none"));
            spec.costAmount = std::max(0, s.value("cost_amount", 0));
            spec.cooldown = std::max(0.1f, s.value("cooldown", 3.0f));
            spec.unlockLevel = std::max(1, s.value("unlock_level", 1));
            if (s.contains("effects") && s.at("effects").is_array()) {
                for (const auto& ef : s.at("effects")) {
                    spec.effects.push_back(ParseSkillEffect(ef));
                }
            }
            m_skillMap[spec.id] = m_skills.size();
            m_skills.push_back(std::move(spec));
        }
        return !m_skills.empty();
    } catch (const std::exception& e) {
        std::cerr << "[DataManager] Error parsing skills.json: " << e.what() << "\n";
        return false;
    }
}

const MapSpec* DataManager::GetMapSpec(const std::string& id) const {
    auto it = m_mapMap.find(id);
    return it != m_mapMap.end() ? &it->second : nullptr;
}

const SkillSpec* DataManager::GetSkillSpec(const std::string& id) const {
    auto it = m_skillMap.find(id);
    return it != m_skillMap.end() ? &m_skills[it->second] : nullptr;
}
