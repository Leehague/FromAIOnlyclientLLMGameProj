#pragma once

#include "core/IManager.h"
#include "core/Types.h"
#include <string>
#include <vector>

// [Requirement 6] Quest state machine: Accept, track kill objectives, turn in & grant rewards.
class QuestManager : public IManager {
public:
    const char* GetName() const override { return "QuestManager"; }
    bool Init() override;
    void Update(float dt) override {}
    void Render() override {}
    void Shutdown() override;

    void Reset();

    QuestStatus GetQuestStatus(const std::string& questId) const;
    const QuestProgress* GetQuestProgress(const std::string& questId) const;
    const std::vector<QuestProgress>& GetAllProgress() const { return m_progress; }

    // Finds the most relevant quest to display when talking to an NPC
    std::string FindRelevantQuestForNpc(const std::string& npcId) const;
    bool NpcHasReadyTurnIn(const std::string& npcId) const;
    bool NpcHasAvailableOffer(const std::string& npcId) const;

    bool AcceptQuest(const std::string& questId);
    bool CompleteQuest(const std::string& questId);
    void NotifyTargetKilled(const std::string& targetId);

private:
    QuestProgress* FindProgressMutable(const std::string& questId);
    std::vector<QuestProgress> m_progress;
};
