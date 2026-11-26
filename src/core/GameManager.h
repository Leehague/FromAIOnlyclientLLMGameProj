#pragma once

#include "core/IManager.h"
#include "core/Types.h"
#include <memory>
#include <string>
#include <vector>

class ResourceManager;
class DataManager;
class MapManager;
class EntityManager;
class CombatManager;
class QuestManager;
class EconomyManager;
class InputManager;
class SaveManager;
class UIManager;

// [Requirement 9] Singleton Unified Manager (Singleton + Facade)
// Owns all abstracted IManager sub-managers and coordinates their unified lifecycle.
class GameManager {
public:
    static GameManager& Get();

    GameManager(const GameManager&) = delete;
    GameManager& operator=(const GameManager&) = delete;
    GameManager(GameManager&&) = delete;
    GameManager& operator=(GameManager&&) = delete;

    // Main entry point (interactive mode or automated verification mode)
    int Run(bool smokeTest = false);

    // Sub-manager accessors
    ResourceManager& Resources();
    DataManager& Data();
    MapManager& Map();
    EntityManager& Entities();
    CombatManager& Combat();
    QuestManager& Quests();
    EconomyManager& Economy();
    InputManager& Input();
    SaveManager& Save();
    UIManager& UI();

    // High-level game flow & shared state
    void StartNewGame(const std::string& customPlayerName = "");
    bool ContinueSavedGame(int slot = 1, bool silent = false);
    bool ChangeMap(const std::string& mapId, int spawnTileX, int spawnTileY, bool autoSave = false);
    void SnapCameraToPlayer();
    void RespawnPlayerInTown();
    void OnBossDefeated();
    void AddToast(const std::string& message, Color color = GOLD, float duration = 2.8f);
    void RequestExitDialog();
    void ConfirmQuit(bool saveBeforeQuit, int slot = 1);
    bool ShouldQuit() const { return m_shouldQuit; }

    GameScreen GetScreen() const { return m_screen; }
    void SetScreen(GameScreen s) { m_screen = s; }

    const Camera2D& GetCamera() const { return m_camera; }
    const std::string& GetBasePath() const { return m_basePath; }
    std::string ResolvePath(const std::string& relativePath) const;

    int GetScreenWidthPx() const { return m_screenWidth; }
    int GetScreenHeightPx() const { return m_screenHeight; }

private:
    GameManager();
    ~GameManager();

    void ResolveProjectBasePath();
    bool InitializeAllManagers();
    void ShutdownAllManagers();
    void UpdateFrame(float dt);
    void RenderFrame();
    void UpdateCamera(float dt);
    int RunSmokeTestVerification();

    int m_screenWidth = 1280;
    int m_screenHeight = 720;
    GameScreen m_screen = GameScreen::Title;
    bool m_shouldQuit = false;
    std::string m_basePath;
    Camera2D m_camera{};
    float m_screenShakeTimer = 0.0f;
    float m_screenShakeIntensity = 0.0f;

    // Concrete sub-managers owned by the single GameManager
    std::unique_ptr<DataManager> m_dataManager;
    std::unique_ptr<ResourceManager> m_resourceManager;
    std::unique_ptr<MapManager> m_mapManager;
    std::unique_ptr<EconomyManager> m_economyManager;
    std::unique_ptr<QuestManager> m_questManager;
    std::unique_ptr<EntityManager> m_entityManager;
    std::unique_ptr<CombatManager> m_combatManager;
    std::unique_ptr<InputManager> m_inputManager;
    std::unique_ptr<SaveManager> m_saveManager;
    std::unique_ptr<UIManager> m_uiManager;

    // Ordered polymorphic registry for unified IManager lifecycle orchestration
    std::vector<IManager*> m_managers;
};
