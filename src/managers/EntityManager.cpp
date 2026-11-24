#include "managers/EntityManager.h"
#include "managers/DataManager.h"
#include "managers/ResourceManager.h"
#include "managers/MapManager.h"
#include "managers/CombatManager.h"
#include "managers/EconomyManager.h"
#include "managers/QuestManager.h"
#include "managers/UIManager.h"
#include "core/GameManager.h"

#include <algorithm>
#include <cmath>

static float VecLen(Vector2 v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

static Vector2 VecNorm(Vector2 v) {
    float l = VecLen(v);
    if (l < 0.0001f) return {0.0f, 0.0f};
    return {v.x / l, v.y / l};
}

bool EntityManager::Init() {
    ResetPlayerFromSpec();
    return true;
}

void EntityManager::Shutdown() {
    m_npcs.clear();
    m_monsters.clear();
    m_boss = BossEntity{};
}

void EntityManager::ResetPlayerFromSpec() {
    const auto& spec = GameManager::Get().Data().GetPlayerSpec();
    m_player = PlayerEntity{};
    m_player.level = spec.startLevel;
    m_player.exp = 0;
    m_player.dir = Direction8::Down;
    m_player.facingVec = Direction8ToVector(m_player.dir);
    m_player.animState = AnimState::Idle;
    RecalculatePlayerStats();
    m_player.hp = m_player.maxHp;
    m_boss.defeatedOnce = false;
}

void EntityManager::RevivePlayerFull() {
    RecalculatePlayerStats();
    m_player.hp = m_player.maxHp;
    m_player.invulnTimer = 1.5f;
    m_player.attackTimer = 0.0f;
    m_player.dashTimer = 0.0f;
}

void EntityManager::RecalculatePlayerStats() {
    const auto& spec = GameManager::Get().Data().GetPlayerSpec();
    const auto& eco = GameManager::Get().Economy();
    int lvlDelta = m_player.level - 1;
    int oldMax = m_player.maxHp;

    m_player.maxHp = spec.baseMaxHp + lvlDelta * spec.hpPerLevel + eco.GetEquipmentHpBonus();
    m_player.atk = spec.baseAtk + lvlDelta * spec.atkPerLevel + eco.GetEquipmentAtkBonus();
    m_player.def = spec.baseDef + lvlDelta * spec.defPerLevel + eco.GetEquipmentDefBonus();

    if (m_player.maxHp > oldMax && oldMax > 0) {
        m_player.hp += (m_player.maxHp - oldMax);
    }
    m_player.hp = std::clamp(m_player.hp, 0, m_player.maxHp);
}

void EntityManager::GainExp(int amount) {
    if (amount <= 0) return;
    const auto& data = GameManager::Get().Data();
    const auto& spec = data.GetPlayerSpec();
    m_player.exp += amount;

    while (m_player.level < spec.maxLevel && m_player.exp >= GetExpToNextLevel()) {
        m_player.exp -= GetExpToNextLevel();
        m_player.level++;
        RecalculatePlayerStats();
        m_player.hp = m_player.maxHp;
        std::string lvlStr = std::to_string(m_player.level);
        GameManager::Get().Combat().SpawnFloatingText(
            Vector2{m_player.pos.x, m_player.pos.y - 26.0f},
            data.FormatText("combat.level_up_float", {lvlStr}), GOLD, 22.0f);
        GameManager::Get().AddToast(data.FormatText("toast.level_up", {lvlStr}), GOLD, 3.0f);
    }
}

void EntityManager::OnMapChanged(const std::string& mapId, int spawnTileX, int spawnTileY) {
    auto& mapMgr = GameManager::Get().Map();
    auto& data = GameManager::Get().Data();

    m_player.pos = mapMgr.TileToWorldCenter(spawnTileX, spawnTileY);
    m_player.warpCooldownTimer = 0.65f;
    m_player.dashTimer = 0.0f;

    // Populate NPCs for this map
    m_npcs.clear();
    for (const auto& ns : data.GetAllNpcs()) {
        if (ns.mapId == mapId) {
            NpcEntity ne;
            ne.specId = ns.id;
            ne.pos = mapMgr.TileToWorldCenter(ns.tileX, ns.tileY);
            m_npcs.push_back(ne);
        }
    }

    // Populate Field Monsters
    m_monsters.clear();
    const MapSpec* ms = data.GetMapSpec(mapId);
    if (ms) {
        for (const auto& sp : ms->monsterSpawns) {
            const MonsterSpec* mspec = data.GetMonsterSpec(sp.monsterId);
            if (!mspec) continue;
            MonsterEntity me;
            me.instanceId = m_nextMonsterInstanceId++;
            me.specId = mspec->id;
            me.spawnPos = mapMgr.TileToWorldCenter(sp.tileX, sp.tileY);
            me.pos = me.spawnPos;
            me.patrolTarget = me.spawnPos;
            me.hp = mspec->maxHp;
            me.maxHp = mspec->maxHp;
            me.alive = true;
            m_monsters.push_back(me);
        }

        // Populate Boss
        bool wasDefeated = m_boss.defeatedOnce;
        m_boss = BossEntity{};
        m_boss.defeatedOnce = wasDefeated;
        if (ms->bossSpawn.hasBoss) {
            const BossSpec* bspec = data.GetBossSpec(ms->bossSpawn.bossId);
            if (bspec) {
                m_boss.active = true;
                m_boss.specId = bspec->id;
                m_boss.spawnPos = mapMgr.TileToWorldCenter(ms->bossSpawn.tileX, ms->bossSpawn.tileY);
                m_boss.pos = m_boss.spawnPos;
                m_boss.hp = bspec->maxHp;
                m_boss.maxHp = bspec->maxHp;
                m_boss.alive = !wasDefeated;
                m_boss.enraged = false;
                m_boss.fireballTimer = 2.4f;
                m_boss.aoeTimer = 3.8f;
                m_boss.chargeTimer = 5.5f;
            }
        }
    }
}

void EntityManager::MoveEntityWithCollision(Vector2& pos, float radius, Vector2 delta) {
    auto& mapMgr = GameManager::Get().Map();
    Vector2 tryX{pos.x + delta.x, pos.y};
    if (mapMgr.IsCircleWalkable(tryX, radius)) {
        pos.x = tryX.x;
    }
    Vector2 tryY{pos.x, pos.y + delta.y};
    if (mapMgr.IsCircleWalkable(tryY, radius)) {
        pos.y = tryY.y;
    }
}

const NpcSpec* EntityManager::FindNearestInteractableNpc(float maxRange) const {
    const auto& data = GameManager::Get().Data();
    const NpcSpec* best = nullptr;
    float bestDistSq = maxRange * maxRange;
    for (const auto& npc : m_npcs) {
        float dx = npc.pos.x - m_player.pos.x;
        float dy = npc.pos.y - m_player.pos.y;
        float d2 = dx * dx + dy * dy;
        if (d2 <= bestDistSq) {
            bestDistSq = d2;
            best = data.GetNpcSpec(npc.specId);
        }
    }
    return best;
}

void EntityManager::Update(float dt) {
    UpdatePlayer(dt);
    UpdateNpcs(dt);
    UpdateMonsters(dt);
    UpdateBoss(dt);
}

void EntityManager::UpdatePlayer(float dt) {
    const auto& spec = GameManager::Get().Data().GetPlayerSpec();
    auto& ui = GameManager::Get().UI();

    if (m_player.invulnTimer > 0.0f) m_player.invulnTimer -= dt;
    if (m_player.hitFlashTimer > 0.0f) m_player.hitFlashTimer -= dt;
    if (m_player.attackCooldownTimer > 0.0f) m_player.attackCooldownTimer -= dt;
    if (m_player.skillCooldownTimer > 0.0f) m_player.skillCooldownTimer -= dt;
    if (m_player.dashCooldownTimer > 0.0f) m_player.dashCooldownTimer -= dt;
    if (m_player.warpCooldownTimer > 0.0f) m_player.warpCooldownTimer -= dt;

    // Block movement/combat inputs while modal UI (shop/dialogue/inventory/quest) is open
    bool uiBlocking = ui.IsBlockingGameplayInput();

    // Quick potion hotkeys work anytime during gameplay
    if (!uiBlocking) {
        if (IsKeyPressed(KEY_ONE)) GameManager::Get().Economy().UseOrEquipItem("potion_hp");
        if (IsKeyPressed(KEY_TWO)) GameManager::Get().Economy().UseOrEquipItem("potion_hi");
    }

    // Interact with NPC (E)
    if (!uiBlocking && IsKeyPressed(KEY_E)) {
        if (const NpcSpec* npc = FindNearestInteractableNpc(54.0f)) {
            ui.InteractWithNpc(*npc);
            return;
        }
    }

    // 8-Direction Movement Input
    float ix = 0.0f;
    float iy = 0.0f;
    if (!uiBlocking) {
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  ix -= 1.0f;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) ix += 1.0f;
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    iy -= 1.0f;
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  iy += 1.0f;
    }

    bool moving = (ix != 0.0f || iy != 0.0f);
    if (moving && m_player.dashTimer <= 0.0f) {
        m_player.dir = VectorToDirection8(ix, iy, m_player.dir);
        m_player.facingVec = Direction8ToVector(m_player.dir);
    }

    // Dash Input (Left Shift)
    if (!uiBlocking && IsKeyPressed(KEY_LEFT_SHIFT) && m_player.dashCooldownTimer <= 0.0f && m_player.dashTimer <= 0.0f) {
        m_player.dashTimer = spec.dashDuration;
        m_player.dashCooldownTimer = spec.dashCooldown;
        m_player.invulnTimer = std::max(m_player.invulnTimer, spec.dashDuration + 0.06f);
        m_player.dashDir = moving ? VecNorm(Vector2{ix, iy}) : m_player.facingVec;
    }

    // Melee Attack Input (J / Space / Left Mouse)
    bool attackPressed = !uiBlocking &&
        (IsKeyPressed(KEY_J) || IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
    if (attackPressed && m_player.attackCooldownTimer <= 0.0f && m_player.attackTimer <= 0.0f) {
        m_player.attackTimer = spec.attackDuration;
        m_player.attackCooldownTimer = spec.attackCooldown;
        m_player.attackHitApplied = false;
        m_player.animState = AnimState::Attack;
        m_player.animFrame = 0;
        m_player.animTimer = 0.0f;
    }

    // Whirlwind Skill Input (K / Right Mouse)
    bool skillPressed = !uiBlocking &&
        (IsKeyPressed(KEY_K) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT));
    if (skillPressed && m_player.skillCooldownTimer <= 0.0f) {
        m_player.skillCooldownTimer = spec.skillCooldown;
        m_player.attackTimer = spec.attackDuration;
        m_player.attackHitApplied = true;
        m_player.animState = AnimState::Attack;
        m_player.animFrame = 0;
        m_player.animTimer = 0.0f;
        GameManager::Get().Combat().PerformPlayerWhirlwindSkill();
    }

    // Movement execution
    if (m_player.dashTimer > 0.0f) {
        m_player.dashTimer -= dt;
        Vector2 delta{m_player.dashDir.x * spec.dashSpeed * dt, m_player.dashDir.y * spec.dashSpeed * dt};
        MoveEntityWithCollision(m_player.pos, m_player.radius, delta);
    } else if (moving) {
        Vector2 moveDir = VecNorm(Vector2{ix, iy});
        float speedScale = (m_player.attackTimer > 0.0f) ? 0.55f : 1.0f;
        Vector2 delta{moveDir.x * spec.moveSpeed * speedScale * dt, moveDir.y * spec.moveSpeed * speedScale * dt};
        MoveEntityWithCollision(m_player.pos, m_player.radius, delta);
    }

    // Attack timer & hit trigger mid-swing
    if (m_player.attackTimer > 0.0f) {
        m_player.attackTimer -= dt;
        if (!m_player.attackHitApplied && m_player.attackTimer <= spec.attackDuration * 0.65f) {
            m_player.attackHitApplied = true;
            GameManager::Get().Combat().PerformPlayerMeleeAttack();
        }
        m_player.animState = AnimState::Attack;
    } else {
        m_player.animState = moving ? AnimState::Walk : AnimState::Idle;
    }

    // 8-Direction Sprite Frame Advancement
    float fps = spec.anim.idleFps;
    if (m_player.animState == AnimState::Walk) fps = spec.anim.walkFps;
    else if (m_player.animState == AnimState::Attack) fps = spec.anim.attackFps;

    m_player.animTimer += dt * fps;
    if (m_player.animTimer >= 1.0f) {
        m_player.animTimer -= 1.0f;
        m_player.animFrame = (m_player.animFrame + 1) % spec.anim.framesPerRow;
    }

    // Town Healing Fountain aura
    auto& mapMgr = GameManager::Get().Map();
    if (m_player.hp < m_player.maxHp && mapMgr.IsNearSpecialTile(m_player.pos, "heal", 2.0f)) {
        m_player.fountainHealTimer += dt;
        if (m_player.fountainHealTimer >= 0.7f) {
            m_player.fountainHealTimer = 0.0f;
            int heal = std::min(15, m_player.maxHp - m_player.hp);
            m_player.hp += heal;
            GameManager::Get().Combat().SpawnFloatingText(
                Vector2{m_player.pos.x, m_player.pos.y - 20.0f},
                GameManager::Get().Data().FormatText("combat.hp_heal_float", {std::to_string(heal)}), LIME, 16.0f);
        }
    }

    // Portal Warp Detection
    if (m_player.warpCooldownTimer <= 0.0f) {
        if (const WarpSpec* warp = mapMgr.FindWarpAtWorldPos(m_player.pos)) {
            GameManager::Get().ChangeMap(warp->targetMap, warp->targetTileX, warp->targetTileY);
        }
    }
}

void EntityManager::UpdateNpcs(float dt) {
    const auto& data = GameManager::Get().Data();
    for (auto& npc : m_npcs) {
        const NpcSpec* spec = data.GetNpcSpec(npc.specId);
        int frames = spec ? spec->frames : 4;
        npc.animTimer += dt * 3.5f;
        if (npc.animTimer >= 1.0f) {
            npc.animTimer -= 1.0f;
            npc.animFrame = (npc.animFrame + 1) % frames;
        }
    }
}

void EntityManager::UpdateMonsters(float dt) {
    const auto& data = GameManager::Get().Data();
    auto& combat = GameManager::Get().Combat();

    for (auto& m : m_monsters) {
        const MonsterSpec* spec = data.GetMonsterSpec(m.specId);
        if (!spec) continue;

        if (!m.alive) {
            m.respawnTimer -= dt;
            if (m.respawnTimer <= 0.0f) {
                m.alive = true;
                m.hp = spec->maxHp;
                m.maxHp = spec->maxHp;
                m.pos = m.spawnPos;
                m.knockbackVel = {0.0f, 0.0f};
                m.path.clear();
                m.repathTimer = 0.0f;
            }
            continue;
        }

        if (m.hitFlashTimer > 0.0f) m.hitFlashTimer -= dt;
        if (m.attackCooldownTimer > 0.0f) m.attackCooldownTimer -= dt;

        // Apply knockback decay
        if (VecLen(m.knockbackVel) > 4.0f) {
            MoveEntityWithCollision(m.pos, m.radius, Vector2{m.knockbackVel.x * dt, m.knockbackVel.y * dt});
            m.knockbackVel.x *= std::exp(-9.0f * dt);
            m.knockbackVel.y *= std::exp(-9.0f * dt);
        }

        Vector2 toPlayer{m_player.pos.x - m.pos.x, m_player.pos.y - m.pos.y};
        float dist = VecLen(toPlayer);

        if (dist <= spec->detectRange) {
            m.facingLeft = (toPlayer.x < 0.0f);

            // Ranged skeleton attack at mid-range
            if (spec->ranged && dist > 65.0f && dist < 175.0f && m.attackCooldownTimer <= 0.0f) {
                m.attackCooldownTimer = spec->attackCooldown * 1.45f;
                Vector2 dir = VecNorm(toPlayer);
                combat.SpawnProjectile(m.pos, Vector2{dir.x * 165.0f, dir.y * 165.0f},
                                       spec->atk - 3, false, 603);
            }

            if (dist > spec->attackRange * 0.85f) {
                m.repathTimer -= dt;
                if (m.repathTimer <= 0.0f || m.path.empty()) {
                    m.repathTimer = 0.25f + static_cast<float>(m.instanceId % 5) * 0.02f;
                    m.path = GameManager::Get().Map().FindPath(m.pos, m_player.pos, m.radius);
                }

                while (!m.path.empty() && VecLen(Vector2{m.path.front().x - m.pos.x, m.path.front().y - m.pos.y}) < 10.0f) {
                    m.path.erase(m.path.begin());
                }

                Vector2 moveTarget = m.path.empty() ? m_player.pos : m.path.front();
                Vector2 dir = VecNorm(Vector2{moveTarget.x - m.pos.x, moveTarget.y - m.pos.y});
                m.facingLeft = (dir.x < 0.0f);
                MoveEntityWithCollision(m.pos, m.radius, Vector2{dir.x * spec->moveSpeed * dt, dir.y * spec->moveSpeed * dt});
            }

            if (dist <= spec->attackRange + m_player.radius && m.attackCooldownTimer <= 0.0f) {
                m.attackCooldownTimer = spec->attackCooldown;
                combat.DamagePlayer(spec->atk, m.pos);
            }
        } else {
            m.path.clear();
            m.repathTimer = 0.0f;
            // Gentle patrol around spawn
            Vector2 toPatrol{m.patrolTarget.x - m.pos.x, m.patrolTarget.y - m.pos.y};
            if (VecLen(toPatrol) < 6.0f) {
                m.patrolWaitTimer -= dt;
                if (m.patrolWaitTimer <= 0.0f) {
                    m.patrolWaitTimer = 1.5f + static_cast<float>(GetRandomValue(0, 20)) * 0.1f;
                    Vector2 candidate{
                        m.spawnPos.x + static_cast<float>(GetRandomValue(-48, 48)),
                        m.spawnPos.y + static_cast<float>(GetRandomValue(-48, 48))
                    };
                    if (GameManager::Get().Map().IsCircleWalkable(candidate, m.radius)) {
                        m.patrolTarget = candidate;
                    } else {
                        m.patrolTarget = m.spawnPos;
                    }
                }
            } else {
                Vector2 dir = VecNorm(toPatrol);
                m.facingLeft = (dir.x < 0.0f);
                MoveEntityWithCollision(m.pos, m.radius, Vector2{dir.x * spec->moveSpeed * 0.45f * dt, dir.y * spec->moveSpeed * 0.45f * dt});
            }
        }

        m.animTimer += dt * 6.0f;
        if (m.animTimer >= 1.0f) {
            m.animTimer -= 1.0f;
            m.animFrame = (m.animFrame + 1) % spec->frames;
        }
    }
}

void EntityManager::UpdateBoss(float dt) {
    if (!m_boss.active || !m_boss.alive) return;
    const BossSpec* spec = GameManager::Get().Data().GetBossSpec(m_boss.specId);
    if (!spec) return;

    auto& combat = GameManager::Get().Combat();

    if (m_boss.hitFlashTimer > 0.0f) m_boss.hitFlashTimer -= dt;
    if (m_boss.attackCooldownTimer > 0.0f) m_boss.attackCooldownTimer -= dt;

    // Check Phase 2 Enrage transition
    const auto& data = GameManager::Get().Data();
    if (!m_boss.enraged && static_cast<float>(m_boss.hp) <= static_cast<float>(m_boss.maxHp) * spec->enrageHpRatio) {
        m_boss.enraged = true;
        GameManager::Get().AddToast(data.FormatText("toast.boss_enrage", {spec->name}), RED, 3.8f);
        combat.SpawnFloatingText(Vector2{m_boss.pos.x, m_boss.pos.y - 44.0f},
                                 data.GetText("combat.boss_enrage_float"), RED, 24.0f);
    }

    // Apply light knockback
    if (VecLen(m_boss.knockbackVel) > 4.0f) {
        MoveEntityWithCollision(m_boss.pos, m_boss.radius, Vector2{m_boss.knockbackVel.x * dt, m_boss.knockbackVel.y * dt});
        m_boss.knockbackVel.x *= std::exp(-12.0f * dt);
        m_boss.knockbackVel.y *= std::exp(-12.0f * dt);
    }

    Vector2 toPlayer{m_player.pos.x - m_boss.pos.x, m_player.pos.y - m_boss.pos.y};
    float dist = VecLen(toPlayer);

    // Phase 2 Dash Charge Logic
    if (m_boss.chargeTelegraphTimer > 0.0f) {
        m_boss.chargeTelegraphTimer -= dt;
        if (m_boss.chargeTelegraphTimer <= 0.0f) {
            m_boss.chargeActiveTimer = 0.48f;
        }
    } else if (m_boss.chargeActiveTimer > 0.0f) {
        m_boss.chargeActiveTimer -= dt;
        MoveEntityWithCollision(m_boss.pos, m_boss.radius,
                                Vector2{m_boss.chargeDir.x * spec->chargeSpeed * dt,
                                        m_boss.chargeDir.y * spec->chargeSpeed * dt});
        if (dist <= m_boss.radius + m_player.radius + 8.0f) {
            combat.DamagePlayer(static_cast<int>(spec->atk * 1.4f), m_boss.pos);
        }
    } else {
        // Standard boss pursuit via A* pathfinding (routes cleanly around obsidian pillars)
        float speed = m_boss.enraged ? spec->enrageSpeed : spec->moveSpeed;
        if (dist > spec->attackRange * 0.85f) {
            m_boss.repathTimer -= dt;
            if (m_boss.repathTimer <= 0.0f || m_boss.path.empty()) {
                m_boss.repathTimer = 0.25f;
                m_boss.path = GameManager::Get().Map().FindPath(m_boss.pos, m_player.pos, m_boss.radius);
            }

            while (!m_boss.path.empty() && VecLen(Vector2{m_boss.path.front().x - m_boss.pos.x, m_boss.path.front().y - m_boss.pos.y}) < 14.0f) {
                m_boss.path.erase(m_boss.path.begin());
            }

            Vector2 moveTarget = m_boss.path.empty() ? m_player.pos : m_boss.path.front();
            Vector2 dir = VecNorm(Vector2{moveTarget.x - m_boss.pos.x, moveTarget.y - m_boss.pos.y});
            MoveEntityWithCollision(m_boss.pos, m_boss.radius, Vector2{dir.x * speed * dt, dir.y * speed * dt});
        }

        // Melee Slam
        if (dist <= spec->attackRange + m_player.radius && m_boss.attackCooldownTimer <= 0.0f) {
            m_boss.attackCooldownTimer = m_boss.enraged ? spec->attackCooldown * 0.8f : spec->attackCooldown;
            combat.DamagePlayer(spec->atk, m_boss.pos);
        }

        // Pattern 1: Radial Fireball Barrage
        m_boss.fireballTimer -= dt;
        if (m_boss.fireballTimer <= 0.0f) {
            m_boss.fireballTimer = m_boss.enraged ? spec->fireballCooldown * 0.72f : spec->fireballCooldown;
            int count = m_boss.enraged ? spec->fireballCountP2 : spec->fireballCountP1;
            m_boss.fireballAngleOffset += 0.35f;
            for (int i = 0; i < count; ++i) {
                float angle = m_boss.fireballAngleOffset + (2.0f * PI * i) / static_cast<float>(count);
                Vector2 vel{std::cos(angle) * spec->fireballSpeed, std::sin(angle) * spec->fireballSpeed};
                combat.SpawnProjectile(m_boss.pos, vel, static_cast<int>(spec->atk * 0.85f), false, 603);
            }
        }

        // Pattern 2: Telegraphed Cursed AoE Slam Zones
        m_boss.aoeTimer -= dt;
        if (m_boss.aoeTimer <= 0.0f) {
            m_boss.aoeTimer = m_boss.enraged ? spec->aoeCooldown * 0.75f : spec->aoeCooldown;
            combat.SpawnTelegraphZone(m_player.pos, spec->aoeRadius, spec->aoeTelegraph,
                                      static_cast<int>(spec->atk * 1.35f));
            if (m_boss.enraged) {
                for (int extra = 0; extra < 2; ++extra) {
                    Vector2 offPos{
                        m_player.pos.x + static_cast<float>(GetRandomValue(-85, 85)),
                        m_player.pos.y + static_cast<float>(GetRandomValue(-85, 85))
                    };
                    combat.SpawnTelegraphZone(offPos, spec->aoeRadius * 0.85f, spec->aoeTelegraph + 0.25f * extra,
                                              static_cast<int>(spec->atk * 1.2f));
                }
            }
        }

        // Pattern 3 (Phase 2): Telegraphed Charge
        if (m_boss.enraged) {
            m_boss.chargeTimer -= dt;
            if (m_boss.chargeTimer <= 0.0f && dist > 60.0f) {
                m_boss.chargeTimer = spec->chargeCooldown;
                m_boss.chargeTelegraphTimer = 0.65f;
                m_boss.chargeDir = VecNorm(toPlayer);
                m_boss.path.clear();
                m_boss.repathTimer = 0.0f;
                combat.SpawnFloatingText(Vector2{m_boss.pos.x, m_boss.pos.y - 42.0f},
                                         data.GetText("combat.boss_charge_float"), ORANGE, 18.0f);
            }
        }
    }

    m_boss.animTimer += dt * (m_boss.enraged ? 8.5f : 5.5f);
    if (m_boss.animTimer >= 1.0f) {
        m_boss.animTimer -= 1.0f;
        m_boss.animFrame = (m_boss.animFrame + 1) % spec->frames;
    }
}

void EntityManager::RenderPlayer() const {
    const auto& spec = GameManager::Get().Data().GetPlayerSpec();
    const auto& res = GameManager::Get().Resources();

    int artId = spec.anim.idleArtId;
    if (m_player.animState == AnimState::Walk) artId = spec.anim.walkArtId;
    else if (m_player.animState == AnimState::Attack) artId = spec.anim.attackArtId;

    const Texture2D& tex = res.GetTexture(artId);
    float fw = static_cast<float>(spec.anim.frameWidth);
    float fh = static_cast<float>(spec.anim.frameHeight);

    // Row = Direction8 (0..7), Col = animFrame (0..3)
    int row = static_cast<int>(m_player.dir);
    Rectangle src{m_player.animFrame * fw, row * fh, fw, fh};
    Rectangle dst{m_player.pos.x - fw * 0.5f, m_player.pos.y - fh * 0.65f, fw, fh};

    // Ground shadow
    DrawEllipse(static_cast<int>(m_player.pos.x), static_cast<int>(m_player.pos.y + 8.0f), 11.0f, 5.0f, Color{0, 0, 0, 110});

    Color tint = WHITE;
    if (m_player.hitFlashTimer > 0.0f) tint = RED;
    else if (m_player.dashTimer > 0.0f) tint = Color{150, 230, 255, 200};
    else if (m_player.invulnTimer > 0.0f && static_cast<int>(m_player.invulnTimer * 18.0f) % 2 == 0) {
        tint = Color{255, 255, 255, 140};
    }

    DrawTexturePro(tex, src, dst, Vector2{0, 0}, 0.0f, tint);
}

void EntityManager::RenderNpc(const NpcEntity& npc) const {
    const auto& data = GameManager::Get().Data();
    const auto& res = GameManager::Get().Resources();
    const auto& quests = GameManager::Get().Quests();
    const NpcSpec* spec = data.GetNpcSpec(npc.specId);
    if (!spec) return;

    const Texture2D& tex = res.GetTexture(spec->artId);
    float fs = static_cast<float>(spec->frameSize);
    Rectangle src{npc.animFrame * fs, 0.0f, fs, fs};
    Rectangle dst{npc.pos.x - fs * 0.5f, npc.pos.y - fs * 0.65f, fs, fs};

    DrawEllipse(static_cast<int>(npc.pos.x), static_cast<int>(npc.pos.y + 8.0f), 11.0f, 5.0f, Color{0, 0, 0, 110});
    DrawTexturePro(tex, src, dst, Vector2{0, 0}, 0.0f, WHITE);

    // Nameplate
    float nameW = res.MeasureTextKR(spec->name, 10.0f);
    res.DrawTextKR(spec->name, npc.pos.x - nameW * 0.5f, npc.pos.y - 34.0f, 10.0f, Color{255, 240, 180, 255});

    // Quest badge above Quest NPC
    if (spec->role == "quest") {
        if (quests.NpcHasReadyTurnIn(spec->id)) {
            res.DrawTextKR("?", npc.pos.x - 4.0f, npc.pos.y - 50.0f, 16.0f, GOLD);
        } else if (quests.NpcHasAvailableOffer(spec->id)) {
            res.DrawTextKR("!", npc.pos.x - 3.0f, npc.pos.y - 50.0f, 16.0f, YELLOW);
        }
    }

    // Proximity interact hint
    float dx = npc.pos.x - m_player.pos.x;
    float dy = npc.pos.y - m_player.pos.y;
    if (dx * dx + dy * dy <= 54.0f * 54.0f) {
        std::string hintKey = "npc.hint_talk";
        if (spec->role == "shop") hintKey = "npc.hint_shop";
        else if (spec->role == "healer") hintKey = "npc.hint_heal";
        const std::string& hint = data.GetText(hintKey);
        float hw = res.MeasureTextKR(hint, 10.0f);
        DrawRectangleRounded(Rectangle{npc.pos.x - hw * 0.5f - 4, npc.pos.y + 12, hw + 8, 14}, 0.3f, 4, Color{10, 12, 20, 200});
        res.DrawTextKR(hint, npc.pos.x - hw * 0.5f, npc.pos.y + 14.0f, 10.0f, SKYBLUE);
    }
}

void EntityManager::RenderMonster(const MonsterEntity& m) const {
    if (!m.alive) return;
    const auto& data = GameManager::Get().Data();
    const auto& res = GameManager::Get().Resources();
    const MonsterSpec* spec = data.GetMonsterSpec(m.specId);
    if (!spec) return;

    const Texture2D& tex = res.GetTexture(spec->artId);
    float fs = static_cast<float>(spec->frameSize);
    Rectangle src{m.animFrame * fs, 0.0f, m.facingLeft ? -fs : fs, fs};
    Rectangle dst{m.pos.x - fs * 0.5f, m.pos.y - fs * 0.62f, fs, fs};

    DrawEllipse(static_cast<int>(m.pos.x), static_cast<int>(m.pos.y + 8.0f), 11.0f, 5.0f, Color{0, 0, 0, 110});
    Color tint = (m.hitFlashTimer > 0.0f) ? Color{255, 110, 110, 255} : WHITE;
    DrawTexturePro(tex, src, dst, Vector2{0, 0}, 0.0f, tint);

    // Overhead mini HP bar
    float barW = 28.0f;
    float barH = 4.0f;
    float ratio = std::clamp(static_cast<float>(m.hp) / static_cast<float>(m.maxHp), 0.0f, 1.0f);
    DrawRectangle(static_cast<int>(m.pos.x - barW * 0.5f), static_cast<int>(m.pos.y - 26.0f), static_cast<int>(barW), static_cast<int>(barH), Color{20, 20, 25, 220});
    DrawRectangle(static_cast<int>(m.pos.x - barW * 0.5f), static_cast<int>(m.pos.y - 26.0f), static_cast<int>(barW * ratio), static_cast<int>(barH), Color{220, 55, 55, 255});
}

void EntityManager::RenderBoss() const {
    if (!m_boss.active || !m_boss.alive) return;
    const auto& data = GameManager::Get().Data();
    const auto& res = GameManager::Get().Resources();
    const BossSpec* spec = data.GetBossSpec(m_boss.specId);
    if (!spec) return;

    // Charge telegraph line
    if (m_boss.chargeTelegraphTimer > 0.0f) {
        Vector2 endPos{m_boss.pos.x + m_boss.chargeDir.x * 160.0f, m_boss.pos.y + m_boss.chargeDir.y * 160.0f};
        DrawLineEx(m_boss.pos, endPos, 18.0f, Color{255, 60, 30, 95});
    }

    // Fiery enrage aura
    if (m_boss.enraged) {
        DrawCircleGradient(static_cast<int>(m_boss.pos.x), static_cast<int>(m_boss.pos.y), 44.0f,
                           Color{255, 70, 20, 110}, Color{255, 30, 0, 0});
    }

    const Texture2D& tex = res.GetTexture(spec->artId);
    float fs = static_cast<float>(spec->frameSize);
    int row = m_boss.enraged ? 1 : 0;
    Rectangle src{m_boss.animFrame * fs, row * fs, fs, fs};
    Rectangle dst{m_boss.pos.x - fs * 0.5f, m_boss.pos.y - fs * 0.6f, fs, fs};

    DrawEllipse(static_cast<int>(m_boss.pos.x), static_cast<int>(m_boss.pos.y + 20.0f), 26.0f, 10.0f, Color{0, 0, 0, 140});
    Color tint = (m_boss.hitFlashTimer > 0.0f) ? Color{255, 120, 120, 255} : WHITE;
    DrawTexturePro(tex, src, dst, Vector2{0, 0}, 0.0f, tint);
}

void EntityManager::Render() {
    // Y-sorted render list for clean top-down overlapping depth
    struct DrawItem {
        float y;
        int kind; // 0=player, 1=npc, 2=monster, 3=boss
        size_t idx;
    };
    std::vector<DrawItem> list;
    list.push_back({m_player.pos.y, 0, 0});
    for (size_t i = 0; i < m_npcs.size(); ++i) list.push_back({m_npcs[i].pos.y, 1, i});
    for (size_t i = 0; i < m_monsters.size(); ++i) {
        if (m_monsters[i].alive) list.push_back({m_monsters[i].pos.y, 2, i});
    }
    if (m_boss.active && m_boss.alive) list.push_back({m_boss.pos.y, 3, 0});

    std::sort(list.begin(), list.end(), [](const DrawItem& a, const DrawItem& b) {
        return a.y < b.y;
    });

    for (const auto& item : list) {
        switch (item.kind) {
            case 0: RenderPlayer(); break;
            case 1: RenderNpc(m_npcs[item.idx]); break;
            case 2: RenderMonster(m_monsters[item.idx]); break;
            case 3: RenderBoss(); break;
        }
    }
}
