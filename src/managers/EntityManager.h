#pragma once

#include "core/IManager.h"
#include "core/Types.h"
#include <string>
#include <vector>

// [Requirement 5, 7] Manages Player (8-dir movement & animation), NPCs, Field Monsters, and Boss Monster.
class EntityManager : public IManager {
public:
    const char* GetName() const override { return "EntityManager"; }
    bool Init() override;
    void Update(float dt) override;
    void Render() override;
    void Shutdown() override;

    void ResetPlayerFromSpec();
    void RevivePlayerFull();
    void RecalculatePlayerStats();
    void GainExp(int amount);
    void OnMapChanged(const std::string& mapId, int spawnTileX, int spawnTileY);

    const PlayerEntity& GetPlayer() const { return m_player; }
    PlayerEntity& GetPlayerMutable() { return m_player; }
    const std::vector<NpcEntity>& GetNpcs() const { return m_npcs; }
    std::vector<MonsterEntity>& GetMonsters() { return m_monsters; }
    const std::vector<MonsterEntity>& GetMonsters() const { return m_monsters; }
    BossEntity& GetBoss() { return m_boss; }
    const BossEntity& GetBoss() const { return m_boss; }

    const NpcSpec* FindNearestInteractableNpc(float maxRange = 54.0f) const;
    int GetExpToNextLevel() const { return m_player.level * 100; }

private:
    void UpdatePlayer(float dt);
    void UpdateNpcs(float dt);
    void UpdateMonsters(float dt);
    void UpdateBoss(float dt);
    void MoveEntityWithCollision(Vector2& pos, float radius, Vector2 delta);

    void RenderPlayer() const;
    void RenderNpc(const NpcEntity& npc) const;
    void RenderMonster(const MonsterEntity& m) const;
    void RenderBoss() const;

    PlayerEntity m_player;
    std::vector<NpcEntity> m_npcs;
    std::vector<MonsterEntity> m_monsters;
    BossEntity m_boss;
    int m_nextMonsterInstanceId = 1;
};
