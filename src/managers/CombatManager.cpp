#include "managers/CombatManager.h"
#include "managers/DataManager.h"
#include "managers/ResourceManager.h"
#include "managers/MapManager.h"
#include "managers/EntityManager.h"
#include "managers/EconomyManager.h"
#include "managers/QuestManager.h"
#include "core/GameManager.h"

#include <algorithm>
#include <cmath>

static float Dist2(Vector2 a, Vector2 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

void CombatManager::ClearTransientEffects() {
    m_projectiles.clear();
    m_telegraphs.clear();
    m_effects.clear();
    m_floatingTexts.clear();
}

void CombatManager::SpawnProjectile(Vector2 pos, Vector2 vel, int damage, bool fromPlayer, int artId) {
    Projectile p;
    p.pos = pos;
    p.vel = vel;
    p.damage = damage;
    p.fromPlayer = fromPlayer;
    p.artId = artId;
    m_projectiles.push_back(p);
}

void CombatManager::SpawnTelegraphZone(Vector2 pos, float radius, float duration, int damage) {
    TelegraphZone z;
    z.pos = pos;
    z.radius = radius;
    z.duration = duration;
    z.damage = damage;
    m_telegraphs.push_back(z);
}

void CombatManager::SpawnVisualEffect(Vector2 pos, float rotationDeg, float scale, int artId, float duration, Color tint) {
    VisualEffect fx;
    fx.pos = pos;
    fx.rotationDeg = rotationDeg;
    fx.scale = scale;
    fx.artId = artId;
    fx.duration = duration;
    fx.tint = tint;
    m_effects.push_back(fx);
}

void CombatManager::SpawnFloatingText(Vector2 pos, const std::string& text, Color color, float fontSize) {
    FloatingText ft;
    ft.pos = pos;
    ft.text = text;
    ft.color = color;
    ft.fontSize = fontSize;
    m_floatingTexts.push_back(ft);
}

void CombatManager::PerformPlayerMeleeAttack() {
    const auto& pSpec = GameManager::Get().Data().GetPlayerSpec();
    auto& ent = GameManager::Get().Entities();
    const auto& player = ent.GetPlayer();

    Vector2 hitCenter{
        player.pos.x + player.facingVec.x * (pSpec.attackRange * 0.55f),
        player.pos.y + player.facingVec.y * (pSpec.attackRange * 0.55f)
    };
    float angleDeg = std::atan2(player.facingVec.y, player.facingVec.x) * (180.0f / PI);
    SpawnVisualEffect(hitCenter, angleDeg, 1.35f, 601, 0.22f, Color{220, 245, 255, 240});

    float hitRadius = pSpec.attackRange * 0.72f;
    for (auto& m : ent.GetMonsters()) {
        if (!m.alive) continue;
        float rSum = hitRadius + m.radius;
        if (Dist2(hitCenter, m.pos) <= rSum * rSum) {
            ApplyDamageToMonster(m, player.atk, player.pos, 180.0f);
        }
    }

    auto& boss = ent.GetBoss();
    if (boss.active && boss.alive) {
        float rSum = hitRadius + boss.radius;
        if (Dist2(hitCenter, boss.pos) <= rSum * rSum) {
            ApplyDamageToBoss(boss, player.atk, player.pos, 80.0f);
        }
    }
}

void CombatManager::PerformPlayerWhirlwindSkill() {
    const auto& pSpec = GameManager::Get().Data().GetPlayerSpec();
    auto& ent = GameManager::Get().Entities();
    const auto& player = ent.GetPlayer();

    SpawnVisualEffect(player.pos, 0.0f, 3.8f, 602, 0.36f, Color{255, 225, 120, 235});
    int skillDmg = static_cast<int>(std::round(player.atk * pSpec.skillMultiplier));

    for (auto& m : ent.GetMonsters()) {
        if (!m.alive) continue;
        float rSum = pSpec.skillRadius + m.radius;
        if (Dist2(player.pos, m.pos) <= rSum * rSum) {
            ApplyDamageToMonster(m, skillDmg, player.pos, 260.0f);
        }
    }

    auto& boss = ent.GetBoss();
    if (boss.active && boss.alive) {
        float rSum = pSpec.skillRadius + boss.radius;
        if (Dist2(player.pos, boss.pos) <= rSum * rSum) {
            ApplyDamageToBoss(boss, skillDmg, player.pos, 110.0f);
        }
    }
}

void CombatManager::ApplyDamageToMonster(MonsterEntity& m, int rawDamage, Vector2 fromPos, float knockback) {
    const MonsterSpec* spec = GameManager::Get().Data().GetMonsterSpec(m.specId);
    if (!spec) return;

    int variance = GetRandomValue(-2, 3);
    int dmg = std::max(1, rawDamage - spec->def + variance);
    m.hp -= dmg;
    m.hitFlashTimer = 0.14f;

    Vector2 kb{m.pos.x - fromPos.x, m.pos.y - fromPos.y};
    float len = std::sqrt(kb.x * kb.x + kb.y * kb.y);
    if (len > 0.01f) {
        m.knockbackVel = {(kb.x / len) * knockback, (kb.y / len) * knockback};
    }

    SpawnFloatingText(Vector2{m.pos.x, m.pos.y - 20.0f}, "-" + std::to_string(dmg), YELLOW, 14.0f);

    if (m.hp <= 0) {
        m.alive = false;
        m.respawnTimer = spec->respawnTime;

        GameManager::Get().Economy().AddGold(spec->goldReward);
        GameManager::Get().Entities().GainExp(spec->expReward);
        GameManager::Get().Quests().NotifyTargetKilled(spec->id);

        SpawnFloatingText(Vector2{m.pos.x, m.pos.y - 32.0f},
                          GameManager::Get().Data().FormatText("combat.reward_float",
                              {std::to_string(spec->goldReward), std::to_string(spec->expReward)}),
                          GOLD, 12.0f);
    }
}

void CombatManager::ApplyDamageToBoss(BossEntity& b, int rawDamage, Vector2 fromPos, float knockback) {
    const BossSpec* spec = GameManager::Get().Data().GetBossSpec(b.specId);
    if (!spec) return;

    int variance = GetRandomValue(-2, 4);
    int dmg = std::max(1, rawDamage - spec->def + variance);
    b.hp -= dmg;
    b.hitFlashTimer = 0.12f;

    Vector2 kb{b.pos.x - fromPos.x, b.pos.y - fromPos.y};
    float len = std::sqrt(kb.x * kb.x + kb.y * kb.y);
    if (len > 0.01f) {
        b.knockbackVel = {(kb.x / len) * knockback, (kb.y / len) * knockback};
    }

    SpawnFloatingText(Vector2{b.pos.x, b.pos.y - 36.0f}, "-" + std::to_string(dmg), GOLD, 16.0f);

    if (b.hp <= 0) {
        b.hp = 0;
        b.alive = false;
        b.defeatedOnce = true;

        ClearTransientEffects();
        GameManager::Get().Economy().AddGold(spec->goldReward);
        GameManager::Get().Entities().GainExp(spec->expReward);
        if (!spec->dropItemId.empty()) {
            GameManager::Get().Economy().AddItem(spec->dropItemId, 1);
        }
        GameManager::Get().Quests().NotifyTargetKilled(spec->id);
        GameManager::Get().OnBossDefeated();
    }
}

void CombatManager::DamagePlayer(int rawDamage, Vector2 /*sourcePos*/) {
    auto& player = GameManager::Get().Entities().GetPlayerMutable();
    if (player.invulnTimer > 0.0f || player.hp <= 0) return;

    int dmg = std::max(1, rawDamage - player.def);
    player.hp = std::max(0, player.hp - dmg);
    player.invulnTimer = 0.55f;
    player.hitFlashTimer = 0.18f;

    SpawnFloatingText(Vector2{player.pos.x, player.pos.y - 24.0f}, "-" + std::to_string(dmg), RED, 16.0f);

    if (player.hp <= 0) {
        GameManager::Get().SetScreen(GameScreen::GameOver);
    }
}

void CombatManager::Update(float dt) {
    auto& mapMgr = GameManager::Get().Map();
    const auto& player = GameManager::Get().Entities().GetPlayer();

    // Update projectiles
    for (auto it = m_projectiles.begin(); it != m_projectiles.end();) {
        it->pos.x += it->vel.x * dt;
        it->pos.y += it->vel.y * dt;
        it->lifetime -= dt;
        it->animTimer += dt * 10.0f;

        bool hitWall = !mapMgr.IsCircleWalkable(it->pos, 4.0f);
        bool hitTarget = false;

        if (!it->fromPlayer) {
            float rSum = it->radius + player.radius;
            if (Dist2(it->pos, player.pos) <= rSum * rSum) {
                DamagePlayer(it->damage, it->pos);
                hitTarget = true;
            }
        }

        if (it->lifetime <= 0.0f || hitWall || hitTarget) {
            it = m_projectiles.erase(it);
        } else {
            ++it;
        }
    }

    // Update Boss Telegraph AoE Zones
    for (auto it = m_telegraphs.begin(); it != m_telegraphs.end();) {
        it->timer += dt;
        if (!it->triggered && it->timer >= it->duration) {
            it->triggered = true;
            SpawnVisualEffect(it->pos, 0.0f, it->radius / 16.0f, 602, 0.35f, Color{255, 80, 30, 230});
            float rSum = it->radius + player.radius * 0.6f;
            if (Dist2(it->pos, player.pos) <= rSum * rSum) {
                DamagePlayer(it->damage, it->pos);
            }
        }
        if (it->timer >= it->duration + 0.12f) {
            it = m_telegraphs.erase(it);
        } else {
            ++it;
        }
    }

    // Update VFX
    for (auto it = m_effects.begin(); it != m_effects.end();) {
        it->timer += dt;
        if (it->timer >= it->duration) {
            it = m_effects.erase(it);
        } else {
            ++it;
        }
    }

    // Update Floating Damage/Reward Texts
    for (auto it = m_floatingTexts.begin(); it != m_floatingTexts.end();) {
        it->timer += dt;
        it->pos.y -= 26.0f * dt;
        if (it->timer >= it->duration) {
            it = m_floatingTexts.erase(it);
        } else {
            ++it;
        }
    }
}

void CombatManager::RenderGroundEffects() const {
    for (const auto& z : m_telegraphs) {
        float progress = std::clamp(z.timer / z.duration, 0.0f, 1.0f);
        DrawCircle(static_cast<int>(z.pos.x), static_cast<int>(z.pos.y), z.radius,
                   Color{220, 35, 25, static_cast<unsigned char>(35 + 65 * progress)});
        DrawCircle(static_cast<int>(z.pos.x), static_cast<int>(z.pos.y), z.radius * progress,
                   Color{255, 90, 30, 95});
        DrawCircleLines(static_cast<int>(z.pos.x), static_cast<int>(z.pos.y), z.radius,
                        Color{255, 70, 50, 220});
    }
}

void CombatManager::Render() {
    const auto& res = GameManager::Get().Resources();

    // Render projectiles
    for (const auto& p : m_projectiles) {
        const Texture2D& tex = res.GetTexture(p.artId);
        int frame = static_cast<int>(p.animTimer) % 4;
        float fw = static_cast<float>(tex.width) / 4.0f;
        float fh = static_cast<float>(tex.height);
        Rectangle src{frame * fw, 0.0f, fw, fh};
        Rectangle dst{p.pos.x, p.pos.y, 24.0f, 24.0f};
        DrawTexturePro(tex, src, dst, Vector2{12.0f, 12.0f}, 0.0f, WHITE);
    }

    // Render combat VFX
    for (const auto& fx : m_effects) {
        const Texture2D& tex = res.GetTexture(fx.artId);
        float progress = std::clamp(fx.timer / fx.duration, 0.0f, 0.999f);
        int frame = static_cast<int>(progress * fx.frames);
        float fw = static_cast<float>(tex.width) / static_cast<float>(fx.frames);
        float fh = static_cast<float>(tex.height);
        Rectangle src{frame * fw, 0.0f, fw, fh};
        float drawW = fw * fx.scale;
        float drawH = fh * fx.scale;
        Rectangle dst{fx.pos.x, fx.pos.y, drawW, drawH};
        Color c = fx.tint;
        c.a = static_cast<unsigned char>(fx.tint.a * (1.0f - progress * 0.45f));
        DrawTexturePro(tex, src, dst, Vector2{drawW * 0.5f, drawH * 0.5f}, fx.rotationDeg, c);
    }

    // Render floating combat text
    for (const auto& ft : m_floatingTexts) {
        float alpha = 1.0f - std::clamp(ft.timer / ft.duration, 0.0f, 1.0f);
        Color c = ft.color;
        c.a = static_cast<unsigned char>(255 * alpha);
        float tw = res.MeasureTextKR(ft.text, ft.fontSize);
        res.DrawTextKR(ft.text, ft.pos.x - tw * 0.5f + 1.0f, ft.pos.y + 1.0f, ft.fontSize, Color{0, 0, 0, c.a});
        res.DrawTextKR(ft.text, ft.pos.x - tw * 0.5f, ft.pos.y, ft.fontSize, c);
    }
}
