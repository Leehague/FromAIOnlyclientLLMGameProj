#pragma once

#include "core/IManager.h"
#include "core/Types.h"
#include <string>
#include <vector>

struct ToastMessage {
    std::string text;
    Color color = GOLD;
    float timer = 0.0f;
    float duration = 2.5f;
};

// Manages HUD, Minimap, Boss Bar, Quest Tracker, Dialogue, Shop, Inventory, and Screens.
class UIManager : public IManager {
public:
    const char* GetName() const override { return "UIManager"; }
    bool Init() override { return true; }
    void Update(float dt) override;
    void Render() override;
    void Shutdown() override { CloseAllPanels(); m_toasts.clear(); }

    void InteractWithNpc(const NpcSpec& npc);
    void CloseAllPanels();
    bool IsBlockingGameplayInput() const;
    void PushToast(const std::string& text, Color color = GOLD, float duration = 2.5f);

private:
    void RenderTitleScreen() const;
    void RenderHUD() const;
    void RenderBossBar() const;
    void RenderQuestTracker() const;
    void RenderMinimap() const;
    void RenderActionBar() const;
    void RenderToasts() const;

    void RenderDialoguePanel();
    void RenderShopPanel();
    void RenderInventoryPanel();
    void RenderQuestLogPanel();
    void RenderGameOverOverlay() const;
    void RenderVictoryOverlay() const;

    bool DrawButton(Rectangle rect, const std::string& label, Color bg, Color hoverBg) const;

    bool m_dialogueOpen = false;
    bool m_shopOpen = false;
    bool m_inventoryOpen = false;
    bool m_questLogOpen = false;

    std::string m_dialogueNpcName;
    std::string m_dialogueText;
    std::string m_dialogueQuestId;
    int m_dialogueQuestAction = 0; // 0=none, 1=accept, 2=complete
    std::string m_activeShopNpcId;

    std::vector<ToastMessage> m_toasts;
};
