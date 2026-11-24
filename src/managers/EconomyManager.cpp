#include "managers/EconomyManager.h"
#include "managers/DataManager.h"
#include "managers/EntityManager.h"
#include "core/GameManager.h"

#include <algorithm>

bool EconomyManager::Init() {
    Reset();
    return true;
}

void EconomyManager::Shutdown() {
    m_inventory.clear();
}

void EconomyManager::Reset() {
    const auto& pSpec = GameManager::Get().Data().GetPlayerSpec();
    m_gold = pSpec.startGold;
    m_inventory.clear();
    m_equippedWeaponId.clear();
    m_equippedArmorId.clear();

    for (const auto& entry : pSpec.startItems) {
        AddItem(entry.itemId, entry.count);
    }
}

void EconomyManager::AddGold(int amount) {
    if (amount <= 0) return;
    m_gold += amount;
}

bool EconomyManager::SpendGold(int amount) {
    if (amount < 0 || m_gold < amount) return false;
    m_gold -= amount;
    return true;
}

void EconomyManager::AddItem(const std::string& itemId, int count) {
    if (count <= 0) return;
    for (auto& slot : m_inventory) {
        if (slot.itemId == itemId) {
            slot.count += count;
            return;
        }
    }
    m_inventory.push_back(InventorySlot{itemId, count});
}

bool EconomyManager::RemoveItem(const std::string& itemId, int count) {
    if (count <= 0) return true;
    for (auto it = m_inventory.begin(); it != m_inventory.end(); ++it) {
        if (it->itemId == itemId) {
            if (it->count < count) return false;
            it->count -= count;
            if (it->count <= 0) {
                m_inventory.erase(it);
            }
            return true;
        }
    }
    return false;
}

int EconomyManager::GetItemCount(const std::string& itemId) const {
    for (const auto& slot : m_inventory) {
        if (slot.itemId == itemId) return slot.count;
    }
    return 0;
}

bool EconomyManager::BuyItem(const std::string& itemId) {
    const auto& data = GameManager::Get().Data();
    const ItemSpec* spec = data.GetItemSpec(itemId);
    if (!spec) return false;
    if (m_gold < spec->price) {
        GameManager::Get().AddToast(data.FormatText("toast.gold_insufficient", {std::to_string(spec->price)}), RED, 2.0f);
        return false;
    }

    // Prevent buying duplicate equipment already owned
    if ((spec->type == ItemType::Weapon || spec->type == ItemType::Armor) && GetItemCount(itemId) > 0) {
        GameManager::Get().AddToast(data.GetText("toast.already_owned_gear"), ORANGE, 2.0f);
        return false;
    }

    SpendGold(spec->price);
    AddItem(itemId, 1);

    // Auto-equip newly bought weapon/armor for smooth RPG flow
    if (spec->type == ItemType::Weapon || spec->type == ItemType::Armor) {
        UseOrEquipItem(itemId);
        GameManager::Get().AddToast(data.FormatText("toast.bought_and_equipped", {spec->name}), GOLD, 2.4f);
    } else {
        GameManager::Get().AddToast(data.FormatText("toast.bought_item", {spec->name}), GREEN, 2.0f);
    }
    return true;
}

bool EconomyManager::UseOrEquipItem(const std::string& itemId) {
    if (GetItemCount(itemId) <= 0) return false;
    const auto& data = GameManager::Get().Data();
    const ItemSpec* spec = data.GetItemSpec(itemId);
    if (!spec) return false;

    auto& entities = GameManager::Get().Entities();
    auto& player = entities.GetPlayerMutable();

    if (spec->type == ItemType::Consumable) {
        if (player.hp >= player.maxHp) {
            GameManager::Get().AddToast(data.GetText("toast.hp_already_full"), SKYBLUE, 1.8f);
            return false;
        }
        RemoveItem(itemId, 1);
        int before = player.hp;
        player.hp = std::min(player.maxHp, player.hp + spec->healHp);
        int healed = player.hp - before;
        GameManager::Get().AddToast(data.FormatText("toast.potion_used", {spec->name, std::to_string(healed)}), LIME, 1.8f);
        return true;
    }
    if (spec->type == ItemType::Weapon) {
        m_equippedWeaponId = itemId;
        entities.RecalculatePlayerStats();
        GameManager::Get().AddToast(data.FormatText("toast.equip_weapon", {spec->name}), GOLD, 2.0f);
        return true;
    }
    if (spec->type == ItemType::Armor) {
        m_equippedArmorId = itemId;
        entities.RecalculatePlayerStats();
        GameManager::Get().AddToast(data.FormatText("toast.equip_armor", {spec->name}), GOLD, 2.0f);
        return true;
    }
    return false;
}

int EconomyManager::GetEquipmentAtkBonus() const {
    int total = 0;
    const auto& data = GameManager::Get().Data();
    if (!m_equippedWeaponId.empty()) {
        if (const auto* w = data.GetItemSpec(m_equippedWeaponId)) total += w->atkBonus;
    }
    if (!m_equippedArmorId.empty()) {
        if (const auto* a = data.GetItemSpec(m_equippedArmorId)) total += a->atkBonus;
    }
    return total;
}

int EconomyManager::GetEquipmentDefBonus() const {
    int total = 0;
    const auto& data = GameManager::Get().Data();
    if (!m_equippedWeaponId.empty()) {
        if (const auto* w = data.GetItemSpec(m_equippedWeaponId)) total += w->defBonus;
    }
    if (!m_equippedArmorId.empty()) {
        if (const auto* a = data.GetItemSpec(m_equippedArmorId)) total += a->defBonus;
    }
    return total;
}

int EconomyManager::GetEquipmentHpBonus() const {
    int total = 0;
    const auto& data = GameManager::Get().Data();
    if (!m_equippedWeaponId.empty()) {
        if (const auto* w = data.GetItemSpec(m_equippedWeaponId)) total += w->hpBonus;
    }
    if (!m_equippedArmorId.empty()) {
        if (const auto* a = data.GetItemSpec(m_equippedArmorId)) total += a->hpBonus;
    }
    return total;
}
