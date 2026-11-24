#pragma once

#include "core/IManager.h"
#include "core/Types.h"
#include <string>
#include <unordered_map>
#include <vector>

class DataManager : public IManager {
public:
    const char* GetName() const override { return "DataManager"; }
    bool Init() override;
    void Update(float dt) override {}
    void Render() override {}
    void Shutdown() override;

    int GetTileSize() const { return m_tileSize; }
    const TileSpec* GetTileSpec(int tileId) const;
    const PlayerSpec& GetPlayerSpec() const { return m_playerSpec; }
    const ItemSpec* GetItemSpec(const std::string& id) const;
    const MonsterSpec* GetMonsterSpec(const std::string& id) const;
    const BossSpec* GetBossSpec(const std::string& id) const;
    const NpcSpec* GetNpcSpec(const std::string& id) const;
    const QuestSpec* GetQuestSpec(const std::string& id) const;
    const MapSpec* GetMapSpec(const std::string& id) const;

    const std::vector<NpcSpec>& GetAllNpcs() const { return m_npcs; }
    const std::vector<QuestSpec>& GetAllQuests() const { return m_quests; }
    const std::vector<ItemSpec>& GetAllItems() const { return m_items; }

    size_t GetTileCount() const { return m_tileMap.size(); }
    size_t GetItemCount() const { return m_itemMap.size(); }
    size_t GetMonsterCount() const { return m_monsterMap.size(); }
    size_t GetBossCount() const { return m_bossMap.size(); }
    size_t GetNpcCount() const { return m_npcMap.size(); }
    size_t GetQuestCount() const { return m_questMap.size(); }
    size_t GetMapCount() const { return m_mapMap.size(); }
    size_t GetStringCount() const { return m_strings.size(); }

    // Data-driven UI/message localization & font config (loaded from data/strings.json)
    const std::string& GetText(const std::string& key) const;
    std::string FormatText(const std::string& key, const std::vector<std::string>& args) const;
    const std::string& GetDirectionName(Direction8 dir) const;
    int GetFontSize() const { return m_fontSize; }
    const std::vector<std::string>& GetFontCandidates() const { return m_fontCandidates; }
    const std::string& GetFontExtraGlyphs() const { return m_fontExtraGlyphs; }

    const std::string& GetAllLoadedJsonText() const { return m_combinedJsonText; }

private:
    bool LoadStrings();
    bool LoadTiles();
    bool LoadPlayer();
    bool LoadItems();
    bool LoadMonsters();
    bool LoadBosses();
    bool LoadNpcs();
    bool LoadQuests();
    bool LoadMaps();

    bool ReadFileText(const std::string& relPath, std::string& outText);

    int m_tileSize = 32;
    PlayerSpec m_playerSpec;
    std::unordered_map<int, TileSpec> m_tileMap;
    std::vector<ItemSpec> m_items;
    std::unordered_map<std::string, size_t> m_itemMap;
    std::unordered_map<std::string, MonsterSpec> m_monsterMap;
    std::unordered_map<std::string, BossSpec> m_bossMap;
    std::vector<NpcSpec> m_npcs;
    std::unordered_map<std::string, size_t> m_npcMap;
    std::vector<QuestSpec> m_quests;
    std::unordered_map<std::string, size_t> m_questMap;
    std::unordered_map<std::string, MapSpec> m_mapMap;
    std::unordered_map<std::string, std::string> m_strings;
    int m_fontSize = 28;
    std::vector<std::string> m_fontCandidates;
    std::string m_fontExtraGlyphs;
    std::string m_combinedJsonText;
};
