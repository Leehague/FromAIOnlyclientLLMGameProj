#pragma once

#include "core/IManager.h"
#include "core/Types.h"
#include <nlohmann/json.hpp>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// Data-Driven Composite + Atomic Effect Registry Skill System.
// Designers compose new skills purely in data/skills.json using registered atomic effect handlers
// (damage_aoe, damage_target, stun, heal, shield, buff_stat, dash_impulse, spawn_projectile, spawn_zone, vfx, restore_resource).
// Supports resource costs (MP / Energy / HP) and dynamically expandable equip slots (default 3, max 10).
class SkillManager : public IManager {
public:
    using SkillEffectHandler = std::function<void(const SkillEffectSpec&, const SkillExecContext&)>;

    const char* GetName() const override { return "SkillManager"; }
    bool Init() override;
    void Update(float dt) override;
    void Render() override {}
    void Shutdown() override;

    void Reset();
    void RegisterEffectHandler(const std::string& effectType, SkillEffectHandler handler);
    bool HasEffectHandler(const std::string& effectType) const;
    void ExecuteEffects(const std::vector<SkillEffectSpec>& effects, const SkillExecContext& ctx);

    // Skill unlocking & expandable equip slots (default 3, max 10)
    void RefreshUnlocksForLevel(int playerLevel, bool showToast = false);
    bool UnlockSkill(const std::string& skillId, bool showToast = false);
    bool IsSkillUnlocked(const std::string& skillId) const;
    const std::vector<std::string>& GetUnlockedSkillIds() const { return m_unlockedSkills; }

    int GetUnlockedSlotCount() const { return m_unlockedSlotCount; }
    int GetMaxSlotCount() const;
    int GetSlotExpandGoldCost() const;
    bool ExpandEquipSlot(bool spendGold = true, bool showToast = true);
    bool SetUnlockedSlotCount(int count);

    bool EquipSkill(int slotIdx, const std::string& skillId, bool showToast = false);
    bool UnequipSlot(int slotIdx);
    const std::string& GetEquippedSkillId(int slotIdx) const;
    int FindEquippedSlot(const std::string& skillId) const;
    float GetSlotCooldown(int slotIdx) const;

    // Resource cost & casting
    bool CanPayCost(const SkillSpec& spec) const;
    bool TryCastSlot(int slotIdx);
    bool TryCastSkillById(const std::string& skillId, bool ignoreCooldown = false);
    std::string FormatSkillCostText(const SkillSpec& spec) const;

    // Memento / Snapshot serialization
    nlohmann::json Serialize() const;
    void Deserialize(const nlohmann::json& j);

private:
    void RegisterBuiltinEffects();
    bool DeductCost(const SkillSpec& spec, bool showToast);
    void ShowInsufficientCostFeedback(const SkillSpec& spec) const;

    // Atomic Effect Handlers
    void HandleDamageAoe(const SkillEffectSpec& ef, const SkillExecContext& ctx);
    void HandleDamageTarget(const SkillEffectSpec& ef, const SkillExecContext& ctx);
    void HandleStun(const SkillEffectSpec& ef, const SkillExecContext& ctx);
    void HandleHeal(const SkillEffectSpec& ef, const SkillExecContext& ctx);
    void HandleShield(const SkillEffectSpec& ef, const SkillExecContext& ctx);
    void HandleBuffStat(const SkillEffectSpec& ef, const SkillExecContext& ctx);
    void HandleDashImpulse(const SkillEffectSpec& ef, const SkillExecContext& ctx);
    void HandleSpawnProjectile(const SkillEffectSpec& ef, const SkillExecContext& ctx);
    void HandleSpawnZone(const SkillEffectSpec& ef, const SkillExecContext& ctx);
    void HandleVfx(const SkillEffectSpec& ef, const SkillExecContext& ctx);
    void HandleRestoreResource(const SkillEffectSpec& ef, const SkillExecContext& ctx);

    std::unordered_map<std::string, SkillEffectHandler> m_effectRegistry;
    int m_unlockedSlotCount = 3;
    std::vector<std::string> m_equippedSlots;
    std::vector<float> m_slotCooldowns;
    std::vector<std::string> m_unlockedSkills;
};
