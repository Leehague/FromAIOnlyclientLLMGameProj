#pragma once

#include "core/IManager.h"
#include "core/Types.h"
#include <string>
#include <vector>

// [Requirement 6] Combat system: Melee arc attacks, Whirlwind skill, Projectiles,
// Boss telegraph AoE zones, knockback, damage numbers, and loot rewards.
class CombatManager : public IManager {
public:
    const char* GetName() const override { return "CombatManager"; }
    bool Init() override { return true; }
    void Update(float dt) override;
    void Render() override;
    void Shutdown() override { ClearTransientEffects(); }

    void ClearTransientEffects();
    void RenderGroundEffects() const;

    void PerformPlayerMeleeAttack();
    void PerformPlayerWhirlwindSkill();
    void DamagePlayer(int rawDamage, Vector2 sourcePos);

    void SpawnProjectile(Vector2 pos, Vector2 vel, int damage, bool fromPlayer, int artId);
    void SpawnTelegraphZone(Vector2 pos, float radius, float duration, int damage);
    void SpawnVisualEffect(Vector2 pos, float rotationDeg, float scale, int artId, float duration, Color tint = WHITE);
    void SpawnFloatingText(Vector2 pos, const std::string& text, Color color, float fontSize = 14.0f);

private:
    void ApplyDamageToMonster(MonsterEntity& m, int rawDamage, Vector2 fromPos, float knockback);
    void ApplyDamageToBoss(BossEntity& b, int rawDamage, Vector2 fromPos, float knockback);

    std::vector<Projectile> m_projectiles;
    std::vector<TelegraphZone> m_telegraphs;
    std::vector<VisualEffect> m_effects;
    std::vector<FloatingText> m_floatingTexts;
};
