#include "managers/SkillManager.h"
#include "managers/DataManager.h"
#include "managers/EntityManager.h"
#include "managers/CombatManager.h"
#include "managers/EconomyManager.h"
#include "managers/InputManager.h"
#include "managers/SaveManager.h"
#include "managers/UIManager.h"
#include "core/GameManager.h"

#include <algorithm>
#include <cmath>
#include <iostream>

static float Dist2(Vector2 a, Vector2 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

static Vector2 SafeNorm(Vector2 v) {
    float len = std::sqrt(v.x * v.x + v.y * v.y);
    if (len < 0.0001f) return Vector2{0.0f, 1.0f};
    return Vector2{v.x / len, v.y / len};
}

static bool IsInsideCone(Vector2 origin, Vector2 dir, Vector2 targetPos, float radius, float coneAngleDeg) {
    Vector2 diff{targetPos.x - origin.x, targetPos.y - origin.y};
    float d2 = diff.x * diff.x + diff.y * diff.y;
    if (d2 > radius * radius) return false;
    if (d2 < 1.0f) return true;
    Vector2 nd = SafeNorm(dir);
    Vector2 nt = SafeNorm(diff);
    float dot = nd.x * nt.x + nd.y * nt.y;
    float halfRad = (coneAngleDeg * 0.5f) * (PI / 180.0f);
    return dot >= std::cos(halfRad);
}

bool SkillManager::Init() {
    RegisterBuiltinEffects();
    Reset();
    return true;
}

void SkillManager::Shutdown() {
    m_effectRegistry.clear();
    m_equippedSlots.clear();
    m_slotCooldowns.clear();
    m_unlockedSkills.clear();
}

void SkillManager::RegisterEffectHandler(const std::string& effectType, SkillEffectHandler handler) {
    m_effectRegistry[effectType] = std::move(handler);
}

bool SkillManager::HasEffectHandler(const std::string& effectType) const {
    return m_effectRegistry.find(effectType) != m_effectRegistry.end();
}

void SkillManager::RegisterBuiltinEffects() {
    m_effectRegistry.clear();
    RegisterEffectHandler("damage_aoe",       [this](const SkillEffectSpec& e, const SkillExecContext& c) { HandleDamageAoe(e, c); });
    RegisterEffectHandler("damage_target",    [this](const SkillEffectSpec& e, const SkillExecContext& c) { HandleDamageTarget(e, c); });
    RegisterEffectHandler("stun",             [this](const SkillEffectSpec& e, const SkillExecContext& c) { HandleStun(e, c); });
    RegisterEffectHandler("heal",             [this](const SkillEffectSpec& e, const SkillExecContext& c) { HandleHeal(e, c); });
    RegisterEffectHandler("shield",           [this](const SkillEffectSpec& e, const SkillExecContext& c) { HandleShield(e, c); });
    RegisterEffectHandler("buff_stat",        [this](const SkillEffectSpec& e, const SkillExecContext& c) { HandleBuffStat(e, c); });
    RegisterEffectHandler("dash_impulse",     [this](const SkillEffectSpec& e, const SkillExecContext& c) { HandleDashImpulse(e, c); });
    RegisterEffectHandler("spawn_projectile", [this](const SkillEffectSpec& e, const SkillExecContext& c) { HandleSpawnProjectile(e, c); });
    RegisterEffectHandler("spawn_zone",       [this](const SkillEffectSpec& e, const SkillExecContext& c) { HandleSpawnZone(e, c); });
    RegisterEffectHandler("vfx",              [this](const SkillEffectSpec& e, const SkillExecContext& c) { HandleVfx(e, c); });
    RegisterEffectHandler("restore_resource", [this](const SkillEffectSpec& e, const SkillExecContext& c) { HandleRestoreResource(e, c); });
}

void SkillManager::Reset() {
    const auto& data = GameManager::Get().Data();
    const auto& cfg = data.GetSkillConfig();

    m_unlockedSlotCount = std::clamp(cfg.defaultEquipSlots, 1, std::clamp(cfg.maxEquipSlots, 1, 10));
    m_equippedSlots.assign(static_cast<size_t>(m_unlockedSlotCount), "");
    m_slotCooldowns.assign(static_cast<size_t>(m_unlockedSlotCount), 0.0f);
    m_unlockedSkills.clear();

    int startLvl = data.GetPlayerSpec().startLevel;
    RefreshUnlocksForLevel(startLvl, false);

    for (size_t i = 0; i < cfg.defaultEquipped.size() && i < m_equippedSlots.size(); ++i) {
        const std::string& sid = cfg.defaultEquipped[i];
        if (data.GetSkillSpec(sid)) {
            UnlockSkill(sid, false);
            m_equippedSlots[i] = sid;
        }
    }
}

int SkillManager::GetMaxSlotCount() const {
    return std::clamp(GameManager::Get().Data().GetSkillConfig().maxEquipSlots, 1, 10);
}

int SkillManager::GetSlotExpandGoldCost() const {
    return GameManager::Get().Data().GetSkillConfig().slotExpandGoldCost;
}

void SkillManager::RefreshUnlocksForLevel(int playerLevel, bool showToast) {
    const auto& data = GameManager::Get().Data();
    for (const auto& s : data.GetAllSkills()) {
        if (s.unlockLevel <= playerLevel) {
            UnlockSkill(s.id, showToast);
        }
    }
}

bool SkillManager::UnlockSkill(const std::string& skillId, bool showToast) {
    const auto& data = GameManager::Get().Data();
    const SkillSpec* spec = data.GetSkillSpec(skillId);
    if (!spec) return false;
    if (IsSkillUnlocked(skillId)) return false;

    m_unlockedSkills.push_back(skillId);
    if (showToast) {
        GameManager::Get().AddToast(data.FormatText("toast.skill_unlocked", {spec->name}), SKYBLUE, 3.2f);
    }
    return true;
}

bool SkillManager::IsSkillUnlocked(const std::string& skillId) const {
    return std::find(m_unlockedSkills.begin(), m_unlockedSkills.end(), skillId) != m_unlockedSkills.end();
}

bool SkillManager::SetUnlockedSlotCount(int count) {
    int maxSlots = GetMaxSlotCount();
    int clamped = std::clamp(count, 1, maxSlots);
    m_unlockedSlotCount = clamped;
    m_equippedSlots.resize(static_cast<size_t>(m_unlockedSlotCount), "");
    m_slotCooldowns.resize(static_cast<size_t>(m_unlockedSlotCount), 0.0f);
    return true;
}

bool SkillManager::ExpandEquipSlot(bool spendGold, bool showToast) {
    auto& gm = GameManager::Get();
    const auto& data = gm.Data();
    int maxSlots = GetMaxSlotCount();

    if (m_unlockedSlotCount >= maxSlots) {
        if (showToast) {
            gm.AddToast(data.FormatText("toast.skill_slot_max", {std::to_string(maxSlots)}), ORANGE, 2.2f);
        }
        return false;
    }

    int cost = GetSlotExpandGoldCost();
    if (spendGold && cost > 0) {
        if (gm.Economy().GetGold() < cost) {
            if (showToast) {
                gm.AddToast(data.FormatText("toast.gold_insufficient", {std::to_string(cost)}), RED, 2.2f);
            }
            return false;
        }
        gm.Economy().AddGold(-cost);
    }

    SetUnlockedSlotCount(m_unlockedSlotCount + 1);
    if (showToast) {
        gm.AddToast(data.FormatText("toast.skill_slot_expanded", {
            std::to_string(m_unlockedSlotCount), std::to_string(maxSlots)
        }), GOLD, 2.8f);
    }
    gm.Save().TriggerAutoSave();
    return true;
}

bool SkillManager::EquipSkill(int slotIdx, const std::string& skillId, bool showToast) {
    if (slotIdx < 0 || slotIdx >= m_unlockedSlotCount) return false;
    const auto& data = GameManager::Get().Data();
    const SkillSpec* spec = data.GetSkillSpec(skillId);
    if (!spec || !IsSkillUnlocked(skillId)) return false;

    int existingSlot = FindEquippedSlot(skillId);
    if (existingSlot == slotIdx) return true;

    // Auto-swap if already equipped in another slot
    if (existingSlot >= 0) {
        std::swap(m_equippedSlots[static_cast<size_t>(existingSlot)], m_equippedSlots[static_cast<size_t>(slotIdx)]);
        std::swap(m_slotCooldowns[static_cast<size_t>(existingSlot)], m_slotCooldowns[static_cast<size_t>(slotIdx)]);
    } else {
        m_equippedSlots[static_cast<size_t>(slotIdx)] = skillId;
        m_slotCooldowns[static_cast<size_t>(slotIdx)] = 0.0f;
    }

    if (showToast) {
        GameManager::Get().AddToast(data.FormatText("toast.skill_equipped", {
            std::to_string(slotIdx + 1), spec->name
        }), LIME, 2.0f);
    }
    GameManager::Get().Save().TriggerAutoSave();
    return true;
}

bool SkillManager::UnequipSlot(int slotIdx) {
    if (slotIdx < 0 || slotIdx >= m_unlockedSlotCount) return false;
    m_equippedSlots[static_cast<size_t>(slotIdx)].clear();
    m_slotCooldowns[static_cast<size_t>(slotIdx)] = 0.0f;
    GameManager::Get().Save().TriggerAutoSave();
    return true;
}

const std::string& SkillManager::GetEquippedSkillId(int slotIdx) const {
    static const std::string kEmpty;
    if (slotIdx < 0 || slotIdx >= static_cast<int>(m_equippedSlots.size())) return kEmpty;
    return m_equippedSlots[static_cast<size_t>(slotIdx)];
}

int SkillManager::FindEquippedSlot(const std::string& skillId) const {
    for (size_t i = 0; i < m_equippedSlots.size(); ++i) {
        if (m_equippedSlots[i] == skillId) return static_cast<int>(i);
    }
    return -1;
}

float SkillManager::GetSlotCooldown(int slotIdx) const {
    if (slotIdx < 0 || slotIdx >= static_cast<int>(m_slotCooldowns.size())) return 0.0f;
    return m_slotCooldowns[static_cast<size_t>(slotIdx)];
}

bool SkillManager::CanPayCost(const SkillSpec& spec) const {
    const auto& player = GameManager::Get().Entities().GetPlayer();
    switch (spec.costType) {
        case SkillCostType::None:   return true;
        case SkillCostType::Mp:     return player.mp >= static_cast<float>(spec.costAmount);
        case SkillCostType::Energy: return player.energy >= static_cast<float>(spec.costAmount);
        case SkillCostType::Hp:     return player.hp > spec.costAmount; // Cannot self-kill with HP cost
    }
    return true;
}

void SkillManager::ShowInsufficientCostFeedback(const SkillSpec& spec) const {
    const auto& data = GameManager::Get().Data();
    switch (spec.costType) {
        case SkillCostType::Mp:
            GameManager::Get().AddToast(data.FormatText("toast.skill_insufficient_mp", {std::to_string(spec.costAmount)}), SKYBLUE, 1.8f);
            break;
        case SkillCostType::Energy:
            GameManager::Get().AddToast(data.FormatText("toast.skill_insufficient_energy", {std::to_string(spec.costAmount)}), GOLD, 1.8f);
            break;
        case SkillCostType::Hp:
            GameManager::Get().AddToast(data.GetText("toast.skill_insufficient_hp"), RED, 1.8f);
            break;
        case SkillCostType::None:
            break;
    }
}

bool SkillManager::DeductCost(const SkillSpec& spec, bool showToast) {
    if (!CanPayCost(spec)) {
        if (showToast) ShowInsufficientCostFeedback(spec);
        return false;
    }
    auto& player = GameManager::Get().Entities().GetPlayerMutable();
    switch (spec.costType) {
        case SkillCostType::None:   break;
        case SkillCostType::Mp:     player.mp = std::max(0.0f, player.mp - static_cast<float>(spec.costAmount)); break;
        case SkillCostType::Energy: player.energy = std::max(0.0f, player.energy - static_cast<float>(spec.costAmount)); break;
        case SkillCostType::Hp:     player.hp = std::max(1, player.hp - spec.costAmount); break;
    }
    return true;
}

std::string SkillManager::FormatSkillCostText(const SkillSpec& spec) const {
    const auto& data = GameManager::Get().Data();
    switch (spec.costType) {
        case SkillCostType::Mp:     return data.FormatText("skill.cost_mp_fmt", {std::to_string(spec.costAmount)});
        case SkillCostType::Energy: return data.FormatText("skill.cost_energy_fmt", {std::to_string(spec.costAmount)});
        case SkillCostType::Hp:     return data.FormatText("skill.cost_hp_fmt", {std::to_string(spec.costAmount)});
        case SkillCostType::None:   return data.GetText("skill.cost_none");
    }
    return "";
}

bool SkillManager::TryCastSlot(int slotIdx) {
    if (slotIdx < 0 || slotIdx >= m_unlockedSlotCount) return false;
    const std::string& sid = m_equippedSlots[static_cast<size_t>(slotIdx)];
    if (sid.empty()) return false;
    if (m_slotCooldowns[static_cast<size_t>(slotIdx)] > 0.0f) return false;

    const SkillSpec* spec = GameManager::Get().Data().GetSkillSpec(sid);
    if (!spec) return false;

    auto& ent = GameManager::Get().Entities();
    auto& player = ent.GetPlayerMutable();
    if (player.hp <= 0) return false;
    if (player.status.IsStunned()) {
        GameManager::Get().AddToast(GameManager::Get().Data().GetText("toast.skill_stunned"), ORANGE, 1.6f);
        return false;
    }

    if (!DeductCost(*spec, true)) return false;

    m_slotCooldowns[static_cast<size_t>(slotIdx)] = spec->cooldown;
    const auto& pSpec = GameManager::Get().Data().GetPlayerSpec();
    player.attackTimer = pSpec.attackDuration;
    player.attackHitApplied = true;
    player.animState = AnimState::Attack;
    player.animFrame = 0;
    player.animTimer = 0.0f;

    SkillExecContext ctx;
    ctx.origin = player.pos;
    ctx.direction = SafeNorm(player.facingVec);
    ctx.casterAtk = std::max(1, static_cast<int>(std::round(static_cast<float>(ent.GetEffectivePlayerAtk()) * player.skillPowerMult)));
    ctx.fromPlayer = true;
    ExecuteEffects(spec->effects, ctx);
    return true;
}

bool SkillManager::TryCastSkillById(const std::string& skillId, bool ignoreCooldown) {
    int slotIdx = FindEquippedSlot(skillId);
    if (slotIdx >= 0 && !ignoreCooldown) {
        return TryCastSlot(slotIdx);
    }
    const SkillSpec* spec = GameManager::Get().Data().GetSkillSpec(skillId);
    if (!spec) return false;
    auto& ent = GameManager::Get().Entities();
    const auto& player = ent.GetPlayer();
    if (player.hp <= 0 || player.status.IsStunned()) return false;
    if (!DeductCost(*spec, true)) return false;

    if (slotIdx >= 0) {
        m_slotCooldowns[static_cast<size_t>(slotIdx)] = spec->cooldown;
    }

    SkillExecContext ctx;
    ctx.origin = player.pos;
    ctx.direction = SafeNorm(player.facingVec);
    ctx.casterAtk = std::max(1, static_cast<int>(std::round(static_cast<float>(ent.GetEffectivePlayerAtk()) * player.skillPowerMult)));
    ctx.fromPlayer = true;
    ExecuteEffects(spec->effects, ctx);
    return true;
}

void SkillManager::Update(float dt) {
    for (float& cd : m_slotCooldowns) {
        if (cd > 0.0f) cd = std::max(0.0f, cd - dt);
    }

    auto& gm = GameManager::Get();
    if (gm.GetScreen() != GameScreen::Playing) return;
    if (gm.UI().IsBlockingGameplayInput()) return;

    const auto& input = gm.Input();
    for (int i = 0; i < m_unlockedSlotCount && i < 10; ++i) {
        if (input.IsActionPressed(SkillSlotToInputAction(i))) {
            TryCastSlot(i);
            return;
        }
    }

    // Right mouse button casts primary equipped skill (Slot 1)
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && m_unlockedSlotCount > 0) {
        TryCastSlot(0);
    }
}

void SkillManager::ExecuteEffects(const std::vector<SkillEffectSpec>& effects, const SkillExecContext& ctx) {
    for (const auto& ef : effects) {
        auto it = m_effectRegistry.find(ef.type);
        if (it != m_effectRegistry.end()) {
            it->second(ef, ctx);
        } else {
            std::cerr << "[SkillManager] Unknown effect type: " << ef.type << "\n";
        }
    }
}

// =========================================================================
// Atomic Effect Handler Implementations (Reusable Building Blocks)
// =========================================================================

void SkillManager::HandleDamageAoe(const SkillEffectSpec& ef, const SkillExecContext& ctx) {
    auto& ent = GameManager::Get().Entities();
    auto& combat = GameManager::Get().Combat();
    int dmg = std::max(1, static_cast<int>(std::round(ctx.casterAtk * ef.atkMultiplier)) + ef.flatAmount);
    bool useCone = (ef.target == "front_cone");

    if (ctx.fromPlayer) {
        for (auto& m : ent.GetMonsters()) {
            if (!m.alive) continue;
            float rSum = ef.radius + m.radius;
            bool hit = useCone
                ? IsInsideCone(ctx.origin, ctx.direction, m.pos, rSum, ef.coneAngleDeg)
                : (Dist2(ctx.origin, m.pos) <= rSum * rSum);
            if (hit) {
                combat.ApplyDamageToMonster(m, dmg, ctx.origin, ef.knockback);
            }
        }
        auto& boss = ent.GetBoss();
        if (boss.active && boss.alive) {
            float rSum = ef.radius + boss.radius;
            bool hit = useCone
                ? IsInsideCone(ctx.origin, ctx.direction, boss.pos, rSum, ef.coneAngleDeg)
                : (Dist2(ctx.origin, boss.pos) <= rSum * rSum);
            if (hit) {
                combat.ApplyDamageToBoss(boss, dmg, ctx.origin, ef.knockback * 0.45f);
            }
        }
    } else {
        const auto& p = ent.GetPlayer();
        float rSum = ef.radius + p.radius;
        if (Dist2(ctx.origin, p.pos) <= rSum * rSum) {
            combat.DamagePlayer(dmg, ctx.origin);
        }
    }
}

void SkillManager::HandleDamageTarget(const SkillEffectSpec& ef, const SkillExecContext& ctx) {
    auto& ent = GameManager::Get().Entities();
    auto& combat = GameManager::Get().Combat();
    int dmg = std::max(1, static_cast<int>(std::round(ctx.casterAtk * ef.atkMultiplier)) + ef.flatAmount);

    if (ctx.fromPlayer) {
        if (ctx.hitMonsterId >= 0) {
            for (auto& m : ent.GetMonsters()) {
                if (m.alive && m.instanceId == ctx.hitMonsterId) {
                    combat.ApplyDamageToMonster(m, dmg, ctx.origin, ef.knockback);
                    break;
                }
            }
        }
        if (ctx.hitBoss) {
            auto& boss = ent.GetBoss();
            if (boss.active && boss.alive) {
                combat.ApplyDamageToBoss(boss, dmg, ctx.origin, ef.knockback * 0.45f);
            }
        }
    } else {
        combat.DamagePlayer(dmg, ctx.origin);
    }
}

void SkillManager::HandleStun(const SkillEffectSpec& ef, const SkillExecContext& ctx) {
    auto& ent = GameManager::Get().Entities();
    auto& combat = GameManager::Get().Combat();
    float dur = std::max(0.1f, ef.duration);

    if (!ctx.fromPlayer) {
        auto& p = ent.GetPlayerMutable();
        p.status.stunTimer = std::max(p.status.stunTimer, dur);
        combat.SpawnFloatingText(Vector2{p.pos.x, p.pos.y - 28.0f},
                                 GameManager::Get().Data().GetText("combat.stun_float"), YELLOW, 15.0f);
        return;
    }

    if (ef.target == "hit_target") {
        if (ctx.hitMonsterId >= 0) {
            for (auto& m : ent.GetMonsters()) {
                if (m.alive && m.instanceId == ctx.hitMonsterId) {
                    combat.ApplyStunToMonster(m, dur);
                    break;
                }
            }
        }
        if (ctx.hitBoss) {
            auto& boss = ent.GetBoss();
            if (boss.active && boss.alive) {
                combat.ApplyStunToBoss(boss, dur * 0.6f); // Bosses have partial CC resistance
            }
        }
        return;
    }

    bool useCone = (ef.target == "front_cone");
    for (auto& m : ent.GetMonsters()) {
        if (!m.alive) continue;
        float rSum = ef.radius + m.radius;
        bool hit = useCone
            ? IsInsideCone(ctx.origin, ctx.direction, m.pos, rSum, ef.coneAngleDeg)
            : (Dist2(ctx.origin, m.pos) <= rSum * rSum);
        if (hit) {
            combat.ApplyStunToMonster(m, dur);
        }
    }
    auto& boss = ent.GetBoss();
    if (boss.active && boss.alive) {
        float rSum = ef.radius + boss.radius;
        bool hit = useCone
            ? IsInsideCone(ctx.origin, ctx.direction, boss.pos, rSum, ef.coneAngleDeg)
            : (Dist2(ctx.origin, boss.pos) <= rSum * rSum);
        if (hit) {
            combat.ApplyStunToBoss(boss, dur * 0.6f);
        }
    }
}

void SkillManager::HandleHeal(const SkillEffectSpec& ef, const SkillExecContext& /*ctx*/) {
    auto& p = GameManager::Get().Entities().GetPlayerMutable();
    int amount = ef.flatAmount + static_cast<int>(std::round(p.maxHp * ef.maxHpRatio));
    if (amount <= 0) return;

    int healed = std::min(amount, p.maxHp - p.hp);
    p.hp = std::min(p.maxHp, p.hp + amount);
    GameManager::Get().Combat().SpawnFloatingText(
        Vector2{p.pos.x, p.pos.y - 24.0f},
        GameManager::Get().Data().FormatText("combat.hp_heal_float", {std::to_string(std::max(healed, amount))}),
        LIME, 16.0f);
}

void SkillManager::HandleShield(const SkillEffectSpec& ef, const SkillExecContext& /*ctx*/) {
    auto& p = GameManager::Get().Entities().GetPlayerMutable();
    int amount = ef.flatAmount + static_cast<int>(std::round(p.maxHp * ef.maxHpRatio));
    if (amount <= 0) return;

    p.status.shieldHp = std::max(p.status.shieldHp, amount);
    p.status.shieldMaxHp = std::max(p.status.shieldMaxHp, p.status.shieldHp);
    p.status.shieldTimer = std::max(p.status.shieldTimer, std::max(0.5f, ef.duration));
    GameManager::Get().Combat().SpawnFloatingText(
        Vector2{p.pos.x, p.pos.y - 34.0f},
        GameManager::Get().Data().FormatText("combat.shield_gain_float", {std::to_string(amount)}),
        SKYBLUE, 15.0f);
}

void SkillManager::HandleBuffStat(const SkillEffectSpec& ef, const SkillExecContext& /*ctx*/) {
    auto& p = GameManager::Get().Entities().GetPlayerMutable();
    p.status.atkBuffMult = std::max(p.status.atkBuffMult, ef.atkBuffMult);
    p.status.speedBuffMult = std::max(p.status.speedBuffMult, ef.speedBuffMult);
    p.status.buffTimer = std::max(p.status.buffTimer, std::max(0.5f, ef.duration));
    GameManager::Get().Combat().SpawnFloatingText(
        Vector2{p.pos.x, p.pos.y - 30.0f},
        GameManager::Get().Data().GetText("combat.buff_float"), GOLD, 15.0f);
}

void SkillManager::HandleDashImpulse(const SkillEffectSpec& ef, const SkillExecContext& ctx) {
    auto& p = GameManager::Get().Entities().GetPlayerMutable();
    p.dashDir = SafeNorm(ctx.direction);
    p.dashTimer = std::max(p.dashTimer, std::max(0.08f, ef.duration));
    if (ef.grantInvuln) {
        p.invulnTimer = std::max(p.invulnTimer, ef.duration + 0.08f);
    }
}

void SkillManager::HandleSpawnProjectile(const SkillEffectSpec& ef, const SkillExecContext& ctx) {
    auto& combat = GameManager::Get().Combat();
    int count = std::max(1, ef.count);
    int dmg = std::max(1, static_cast<int>(std::round(ctx.casterAtk * ef.atkMultiplier)) + ef.flatAmount);
    Vector2 baseDir = SafeNorm(ctx.direction);
    float baseAngle = std::atan2(baseDir.y, baseDir.x);
    float spreadRad = ef.spreadDeg * (PI / 180.0f);

    for (int i = 0; i < count; ++i) {
        float offset = (count == 1) ? 0.0f : (static_cast<float>(i) - (count - 1) * 0.5f) * spreadRad;
        float ang = baseAngle + offset;
        Vector2 vel{std::cos(ang) * ef.speed, std::sin(ang) * ef.speed};
        combat.SpawnProjectile(ctx.origin, vel, dmg, ctx.fromPlayer, ef.artId, ctx.casterAtk, ef.onHitEffects);
    }
}

void SkillManager::HandleSpawnZone(const SkillEffectSpec& ef, const SkillExecContext& ctx) {
    Vector2 pos = ctx.origin;
    if (ef.target == "aim_point") {
        Vector2 dir = SafeNorm(ctx.direction);
        pos.x += dir.x * ef.distance;
        pos.y += dir.y * ef.distance;
    }
    int dmg = std::max(0, static_cast<int>(std::round(ctx.casterAtk * ef.atkMultiplier)) + ef.flatAmount);
    GameManager::Get().Combat().SpawnTelegraphZone(
        pos, ef.radius, std::max(0.1f, ef.telegraphSec), dmg,
        ctx.fromPlayer, ctx.casterAtk, ef.onExplodeEffects);
}

void SkillManager::HandleVfx(const SkillEffectSpec& ef, const SkillExecContext& ctx) {
    float rot = std::atan2(ctx.direction.y, ctx.direction.x) * (180.0f / PI);
    if (ef.artId == 602) rot = 0.0f;
    GameManager::Get().Combat().SpawnVisualEffect(ctx.origin, rot, ef.scale, ef.artId, std::max(0.1f, ef.duration), ef.tint);
}

void SkillManager::HandleRestoreResource(const SkillEffectSpec& ef, const SkillExecContext& /*ctx*/) {
    auto& p = GameManager::Get().Entities().GetPlayerMutable();
    int amt = std::max(0, ef.flatAmount);
    if (amt <= 0) return;

    const auto& data = GameManager::Get().Data();
    auto& combat = GameManager::Get().Combat();
    if (ef.resourceType == "mp") {
        p.mp = std::min(static_cast<float>(p.maxMp), p.mp + static_cast<float>(amt));
        combat.SpawnFloatingText(Vector2{p.pos.x, p.pos.y - 26.0f},
                                 data.FormatText("combat.mp_restore_float", {std::to_string(amt)}),
                                 Color{90, 185, 255, 255}, 14.0f);
    } else if (ef.resourceType == "energy") {
        p.energy = std::min(static_cast<float>(p.maxEnergy), p.energy + static_cast<float>(amt));
        combat.SpawnFloatingText(Vector2{p.pos.x, p.pos.y - 26.0f},
                                 data.FormatText("combat.energy_restore_float", {std::to_string(amt)}),
                                 GOLD, 14.0f);
    } else if (ef.resourceType == "hp") {
        p.hp = std::min(p.maxHp, p.hp + amt);
        combat.SpawnFloatingText(Vector2{p.pos.x, p.pos.y - 26.0f},
                                 data.FormatText("combat.hp_heal_float", {std::to_string(amt)}),
                                 LIME, 14.0f);
    }
}

nlohmann::json SkillManager::Serialize() const {
    return {
        {"unlocked_slot_count", m_unlockedSlotCount},
        {"equipped_slots", m_equippedSlots},
        {"unlocked_skills", m_unlockedSkills}
    };
}

void SkillManager::Deserialize(const nlohmann::json& j) {
    Reset();
    if (!j.is_object()) return;
    const auto& data = GameManager::Get().Data();

    int slotCount = j.value("unlocked_slot_count", m_unlockedSlotCount);
    SetUnlockedSlotCount(slotCount);

    if (j.contains("unlocked_skills") && j["unlocked_skills"].is_array()) {
        for (const auto& sid : j["unlocked_skills"]) {
            UnlockSkill(sid.get<std::string>(), false);
        }
    }

    if (j.contains("equipped_slots") && j["equipped_slots"].is_array()) {
        m_equippedSlots.assign(static_cast<size_t>(m_unlockedSlotCount), "");
        m_slotCooldowns.assign(static_cast<size_t>(m_unlockedSlotCount), 0.0f);
        size_t idx = 0;
        for (const auto& sidJson : j["equipped_slots"]) {
            if (idx >= m_equippedSlots.size()) break;
            std::string sid = sidJson.get<std::string>();
            if (!sid.empty() && data.GetSkillSpec(sid)) {
                UnlockSkill(sid, false);
                m_equippedSlots[idx] = sid;
            }
            ++idx;
        }
    }
}
