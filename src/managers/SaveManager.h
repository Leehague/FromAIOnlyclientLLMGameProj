#pragma once

#include "core/IManager.h"
#include <filesystem>
#include <string>

// JSON-based client-only persistence manager using Memento / Snapshot pattern
// with atomic file writes (.tmp -> .bak -> rename) and backup fallback.
class SaveManager : public IManager {
public:
    static constexpr int kSaveVersion = 1;

    const char* GetName() const override { return "SaveManager"; }
    bool Init() override;
    void Update(float dt) override;
    void Render() override {}
    void Shutdown() override {}

    bool HasSaveFile(int slot = 1) const;
    bool SaveGame(int slot = 1, bool silent = false);
    bool LoadGame(int slot = 1, bool silent = false);
    bool DeleteSave(int slot = 1);

    void SetAutoSaveEnabled(bool enabled) { m_autoSaveEnabled = enabled; }
    bool IsAutoSaveEnabled() const { return m_autoSaveEnabled; }
    void TriggerAutoSave();

    float GetPlayTimeSec() const { return m_playTimeSec; }
    void ResetPlayTime() { m_playTimeSec = 0.0f; }

private:
    std::filesystem::path GetSavesDir() const;
    std::filesystem::path GetSlotPath(int slot) const;
    std::filesystem::path GetBackupPath(int slot) const;
    std::string GetCurrentTimestamp() const;

    bool m_autoSaveEnabled = true;
    float m_playTimeSec = 0.0f;
    float m_manualSaveCooldown = 0.0f;
};
