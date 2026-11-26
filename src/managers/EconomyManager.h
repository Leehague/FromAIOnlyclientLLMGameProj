#pragma once

#include "core/IManager.h"
#include "core/Types.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// [Requirement 6] Economy system: Gold, Inventory, Equipment stats, Shop trading.
class EconomyManager : public IManager {
public:
    const char* GetName() const override { return "EconomyManager"; }
    bool Init() override;
    void Update(float dt) override {}
    void Render() override {}
    void Shutdown() override;

    void Reset();

    int GetGold() const { return m_gold; }
    void AddGold(int amount);
    bool SpendGold(int amount);

    void AddItem(const std::string& itemId, int count = 1);
    bool RemoveItem(const std::string& itemId, int count = 1);
    int GetItemCount(const std::string& itemId) const;
    const std::vector<InventorySlot>& GetInventory() const { return m_inventory; }

    bool BuyItem(const std::string& itemId);
    bool UseOrEquipItem(const std::string& itemId);

    const std::string& GetEquippedWeaponId() const { return m_equippedWeaponId; }
    const std::string& GetEquippedArmorId() const { return m_equippedArmorId; }

    int GetEquipmentAtkBonus() const;
    int GetEquipmentDefBonus() const;
    int GetEquipmentHpBonus() const;

    // Memento / Snapshot serialization
    nlohmann::json Serialize() const;
    void Deserialize(const nlohmann::json& j);

private:
    int m_gold = 0;
    std::vector<InventorySlot> m_inventory;
    std::string m_equippedWeaponId;
    std::string m_equippedArmorId;
};
