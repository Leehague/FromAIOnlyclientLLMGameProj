#pragma once

#include <raylib.h>
#include <string>
#include <vector>
#include <unordered_map>

// 8-direction movement & animation enum (matches row index in player sprite sheets)
enum class Direction8 : int {
    Down = 0,       // S  (↓)
    DownLeft = 1,   // SW (↙)
    Left = 2,       // W  (←)
    UpLeft = 3,     // NW (↖)
    Up = 4,         // N  (↑)
    UpRight = 5,    // NE (↗)
    Right = 6,      // E  (→)
    DownRight = 7   // SE (↘)
};

enum class AnimState {
    Idle,
    Walk,
    Attack
};

enum class GameScreen {
    Title,
    Playing,
    GameOver,
    Victory
};

enum class ItemType {
    Consumable,
    Weapon,
    Armor,
    Quest
};

enum class QuestStatus {
    NotStarted,
    Active,
    ReadyToTurnIn,
    Completed
};

enum class InputAction : int {
    MoveUp = 0,
    MoveDown,
    MoveLeft,
    MoveRight,
    Attack,
    Skill,
    Dash,
    Interact,
    Potion1,
    Potion2,
    Inventory,
    QuestLog,
    QuickSave,
    Settings,
    Count
};

struct KeyBindingEntry {
    InputAction action = InputAction::MoveUp;
    std::string actionId;
    std::string labelKey;
    int primaryKey = 0;
    int secondaryKey = 0;
};

// ---------------- Data Spec Structs (Loaded from JSON) ----------------

struct TileSpec {
    int id = 0;
    std::string key;
    std::string name;
    int artId = 0;
    bool walkable = true;
    std::string special; // "none", "solid", "water", "lava", "heal", "portal"
    std::string conceptDesc;
};

struct ItemSpec {
    std::string id;
    std::string name;
    ItemType type = ItemType::Consumable;
    int price = 0;
    int healHp = 0;
    int atkBonus = 0;
    int defBonus = 0;
    int hpBonus = 0;
    int artId = 0;
    std::string description;
};

struct PlayerAnimSpec {
    int idleArtId = 101;
    int walkArtId = 102;
    int attackArtId = 103;
    int frameWidth = 48;
    int frameHeight = 48;
    int framesPerRow = 4;
    int directions = 8;
    float idleFps = 4.5f;
    float walkFps = 10.0f;
    float attackFps = 15.0f;
};

struct StartItemEntry {
    std::string itemId;
    int count = 1;
};

struct PlayerSpec {
    std::string name;
    std::string startMap = "town";
    int startTileX = 11;
    int startTileY = 13;
    int startLevel = 1;
    int maxLevel = 10;
    int baseMaxHp = 120;
    int baseAtk = 16;
    int baseDef = 4;
    int hpPerLevel = 25;
    int atkPerLevel = 4;
    int defPerLevel = 2;
    float moveSpeed = 145.0f;
    float dashSpeed = 390.0f;
    float dashDuration = 0.22f;
    float dashCooldown = 1.6f;
    float attackRange = 46.0f;
    float attackCooldown = 0.34f;
    float attackDuration = 0.26f;
    std::string skillName;
    float skillRadius = 72.0f;
    float skillMultiplier = 1.85f;
    float skillCooldown = 3.8f;
    int startGold = 60;
    std::vector<StartItemEntry> startItems;
    PlayerAnimSpec anim;
};

struct MonsterSpec {
    std::string id;
    std::string name;
    int maxHp = 50;
    int atk = 10;
    int def = 2;
    float moveSpeed = 65.0f;
    float detectRange = 160.0f;
    float attackRange = 30.0f;
    float attackCooldown = 1.2f;
    int expReward = 30;
    int goldReward = 20;
    float respawnTime = 12.0f;
    int artId = 201;
    int frameSize = 40;
    int frames = 4;
    bool ranged = false;
};

struct BossSpec {
    std::string id;
    std::string name;
    std::string title;
    int maxHp = 750;
    int atk = 30;
    int def = 9;
    float moveSpeed = 76.0f;
    float enrageSpeed = 106.0f;
    float enrageHpRatio = 0.5f;
    float attackRange = 52.0f;
    float attackCooldown = 1.15f;
    float fireballCooldown = 4.0f;
    int fireballCountP1 = 8;
    int fireballCountP2 = 12;
    float fireballSpeed = 155.0f;
    float aoeCooldown = 5.2f;
    float aoeRadius = 54.0f;
    float aoeTelegraph = 1.15f;
    float chargeCooldown = 6.5f;
    float chargeSpeed = 310.0f;
    int expReward = 500;
    int goldReward = 500;
    std::string dropItemId;
    int artId = 301;
    int frameSize = 80;
    int frames = 4;
};

struct NpcSpec {
    std::string id;
    std::string name;
    std::string role; // "quest", "shop", "healer"
    std::string mapId;
    int tileX = 0;
    int tileY = 0;
    int artId = 401;
    int frameSize = 40;
    int frames = 4;
    std::string greeting;
    std::vector<std::string> shopItems;
};

struct QuestObjectiveSpec {
    std::string targetId;
    std::string targetName;
    int requiredCount = 1;
};

struct QuestSpec {
    std::string id;
    std::string title;
    std::string description;
    std::string prereqQuestId;
    std::string giverNpcId;
    std::vector<QuestObjectiveSpec> objectives;
    int rewardGold = 0;
    int rewardExp = 0;
    std::string rewardItemId;
    int rewardItemCount = 0;
    std::string dialogueOffer;
    std::string dialogueProgress;
    std::string dialogueComplete;
};

struct WarpSpec {
    int tileX = 0;
    int tileY = 0;
    std::string targetMap;
    int targetTileX = 0;
    int targetTileY = 0;
    std::string label;
};

struct MonsterSpawnSpec {
    std::string monsterId;
    int tileX = 0;
    int tileY = 0;
};

struct BossSpawnSpec {
    bool hasBoss = false;
    std::string bossId;
    int tileX = 0;
    int tileY = 0;
};

struct MapSpec {
    std::string id;
    std::string name;
    std::string type; // "town", "field", "boss"
    std::string txtPath;
    bool safeZone = false;
    Color ambientTint = WHITE;
    std::vector<WarpSpec> warps;
    std::vector<MonsterSpawnSpec> monsterSpawns;
    BossSpawnSpec bossSpawn;
};

// ---------------- Data-Driven UI Layout Specs (data/ui_layout.json) ----------------

struct LobbyElementSpec {
    std::string id;
    std::string label;
    std::string type;   // "sprite", "text", "guide_card", "button", "panel"
    std::string action; // e.g. "start_game"
    bool visible = true;
    float x = 0.0f;
    float y = 0.0f;
    float width = 100.0f;
    float height = 40.0f;
    int artId = 0;
    int frameW = 0;
    int frameH = 0;
    std::string textKey;
    std::string text; // Optional direct text override
    std::vector<std::string> textKeys;
    float fontSize = 18.0f;
    std::string align = "center"; // "left", "center", "right"
    Color color = WHITE;
    Color bgColor = Color{15, 18, 28, 210};
    Color hoverColor = Color{185, 65, 55, 255};
    Color borderColor = Color{160, 140, 95, 200};
};

struct LobbyLayoutSpec {
    Color bgTopColor = Color{18, 22, 34, 255};
    Color bgBottomColor = Color{38, 20, 26, 255};
    std::vector<LobbyElementSpec> elements;
};

struct HudWidgetSpec {
    std::string id;
    std::string label;
    std::string group = "hud"; // "hud" or "popup"
    bool visible = true;
    float x = 0.0f;
    float y = 0.0f;
    float width = 200.0f;
    float height = 100.0f;
    Color bgColor = Color{14, 16, 24, 215};
    Color borderColor = Color{130, 118, 92, 220};
    Color accentColor = GOLD;
    float cellSize = 5.0f; // used by minimap
};

struct UILayoutSpec {
    int refWidth = 1280;
    int refHeight = 720;
    LobbyLayoutSpec lobby;
    std::vector<HudWidgetSpec> hudWidgets;
    std::unordered_map<std::string, size_t> hudWidgetIndex;
};

// ---------------- Runtime Entity & Effect Structs ----------------

struct PlayerEntity {
    std::string name;
    Vector2 pos{0.0f, 0.0f};
    Vector2 facingVec{0.0f, 1.0f};
    Direction8 dir = Direction8::Down;
    AnimState animState = AnimState::Idle;
    float animTimer = 0.0f;
    int animFrame = 0;

    int level = 1;
    int exp = 0;
    int hp = 120;
    int maxHp = 120;
    int atk = 16;
    int def = 4;

    float invulnTimer = 0.0f;
    float hitFlashTimer = 0.0f;

    float attackTimer = 0.0f;
    float attackCooldownTimer = 0.0f;
    bool attackHitApplied = false;

    float skillCooldownTimer = 0.0f;
    float dashTimer = 0.0f;
    float dashCooldownTimer = 0.0f;
    Vector2 dashDir{0.0f, 1.0f};

    float warpCooldownTimer = 0.0f;
    float fountainHealTimer = 0.0f;
    float radius = 11.0f;
};

struct NpcEntity {
    std::string specId;
    Vector2 pos{0.0f, 0.0f};
    float animTimer = 0.0f;
    int animFrame = 0;
    float radius = 13.0f;
};

struct MonsterEntity {
    int instanceId = 0;
    std::string specId;
    Vector2 spawnPos{0.0f, 0.0f};
    Vector2 pos{0.0f, 0.0f};
    Vector2 knockbackVel{0.0f, 0.0f};
    Vector2 patrolTarget{0.0f, 0.0f};
    float patrolWaitTimer = 0.0f;
    std::vector<Vector2> path;
    float repathTimer = 0.0f;

    int hp = 50;
    int maxHp = 50;
    bool alive = true;
    float respawnTimer = 0.0f;
    float attackCooldownTimer = 0.0f;
    float hitFlashTimer = 0.0f;
    float animTimer = 0.0f;
    int animFrame = 0;
    bool facingLeft = false;
    float radius = 13.0f;
};

struct BossEntity {
    bool active = false;
    std::string specId;
    Vector2 spawnPos{0.0f, 0.0f};
    Vector2 pos{0.0f, 0.0f};
    Vector2 knockbackVel{0.0f, 0.0f};
    std::vector<Vector2> path;
    float repathTimer = 0.0f;

    int hp = 750;
    int maxHp = 750;
    bool alive = false;
    bool defeatedOnce = false;
    bool enraged = false;

    float attackCooldownTimer = 0.0f;
    float fireballTimer = 2.5f;
    float aoeTimer = 4.0f;
    float chargeTimer = 5.5f;
    float chargeTelegraphTimer = 0.0f;
    float chargeActiveTimer = 0.0f;
    Vector2 chargeDir{0.0f, 1.0f};
    float fireballAngleOffset = 0.0f;

    float hitFlashTimer = 0.0f;
    float animTimer = 0.0f;
    int animFrame = 0;
    float radius = 26.0f;
};

struct Projectile {
    Vector2 pos{0.0f, 0.0f};
    Vector2 vel{0.0f, 0.0f};
    int damage = 15;
    float radius = 9.0f;
    float lifetime = 3.5f;
    bool fromPlayer = false;
    int artId = 603;
    float animTimer = 0.0f;
};

struct TelegraphZone {
    Vector2 pos{0.0f, 0.0f};
    float radius = 50.0f;
    float timer = 0.0f;
    float duration = 1.15f;
    int damage = 35;
    bool triggered = false;
};

struct VisualEffect {
    Vector2 pos{0.0f, 0.0f};
    float rotationDeg = 0.0f;
    float scale = 1.0f;
    int artId = 601;
    int frames = 4;
    float timer = 0.0f;
    float duration = 0.28f;
    Color tint = WHITE;
};

struct FloatingText {
    Vector2 pos{0.0f, 0.0f};
    std::string text;
    Color color = WHITE;
    float timer = 0.0f;
    float duration = 0.85f;
    float fontSize = 18.0f;
};

struct InventorySlot {
    std::string itemId;
    int count = 0;
};

struct QuestProgress {
    std::string questId;
    QuestStatus status = QuestStatus::NotStarted;
    std::vector<int> counts;
};

inline Vector2 Direction8ToVector(Direction8 d) {
    constexpr float kDiag = 0.70710678f;
    switch (d) {
        case Direction8::Down:      return { 0.0f,  1.0f };
        case Direction8::DownLeft:  return { -kDiag, kDiag };
        case Direction8::Left:      return { -1.0f, 0.0f };
        case Direction8::UpLeft:    return { -kDiag, -kDiag };
        case Direction8::Up:        return { 0.0f, -1.0f };
        case Direction8::UpRight:   return {  kDiag, -kDiag };
        case Direction8::Right:     return {  1.0f, 0.0f };
        case Direction8::DownRight: return {  kDiag,  kDiag };
    }
    return { 0.0f, 1.0f };
}

inline Direction8 VectorToDirection8(float dx, float dy, Direction8 fallback) {
    if (dx == 0.0f && dy == 0.0f) return fallback;
    if (dx == 0.0f && dy > 0.0f) return Direction8::Down;
    if (dx < 0.0f && dy > 0.0f)  return Direction8::DownLeft;
    if (dx < 0.0f && dy == 0.0f) return Direction8::Left;
    if (dx < 0.0f && dy < 0.0f)  return Direction8::UpLeft;
    if (dx == 0.0f && dy < 0.0f) return Direction8::Up;
    if (dx > 0.0f && dy < 0.0f)  return Direction8::UpRight;
    if (dx > 0.0f && dy == 0.0f) return Direction8::Right;
    return Direction8::DownRight;
}

inline const char* Direction8Key(Direction8 d) {
    switch (d) {
        case Direction8::Down:      return "dir.down";
        case Direction8::DownLeft:  return "dir.down_left";
        case Direction8::Left:      return "dir.left";
        case Direction8::UpLeft:    return "dir.up_left";
        case Direction8::Up:        return "dir.up";
        case Direction8::UpRight:   return "dir.up_right";
        case Direction8::Right:     return "dir.right";
        case Direction8::DownRight: return "dir.down_right";
    }
    return "dir.down";
}
