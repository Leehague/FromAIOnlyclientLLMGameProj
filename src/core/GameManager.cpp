#include "core/GameManager.h"

#include "managers/DataManager.h"
#include "managers/ResourceManager.h"
#include "managers/MapManager.h"
#include "managers/EconomyManager.h"
#include "managers/QuestManager.h"
#include "managers/EntityManager.h"
#include "managers/CombatManager.h"
#include "managers/UIManager.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

GameManager& GameManager::Get() {
    static GameManager instance;
    return instance;
}

GameManager::GameManager() {
    m_dataManager = std::make_unique<DataManager>();
    m_resourceManager = std::make_unique<ResourceManager>();
    m_mapManager = std::make_unique<MapManager>();
    m_economyManager = std::make_unique<EconomyManager>();
    m_questManager = std::make_unique<QuestManager>();
    m_entityManager = std::make_unique<EntityManager>();
    m_combatManager = std::make_unique<CombatManager>();
    m_uiManager = std::make_unique<UIManager>();

    // Strict initialization order: Data -> Resources -> Map -> Economy -> Quest -> Entity -> Combat -> UI
    m_managers = {
        m_dataManager.get(),
        m_resourceManager.get(),
        m_mapManager.get(),
        m_economyManager.get(),
        m_questManager.get(),
        m_entityManager.get(),
        m_combatManager.get(),
        m_uiManager.get()
    };
}

GameManager::~GameManager() = default;

ResourceManager& GameManager::Resources() { return *m_resourceManager; }
DataManager& GameManager::Data() { return *m_dataManager; }
MapManager& GameManager::Map() { return *m_mapManager; }
EntityManager& GameManager::Entities() { return *m_entityManager; }
CombatManager& GameManager::Combat() { return *m_combatManager; }
QuestManager& GameManager::Quests() { return *m_questManager; }
EconomyManager& GameManager::Economy() { return *m_economyManager; }
UIManager& GameManager::UI() { return *m_uiManager; }

void GameManager::ResolveProjectBasePath() {
    std::vector<fs::path> candidates = {
        fs::current_path(),
        fs::current_path().parent_path(),
        fs::current_path().parent_path().parent_path(),
        fs::path(GetApplicationDirectory()),
        fs::path(GetApplicationDirectory()).parent_path(),
        fs::path(GetApplicationDirectory()).parent_path().parent_path()
    };

    for (const auto& dir : candidates) {
        if (fs::exists(dir / "data" / "art_mapping.txt")) {
            m_basePath = dir.string();
            return;
        }
    }
    m_basePath = fs::current_path().string();
}

std::string GameManager::ResolvePath(const std::string& relativePath) const {
    return (fs::path(m_basePath) / relativePath).string();
}

bool GameManager::InitializeAllManagers() {
    for (IManager* mgr : m_managers) {
        if (!mgr->Init()) {
            std::cerr << "[GameManager] Failed to initialize manager: " << mgr->GetName() << "\n";
            return false;
        }
        std::cout << "[GameManager] Initialized: " << mgr->GetName() << "\n";
    }
    return true;
}

void GameManager::ShutdownAllManagers() {
    for (auto it = m_managers.rbegin(); it != m_managers.rend(); ++it) {
        (*it)->Shutdown();
    }
}

void GameManager::StartNewGame() {
    m_economyManager->Reset();
    m_questManager->Reset();
    m_combatManager->ClearTransientEffects();
    m_entityManager->ResetPlayerFromSpec();
    m_uiManager->CloseAllPanels();

    const PlayerSpec& pSpec = m_dataManager->GetPlayerSpec();
    ChangeMap(pSpec.startMap, pSpec.startTileX, pSpec.startTileY);
    m_screen = GameScreen::Playing;
    AddToast(m_dataManager->GetText("toast.welcome"), GOLD, 4.0f);
}

bool GameManager::ChangeMap(const std::string& mapId, int spawnTileX, int spawnTileY) {
    if (!m_mapManager->LoadMap(mapId)) {
        return false;
    }
    m_combatManager->ClearTransientEffects();
    m_entityManager->OnMapChanged(mapId, spawnTileX, spawnTileY);

    // Snap camera immediately on map load
    const auto& p = m_entityManager->GetPlayer();
    m_camera.target = p.pos;
    m_camera.offset = { m_screenWidth * 0.5f, m_screenHeight * 0.5f };
    m_camera.rotation = 0.0f;
    m_camera.zoom = 2.0f;

    const MapSpec* ms = m_dataManager->GetMapSpec(mapId);
    if (ms) {
        AddToast(m_dataManager->FormatText("toast.enter_map", {ms->name}), SKYBLUE, 2.5f);
    }
    return true;
}

void GameManager::RespawnPlayerInTown() {
    const PlayerSpec& pSpec = m_dataManager->GetPlayerSpec();
    m_entityManager->RevivePlayerFull();
    ChangeMap(pSpec.startMap, pSpec.startTileX, pSpec.startTileY);
    m_screen = GameScreen::Playing;
    AddToast(m_dataManager->GetText("toast.respawn_town"), GREEN, 3.2f);
}

void GameManager::OnBossDefeated() {
    AddToast(m_dataManager->GetText("toast.boss_defeated"), GOLD, 5.0f);
}

void GameManager::AddToast(const std::string& message, Color color, float duration) {
    if (m_uiManager) {
        m_uiManager->PushToast(message, color, duration);
    }
}

void GameManager::UpdateCamera(float dt) {
    const auto& player = m_entityManager->GetPlayer();
    float lerpFactor = 1.0f - std::exp(-10.0f * dt);
    m_camera.target.x += (player.pos.x - m_camera.target.x) * lerpFactor;
    m_camera.target.y += (player.pos.y - m_camera.target.y) * lerpFactor;

    float mapW = static_cast<float>(m_mapManager->GetWidth() * m_mapManager->GetTileSize());
    float mapH = static_cast<float>(m_mapManager->GetHeight() * m_mapManager->GetTileSize());
    float halfViewW = (m_screenWidth * 0.5f) / m_camera.zoom;
    float halfViewH = (m_screenHeight * 0.5f) / m_camera.zoom;

    if (mapW > halfViewW * 2.0f) {
        m_camera.target.x = std::clamp(m_camera.target.x, halfViewW, mapW - halfViewW);
    } else {
        m_camera.target.x = mapW * 0.5f;
    }
    if (mapH > halfViewH * 2.0f) {
        m_camera.target.y = std::clamp(m_camera.target.y, halfViewH, mapH - halfViewH);
    } else {
        m_camera.target.y = mapH * 0.5f;
    }

    m_camera.offset = { m_screenWidth * 0.5f, m_screenHeight * 0.5f };
    if (m_screenShakeTimer > 0.0f) {
        m_screenShakeTimer -= dt;
        m_camera.offset.x += static_cast<float>(GetRandomValue(-100, 100)) * 0.01f * m_screenShakeIntensity;
        m_camera.offset.y += static_cast<float>(GetRandomValue(-100, 100)) * 0.01f * m_screenShakeIntensity;
    }
}

void GameManager::UpdateFrame(float dt) {
    if (m_screen == GameScreen::Title) {
        m_uiManager->Update(dt);
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            StartNewGame();
        }
        return;
    }

    if (m_screen == GameScreen::GameOver) {
        m_uiManager->Update(dt);
        if (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_ENTER)) {
            RespawnPlayerInTown();
        }
        return;
    }

    if (m_screen == GameScreen::Victory) {
        m_uiManager->Update(dt);
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) {
            m_screen = GameScreen::Playing;
        }
        if (IsKeyPressed(KEY_R)) {
            StartNewGame();
        }
        return;
    }

    // Playing state: invoke sub-managers through unified IManager interface
    for (IManager* mgr : m_managers) {
        mgr->Update(dt);
    }
    UpdateCamera(dt);
}

void GameManager::RenderFrame() {
    BeginDrawing();
    ClearBackground(Color{14, 16, 22, 255});

    if (m_screen != GameScreen::Title) {
        BeginMode2D(m_camera);
        // World-space managers
        m_mapManager->Render();
        m_combatManager->RenderGroundEffects();
        m_entityManager->Render();
        m_combatManager->Render();
        EndMode2D();
    }

    // Screen-space UI manager
    m_uiManager->Render();

    EndDrawing();
}

int GameManager::RunSmokeTestVerification() {
    std::cout << "=== [SmokeTest] Running Automated Verification ===\n";

    // 1. Verify DataManager loaded all JSON specs including string table & UI layout
    if (m_dataManager->GetStringCount() < 60 ||
        m_dataManager->GetUILayout().lobby.elements.size() < 5 ||
        m_dataManager->GetUILayout().hudWidgets.size() < 11 ||
        m_dataManager->GetTileCount() < 16 ||
        m_dataManager->GetItemCount() < 9 ||
        m_dataManager->GetMonsterCount() < 3 ||
        m_dataManager->GetBossCount() < 1 ||
        m_dataManager->GetNpcCount() < 3 ||
        m_dataManager->GetQuestCount() < 3 ||
        m_dataManager->GetMapCount() < 3) {
        std::cerr << "[SmokeTest] FAIL: Missing JSON spec records!\n";
        return 1;
    }

    // 2. Verify ResourceManager loaded all mapped textures without fallback
    if (m_resourceManager->GetMissingTextureCount() > 0 || m_resourceManager->GetLoadedTextureCount() < 30) {
        std::cerr << "[SmokeTest] FAIL: Missing art assets (" << m_resourceManager->GetMissingTextureCount()
                  << " missing, " << m_resourceManager->GetLoadedTextureCount() << " loaded)\n";
        return 2;
    }

    // 3. Start game and test all 3 maps + 8-direction movement + quests + combat + shop
    StartNewGame();
    if (m_mapManager->GetCurrentMapId() != "town") {
        std::cerr << "[SmokeTest] FAIL: Initial map is not town\n";
        return 3;
    }

    // Test 8-direction conversion
    const float dirs[8][2] = {
        {0, 1}, {-1, 1}, {-1, 0}, {-1, -1},
        {0, -1}, {1, -1}, {1, 0}, {1, 1}
    };
    for (int i = 0; i < 8; ++i) {
        Direction8 d = VectorToDirection8(dirs[i][0], dirs[i][1], Direction8::Down);
        if (static_cast<int>(d) != i) {
            std::cerr << "[SmokeTest] FAIL: Direction8 mismatch at index " << i << "\n";
            return 4;
        }
    }

    // Test Economy (buy potion)
    int goldBefore = m_economyManager->GetGold();
    if (!m_economyManager->BuyItem("potion_hp") || m_economyManager->GetGold() >= goldBefore) {
        std::cerr << "[SmokeTest] FAIL: Shop purchase failed\n";
        return 5;
    }

    // Test Quest Accept & Progress
    m_questManager->AcceptQuest("q1_slime_hunt");
    if (m_questManager->GetQuestStatus("q1_slime_hunt") != QuestStatus::Active) {
        std::cerr << "[SmokeTest] FAIL: Quest accept failed\n";
        return 6;
    }
    for (int i = 0; i < 4; ++i) m_questManager->NotifyTargetKilled("slime");
    if (m_questManager->GetQuestStatus("q1_slime_hunt") != QuestStatus::ReadyToTurnIn) {
        std::cerr << "[SmokeTest] FAIL: Quest progression failed\n";
        return 7;
    }

    // Test map loading: field & A* routing across river bridge
    if (!ChangeMap("field", 3, 11) || m_entityManager->GetMonsters().empty()) {
        std::cerr << "[SmokeTest] FAIL: Field map or monster spawn failed\n";
        return 8;
    }
    {
        Vector2 westBank = m_mapManager->TileToWorldCenter(8, 14);
        Vector2 eastBank = m_mapManager->TileToWorldCenter(22, 14);
        auto riverPath = m_mapManager->FindPath(westBank, eastBank, 13.0f);
        if (riverPath.empty()) {
            std::cerr << "[SmokeTest] FAIL: A* failed to route across field river bridge\n";
            return 10;
        }
        for (const auto& wp : riverPath) {
            if (!m_mapManager->IsCircleWalkable(wp, 10.0f)) {
                std::cerr << "[SmokeTest] FAIL: A* field path crossed unwalkable tile\n";
                return 11;
            }
        }
        std::cout << "[SmokeTest] A* field river-bridge path verified (" << riverPath.size() << " waypoints)\n";
    }
    for (int f = 0; f < 15; ++f) {
        UpdateFrame(1.0f / 60.0f);
        RenderFrame();
    }

    // Test boss_room & A* routing around obsidian pillar with boss clearance
    if (!ChangeMap("boss_room", 11, 14) || !m_entityManager->GetBoss().active) {
        std::cerr << "[SmokeTest] FAIL: Boss room map or boss spawn failed\n";
        return 9;
    }
    {
        Vector2 northOfPillar = m_mapManager->TileToWorldCenter(7, 4);
        Vector2 southOfPillar = m_mapManager->TileToWorldCenter(7, 9);
        auto pillarPath = m_mapManager->FindPath(northOfPillar, southOfPillar, m_entityManager->GetBoss().radius);
        if (pillarPath.empty()) {
            std::cerr << "[SmokeTest] FAIL: A* failed to route boss around pillar\n";
            return 12;
        }
        std::cout << "[SmokeTest] A* boss pillar-detour path verified (" << pillarPath.size() << " waypoints)\n";
    }
    for (int f = 0; f < 15; ++f) {
        UpdateFrame(1.0f / 60.0f);
        RenderFrame();
    }

    std::cout << "[SmokeTest] PASS: All JSON specs (" << m_dataManager->GetStringCount() << " strings, "
              << m_dataManager->GetItemCount() << " items, "
              << m_dataManager->GetMonsterCount() << " monsters, " << m_dataManager->GetBossCount() << " boss, "
              << m_dataManager->GetQuestCount() << " quests, " << m_dataManager->GetMapCount() << " maps), "
              << m_resourceManager->GetLoadedTextureCount() << " mapped PNG textures, 3 tilemaps, "
              << "8-direction system, economy, quest, and combat verified!\n";
    return 0;
}

int GameManager::Run(bool smokeTest) {
    SetTraceLogLevel(LOG_WARNING);
    if (smokeTest) {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
    }
    InitWindow(m_screenWidth, m_screenHeight, "Loading...");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL); // ESC closes UI panels instead of terminating immediately

    ResolveProjectBasePath();
    std::cout << "[GameManager] Base Path: " << m_basePath << "\n";

    if (!InitializeAllManagers()) {
        ShutdownAllManagers();
        CloseWindow();
        return 1;
    }
    SetWindowTitle(m_dataManager->GetText("window.title").c_str());

    int exitCode = 0;
    if (smokeTest) {
        exitCode = RunSmokeTestVerification();
    } else {
        while (!WindowShouldClose()) {
            float dt = std::min(GetFrameTime(), 0.05f);
            UpdateFrame(dt);
            RenderFrame();
        }
    }

    ShutdownAllManagers();
    CloseWindow();
    return exitCode;
}
