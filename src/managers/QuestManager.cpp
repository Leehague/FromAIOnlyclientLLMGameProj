#include "managers/QuestManager.h"
#include "managers/DataManager.h"
#include "managers/EconomyManager.h"
#include "managers/EntityManager.h"
#include "core/GameManager.h"

#include <algorithm>

bool QuestManager::Init() {
    Reset();
    return true;
}

void QuestManager::Shutdown() {
    m_progress.clear();
}

void QuestManager::Reset() {
    m_progress.clear();
    for (const auto& q : GameManager::Get().Data().GetAllQuests()) {
        QuestProgress qp;
        qp.questId = q.id;
        qp.status = QuestStatus::NotStarted;
        qp.counts.assign(q.objectives.size(), 0);
        m_progress.push_back(std::move(qp));
    }
}

QuestProgress* QuestManager::FindProgressMutable(const std::string& questId) {
    for (auto& qp : m_progress) {
        if (qp.questId == questId) return &qp;
    }
    return nullptr;
}

const QuestProgress* QuestManager::GetQuestProgress(const std::string& questId) const {
    for (const auto& qp : m_progress) {
        if (qp.questId == questId) return &qp;
    }
    return nullptr;
}

QuestStatus QuestManager::GetQuestStatus(const std::string& questId) const {
    const auto* qp = GetQuestProgress(questId);
    return qp ? qp->status : QuestStatus::NotStarted;
}

bool QuestManager::NpcHasReadyTurnIn(const std::string& npcId) const {
    const auto& data = GameManager::Get().Data();
    for (const auto& q : data.GetAllQuests()) {
        if (q.giverNpcId == npcId && GetQuestStatus(q.id) == QuestStatus::ReadyToTurnIn) {
            return true;
        }
    }
    return false;
}

bool QuestManager::NpcHasAvailableOffer(const std::string& npcId) const {
    const auto& data = GameManager::Get().Data();
    for (const auto& q : data.GetAllQuests()) {
        if (q.giverNpcId != npcId) continue;
        if (GetQuestStatus(q.id) != QuestStatus::NotStarted) continue;
        if (q.prereqQuestId.empty() || GetQuestStatus(q.prereqQuestId) == QuestStatus::Completed) {
            return true;
        }
    }
    return false;
}

std::string QuestManager::FindRelevantQuestForNpc(const std::string& npcId) const {
    const auto& data = GameManager::Get().Data();
    // 1. Ready to turn in
    for (const auto& q : data.GetAllQuests()) {
        if (q.giverNpcId == npcId && GetQuestStatus(q.id) == QuestStatus::ReadyToTurnIn) {
            return q.id;
        }
    }
    // 2. Available to accept
    for (const auto& q : data.GetAllQuests()) {
        if (q.giverNpcId != npcId) continue;
        if (GetQuestStatus(q.id) != QuestStatus::NotStarted) continue;
        if (q.prereqQuestId.empty() || GetQuestStatus(q.prereqQuestId) == QuestStatus::Completed) {
            return q.id;
        }
    }
    // 3. Currently active
    for (const auto& q : data.GetAllQuests()) {
        if (q.giverNpcId == npcId && GetQuestStatus(q.id) == QuestStatus::Active) {
            return q.id;
        }
    }
    return "";
}

bool QuestManager::AcceptQuest(const std::string& questId) {
    const auto& data = GameManager::Get().Data();
    const QuestSpec* spec = data.GetQuestSpec(questId);
    QuestProgress* qp = FindProgressMutable(questId);
    if (!spec || !qp || qp->status != QuestStatus::NotStarted) return false;

    qp->status = QuestStatus::Active;
    GameManager::Get().AddToast(data.FormatText("toast.quest_accepted", {spec->title}), GOLD, 2.8f);
    return true;
}

bool QuestManager::CompleteQuest(const std::string& questId) {
    const auto& data = GameManager::Get().Data();
    const QuestSpec* spec = data.GetQuestSpec(questId);
    QuestProgress* qp = FindProgressMutable(questId);
    if (!spec || !qp || qp->status != QuestStatus::ReadyToTurnIn) return false;

    qp->status = QuestStatus::Completed;

    auto& eco = GameManager::Get().Economy();
    auto& ent = GameManager::Get().Entities();
    if (spec->rewardGold > 0) eco.AddGold(spec->rewardGold);
    if (spec->rewardExp > 0) ent.GainExp(spec->rewardExp);
    if (!spec->rewardItemId.empty() && spec->rewardItemCount > 0) {
        eco.AddItem(spec->rewardItemId, spec->rewardItemCount);
    }

    GameManager::Get().AddToast(
        data.FormatText("toast.quest_completed", {spec->title, std::to_string(spec->rewardGold)}),
        GOLD, 3.2f);

    if (questId == "q3_ashen_lord") {
        GameManager::Get().SetScreen(GameScreen::Victory);
    }
    return true;
}

void QuestManager::NotifyTargetKilled(const std::string& targetId) {
    const auto& data = GameManager::Get().Data();
    for (auto& qp : m_progress) {
        if (qp.status != QuestStatus::Active) continue;
        const QuestSpec* spec = data.GetQuestSpec(qp.questId);
        if (!spec) continue;

        bool changed = false;
        bool allDone = true;
        for (size_t i = 0; i < spec->objectives.size(); ++i) {
            if (spec->objectives[i].targetId == targetId && qp.counts[i] < spec->objectives[i].requiredCount) {
                qp.counts[i]++;
                changed = true;
            }
            if (qp.counts[i] < spec->objectives[i].requiredCount) {
                allDone = false;
            }
        }

        if (changed && allDone) {
            qp.status = QuestStatus::ReadyToTurnIn;
            GameManager::Get().AddToast(data.FormatText("toast.quest_ready", {spec->title}), LIME, 3.5f);
        }
    }
}
