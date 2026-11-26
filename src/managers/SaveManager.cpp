#include "managers/SaveManager.h"
#include "managers/DataManager.h"
#include "managers/MapManager.h"
#include "managers/EconomyManager.h"
#include "managers/QuestManager.h"
#include "managers/EntityManager.h"
#include "managers/CombatManager.h"
#include "managers/UIManager.h"
#include "core/GameManager.h"

#include <nlohmann/json.hpp>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

bool SaveManager::Init() {
    m_playTimeSec = 0.0f;
    m_manualSaveCooldown = 0.0f;
    return true;
}

void SaveManager::Update(float dt) {
    auto& gm = GameManager::Get();
    if (gm.GetScreen() != GameScreen::Playing) return;

    m_playTimeSec += dt;
    if (m_manualSaveCooldown > 0.0f) {
        m_manualSaveCooldown -= dt;
    }

    // Manual quick-save hotkey (F5)
    if (IsKeyPressed(KEY_F5) && m_manualSaveCooldown <= 0.0f && !gm.UI().IsBlockingGameplayInput()) {
        m_manualSaveCooldown = 1.0f;
        SaveGame(1, false);
    }
}

fs::path SaveManager::GetSavesDir() const {
    return fs::path(GameManager::Get().GetBasePath()) / "saves";
}

fs::path SaveManager::GetSlotPath(int slot) const {
    return GetSavesDir() / ("slot" + std::to_string(slot) + ".json");
}

fs::path SaveManager::GetBackupPath(int slot) const {
    return GetSavesDir() / ("slot" + std::to_string(slot) + ".json.bak");
}

std::string SaveManager::GetCurrentTimestamp() const {
    std::time_t now = std::time(nullptr);
    std::tm tmBuf{};
#ifdef _WIN32
    localtime_s(&tmBuf, &now);
#else
    localtime_r(&now, &tmBuf);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmBuf);
    return std::string(buf);
}

bool SaveManager::HasSaveFile(int slot) const {
    std::error_code ec;
    return fs::exists(GetSlotPath(slot), ec) || fs::exists(GetBackupPath(slot), ec);
}

void SaveManager::TriggerAutoSave() {
    if (!m_autoSaveEnabled) return;
    if (GameManager::Get().GetScreen() != GameScreen::Playing) return;
    if (GameManager::Get().Entities().GetPlayer().hp <= 0) return;
    SaveGame(1, true);
}

bool SaveManager::SaveGame(int slot, bool silent) {
    auto& gm = GameManager::Get();
    if (gm.Entities().GetPlayer().hp <= 0) {
        return false;
    }

    try {
        // Capture Memento snapshots from each stateful sub-manager
        json root;
        root["save_version"] = kSaveVersion;
        root["saved_at"] = GetCurrentTimestamp();
        root["play_time_sec"] = m_playTimeSec;
        root["map_id"] = gm.Map().GetCurrentMapId();
        root["player"] = gm.Entities().SerializePlayer();
        root["world"] = gm.Entities().SerializeWorld();
        root["economy"] = gm.Economy().Serialize();
        root["quests"] = gm.Quests().Serialize();

        fs::path dir = GetSavesDir();
        std::error_code ec;
        fs::create_directories(dir, ec);

        fs::path target = GetSlotPath(slot);
        fs::path tmpPath = target.string() + ".tmp";
        fs::path bakPath = GetBackupPath(slot);

        // 1. Write full JSON to temporary file first
        {
            std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
            if (!out.is_open()) {
                std::cerr << "[SaveManager] Failed to open temp save file: " << tmpPath << "\n";
                if (!silent) gm.AddToast(gm.Data().GetText("toast.save_failed"), RED, 2.5f);
                return false;
            }
            out << root.dump(2) << "\n";
            out.flush();
            if (!out.good()) {
                std::cerr << "[SaveManager] Failed to write temp save file: " << tmpPath << "\n";
                if (!silent) gm.AddToast(gm.Data().GetText("toast.save_failed"), RED, 2.5f);
                return false;
            }
        }

        // 2. Backup existing primary save file before replacing
        if (fs::exists(target, ec)) {
            fs::copy_file(target, bakPath, fs::copy_options::overwrite_existing, ec);
        }

        // 3. Atomic swap (.tmp -> target)
        ec.clear();
        fs::rename(tmpPath, target, ec);
        if (ec) {
            // Fallback for filesystems where rename does not replace existing target
            fs::remove(target, ec);
            ec.clear();
            fs::rename(tmpPath, target, ec);
            if (ec) {
                std::cerr << "[SaveManager] Failed to finalize save file: " << ec.message() << "\n";
                if (!silent) gm.AddToast(gm.Data().GetText("toast.save_failed"), RED, 2.5f);
                return false;
            }
        }

        if (!silent) {
            gm.AddToast(gm.Data().FormatText("toast.game_saved", {std::to_string(slot)}), LIME, 2.4f);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[SaveManager] SaveGame exception: " << e.what() << "\n";
        if (!silent) gm.AddToast(gm.Data().GetText("toast.save_failed"), RED, 2.5f);
        return false;
    }
}

bool SaveManager::LoadGame(int slot, bool silent) {
    auto& gm = GameManager::Get();
    fs::path primary = GetSlotPath(slot);
    fs::path backup = GetBackupPath(slot);

    auto tryReadJson = [](const fs::path& p, json& outJson) -> bool {
        std::error_code ec;
        if (!fs::exists(p, ec)) return false;
        std::ifstream in(p, std::ios::binary);
        if (!in.is_open()) return false;
        try {
            in >> outJson;
            return outJson.is_object();
        } catch (...) {
            return false;
        }
    };

    json root;
    if (!tryReadJson(primary, root)) {
        if (!tryReadJson(backup, root)) {
            if (!silent) gm.AddToast(gm.Data().GetText("toast.no_save_file"), ORANGE, 2.4f);
            return false;
        }
        std::cout << "[SaveManager] Primary save unreadable; recovered from backup: " << backup << "\n";
    }

    try {
        // Restore state in strict dependency order without triggering intermediate auto-saves
        bool prevAuto = m_autoSaveEnabled;
        m_autoSaveEnabled = false;

        gm.Combat().ClearTransientEffects();
        gm.UI().CloseAllPanels();

        // 1. Economy first (equipped weapon/armor needed for player stat recalculation)
        if (root.contains("economy")) {
            gm.Economy().Deserialize(root.at("economy"));
        } else {
            gm.Economy().Reset();
        }

        // 2. Quests (ID-keyed restoration; newly added quests stay NotStarted)
        if (root.contains("quests")) {
            gm.Quests().Deserialize(root.at("quests"));
        } else {
            gm.Quests().Reset();
        }

        // 3. World flags before ChangeMap so Boss defeated state is respected
        gm.Entities().ResetPlayerFromSpec();
        if (root.contains("world")) {
            gm.Entities().DeserializeWorld(root.at("world"));
        }

        // 4. Resolve map (fallback to player start map if saved map was removed)
        const auto& pSpec = gm.Data().GetPlayerSpec();
        std::string mapId = root.value("map_id", pSpec.startMap);
        if (!gm.Data().GetMapSpec(mapId)) {
            mapId = pSpec.startMap;
        }
        if (!gm.ChangeMap(mapId, pSpec.startTileX, pSpec.startTileY, false)) {
            m_autoSaveEnabled = prevAuto;
            return false;
        }

        // 5. Restore player position, facing, level, exp, hp & recalculate derived stats
        if (root.contains("player")) {
            gm.Entities().DeserializePlayer(root.at("player"));
        }

        m_playTimeSec = root.value("play_time_sec", 0.0f);
        gm.SnapCameraToPlayer();
        gm.SetScreen(GameScreen::Playing);
        m_autoSaveEnabled = prevAuto;

        if (!silent) {
            gm.AddToast(gm.Data().FormatText("toast.game_loaded", {std::to_string(slot)}), SKYBLUE, 2.6f);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[SaveManager] LoadGame exception: " << e.what() << "\n";
        m_autoSaveEnabled = true;
        return false;
    }
}

bool SaveManager::DeleteSave(int slot) {
    std::error_code ec;
    bool removed = false;
    if (fs::remove(GetSlotPath(slot), ec)) removed = true;
    fs::remove(GetBackupPath(slot), ec);
    fs::remove(GetSlotPath(slot).string() + ".tmp", ec);
    return removed;
}
