#include "managers/UIManager.h"
#include "managers/DataManager.h"
#include "managers/ResourceManager.h"
#include "managers/MapManager.h"
#include "managers/EntityManager.h"
#include "managers/EconomyManager.h"
#include "managers/QuestManager.h"
#include "core/GameManager.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

void UIManager::CloseAllPanels() {
    m_dialogueOpen = false;
    m_shopOpen = false;
    m_inventoryOpen = false;
    m_questLogOpen = false;
}

bool UIManager::IsBlockingGameplayInput() const {
    return m_dialogueOpen || m_shopOpen || m_inventoryOpen || m_questLogOpen;
}

void UIManager::PushToast(const std::string& text, Color color, float duration) {
    m_toasts.push_back(ToastMessage{text, color, 0.0f, duration});
    if (m_toasts.size() > 4) {
        m_toasts.erase(m_toasts.begin());
    }
}

void UIManager::InteractWithNpc(const NpcSpec& npc) {
    CloseAllPanels();
    const auto& data = GameManager::Get().Data();

    if (npc.role == "healer") {
        auto& player = GameManager::Get().Entities().GetPlayerMutable();
        player.hp = player.maxHp;
        m_dialogueOpen = true;
        m_dialogueNpcName = npc.name;
        m_dialogueText = npc.greeting;
        m_dialogueQuestId.clear();
        m_dialogueQuestAction = 0;
        PushToast(data.FormatText("toast.healer_blessing", {npc.name}), LIME, 2.4f);
        return;
    }

    if (npc.role == "shop") {
        m_shopOpen = true;
        m_activeShopNpcId = npc.id;
        return;
    }

    // Quest NPC dialogue flow
    m_dialogueOpen = true;
    m_dialogueNpcName = npc.name;
    m_dialogueQuestId.clear();
    m_dialogueQuestAction = 0;

    auto& quests = GameManager::Get().Quests();
    std::string qid = quests.FindRelevantQuestForNpc(npc.id);
    if (qid.empty()) {
        m_dialogueText = data.FormatText("npc.all_quests_done", {npc.greeting});
        return;
    }

    const QuestSpec* qSpec = data.GetQuestSpec(qid);
    if (!qSpec) return;

    m_dialogueQuestId = qid;
    QuestStatus st = quests.GetQuestStatus(qid);
    if (st == QuestStatus::NotStarted) {
        m_dialogueText = data.FormatText("npc.quest_offer_fmt", {
            qSpec->title, qSpec->dialogueOffer,
            std::to_string(qSpec->rewardGold), std::to_string(qSpec->rewardExp)
        });
        m_dialogueQuestAction = 1;
    } else if (st == QuestStatus::Active) {
        m_dialogueText = data.FormatText("npc.quest_active_fmt", {qSpec->title, qSpec->dialogueProgress});
        m_dialogueQuestAction = 0;
    } else if (st == QuestStatus::ReadyToTurnIn) {
        m_dialogueText = data.FormatText("npc.quest_ready_fmt", {qSpec->title, qSpec->dialogueComplete});
        m_dialogueQuestAction = 2;
    }
}

void UIManager::Update(float dt) {
    // Update toast timers
    for (auto it = m_toasts.begin(); it != m_toasts.end();) {
        it->timer += dt;
        if (it->timer >= it->duration) it = m_toasts.erase(it);
        else ++it;
    }

    if (GameManager::Get().GetScreen() != GameScreen::Playing) return;

    if (IsKeyPressed(KEY_ESCAPE)) {
        CloseAllPanels();
        return;
    }

    if (!m_dialogueOpen && !m_shopOpen) {
        if (IsKeyPressed(KEY_I)) {
            m_inventoryOpen = !m_inventoryOpen;
            if (m_inventoryOpen) m_questLogOpen = false;
        }
        if (IsKeyPressed(KEY_Q)) {
            m_questLogOpen = !m_questLogOpen;
            if (m_questLogOpen) m_inventoryOpen = false;
        }
    }

    // Quick keyboard confirm inside dialogue
    if (m_dialogueOpen && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))) {
        if (m_dialogueQuestAction == 1) {
            GameManager::Get().Quests().AcceptQuest(m_dialogueQuestId);
        } else if (m_dialogueQuestAction == 2) {
            GameManager::Get().Quests().CompleteQuest(m_dialogueQuestId);
        }
        m_dialogueOpen = false;
    }
}

bool UIManager::DrawButton(Rectangle rect, const std::string& label, Color bg, Color hoverBg) const {
    Vector2 mp = GetMousePosition();
    bool hover = CheckCollisionPointRec(mp, rect);
    DrawRectangleRounded(rect, 0.22f, 6, hover ? hoverBg : bg);
    DrawRectangleRoundedLinesEx(rect, 0.22f, 6, 1.5f, Color{200, 185, 145, 220});

    const auto& res = GameManager::Get().Resources();
    float tw = res.MeasureTextKR(label, 16.0f);
    res.DrawTextKR(label, rect.x + (rect.width - tw) * 0.5f, rect.y + (rect.height - 16.0f) * 0.5f, 16.0f, WHITE);

    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void UIManager::RenderTitleScreen() const {
    auto& gm = GameManager::Get();
    const auto& res = gm.Resources();
    const auto& data = gm.Data();
    int sw = gm.GetScreenWidthPx();
    int sh = gm.GetScreenHeightPx();

    DrawRectangleGradientV(0, 0, sw, sh, Color{18, 22, 34, 255}, Color{38, 20, 26, 255});

    // Showcase boss sprite on title screen
    const Texture2D& bossTex = res.GetTexture(301);
    DrawTexturePro(bossTex, Rectangle{0, 0, 80, 80},
                   Rectangle{sw * 0.5f - 80.0f, 95.0f, 160.0f, 160.0f}, Vector2{0, 0}, 0.0f, WHITE);

    const std::string& title = data.GetText("title.main");
    const std::string& sub = data.GetText("title.subtitle");
    float tw = res.MeasureTextKR(title, 44.0f);
    float subW = res.MeasureTextKR(sub, 18.0f);
    res.DrawTextKR(title, (sw - tw) * 0.5f, 270.0f, 44.0f, GOLD);
    res.DrawTextKR(sub, (sw - subW) * 0.5f, 326.0f, 18.0f, Color{210, 205, 195, 255});

    // Controls summary card
    Rectangle card{sw * 0.5f - 325.0f, 375.0f, 650.0f, 175.0f};
    DrawRectangleRounded(card, 0.1f, 8, Color{15, 18, 28, 210});
    DrawRectangleRoundedLinesEx(card, 0.1f, 8, 1.5f, Color{160, 140, 95, 200});

    res.DrawTextKR(data.GetText("title.guide_1"), card.x + 24, card.y + 18, 17.0f, RAYWHITE);
    res.DrawTextKR(data.GetText("title.guide_2"), card.x + 24, card.y + 48, 17.0f, RAYWHITE);
    res.DrawTextKR(data.GetText("title.guide_3"), card.x + 24, card.y + 78, 17.0f, RAYWHITE);
    res.DrawTextKR(data.GetText("title.guide_4"), card.x + 24, card.y + 108, 17.0f, Color{180, 225, 255, 255});
    res.DrawTextKR(data.GetText("title.guide_5"), card.x + 24, card.y + 138, 15.0f, Color{190, 210, 180, 255});

    Rectangle startBtn{sw * 0.5f - 130.0f, 585.0f, 260.0f, 52.0f};
    if (DrawButton(startBtn, data.GetText("title.start_btn"), Color{140, 48, 42, 255}, Color{185, 65, 55, 255})) {
        GameManager::Get().StartNewGame();
    }
}

void UIManager::RenderHUD() const {
    auto& gm = GameManager::Get();
    const auto& data = gm.Data();
    const auto& res = gm.Resources();
    const auto& ent = gm.Entities();
    const auto& player = ent.GetPlayer();
    const auto& eco = gm.Economy();
    const auto& mapMgr = gm.Map();

    // --- Top-Left Player Status Card ---
    Rectangle card{14, 14, 285, 134};
    DrawRectangleRounded(card, 0.12f, 8, Color{14, 16, 24, 215});
    DrawRectangleRoundedLinesEx(card, 0.12f, 8, 1.5f, Color{130, 118, 92, 220});

    std::string header = "Lv." + std::to_string(player.level) + "  " + data.GetPlayerSpec().name;
    res.DrawTextKR(header, card.x + 12, card.y + 8, 18.0f, GOLD);

    // HP Bar
    float hpRatio = std::clamp(static_cast<float>(player.hp) / static_cast<float>(std::max(1, player.maxHp)), 0.0f, 1.0f);
    DrawRectangleRounded(Rectangle{card.x + 12, card.y + 34, 260, 18}, 0.25f, 4, Color{45, 20, 22, 255});
    DrawRectangleRounded(Rectangle{card.x + 12, card.y + 34, 260 * hpRatio, 18}, 0.25f, 4, Color{215, 48, 52, 255});
    res.DrawTextKR("HP " + std::to_string(player.hp) + " / " + std::to_string(player.maxHp), card.x + 18, card.y + 35, 14.0f, WHITE);

    // EXP Bar
    int needExp = ent.GetExpToNextLevel();
    float expRatio = std::clamp(static_cast<float>(player.exp) / static_cast<float>(std::max(1, needExp)), 0.0f, 1.0f);
    DrawRectangleRounded(Rectangle{card.x + 12, card.y + 58, 260, 14}, 0.25f, 4, Color{24, 30, 48, 255});
    DrawRectangleRounded(Rectangle{card.x + 12, card.y + 58, 260 * expRatio, 14}, 0.25f, 4, Color{75, 155, 245, 255});
    res.DrawTextKR("EXP " + std::to_string(player.exp) + " / " + std::to_string(needExp), card.x + 18, card.y + 58, 12.0f, WHITE);

    // Stats & Gold
    std::string statLine = "ATK " + std::to_string(player.atk) + "   DEF " + std::to_string(player.def);
    res.DrawTextKR(statLine, card.x + 12, card.y + 80, 15.0f, RAYWHITE);

    const Texture2D& coinTex = res.GetTexture(604);
    DrawTexturePro(coinTex, Rectangle{0, 0, (float)coinTex.width, (float)coinTex.height},
                   Rectangle{card.x + 175, card.y + 78, 18, 18}, Vector2{0, 0}, 0.0f, WHITE);
    res.DrawTextKR(std::to_string(eco.GetGold()) + " G", card.x + 198, card.y + 79, 16.0f, GOLD);

    // 8-Direction Live Compass Indicator
    std::string dirLine = data.FormatText("hud.direction_fmt", {data.GetDirectionName(player.dir)});
    res.DrawTextKR(dirLine, card.x + 12, card.y + 106, 14.0f, SKYBLUE);
    Vector2 cc{card.x + 252, card.y + 114};
    DrawCircleLines(static_cast<int>(cc.x), static_cast<int>(cc.y), 11.0f, Color{120, 160, 210, 180});
    DrawCircle(static_cast<int>(cc.x + player.facingVec.x * 8.0f),
               static_cast<int>(cc.y + player.facingVec.y * 8.0f), 3.5f, GOLD);

    // --- Top-Center Current Map Banner ---
    if (const MapSpec* ms = mapMgr.GetCurrentMapSpec()) {
        int sw = gm.GetScreenWidthPx();
        std::string zoneTag = ms->safeZone
            ? data.GetText("hud.zone_safe")
            : (ms->type == "boss" ? data.GetText("hud.zone_boss") : data.GetText("hud.zone_field"));
        std::string mapTitle = ms->name + zoneTag;
        float mw = res.MeasureTextKR(mapTitle, 18.0f);
        Rectangle banner{(sw - mw) * 0.5f - 18, 14, mw + 36, 32};
        DrawRectangleRounded(banner, 0.25f, 6, Color{14, 16, 24, 215});
        DrawRectangleRoundedLinesEx(banner, 0.25f, 6, 1.5f, ms->type == "boss" ? RED : Color{130, 118, 92, 220});
        res.DrawTextKR(mapTitle, banner.x + 18, banner.y + 7, 18.0f, ms->type == "boss" ? Color{255, 130, 120, 255} : WHITE);
    }
}

void UIManager::RenderBossBar() const {
    const auto& boss = GameManager::Get().Entities().GetBoss();
    if (!boss.active || !boss.alive) return;
    const auto& data = GameManager::Get().Data();
    const BossSpec* spec = data.GetBossSpec(boss.specId);
    if (!spec) return;

    const auto& res = GameManager::Get().Resources();
    int sw = GameManager::Get().GetScreenWidthPx();
    float barW = 540.0f;
    float x = (sw - barW) * 0.5f;
    float y = 56.0f;

    DrawRectangleRounded(Rectangle{x - 10, y - 4, barW + 20, 48}, 0.15f, 6, Color{18, 10, 12, 225});
    DrawRectangleRoundedLinesEx(Rectangle{x - 10, y - 4, barW + 20, 48}, 0.15f, 6, 1.5f, boss.enraged ? ORANGE : RED);

    std::string label = data.FormatText("hud.boss_label_fmt", {spec->name, spec->title}) +
                        (boss.enraged ? data.GetText("hud.boss_enraged_tag") : "");
    res.DrawTextKR(label, x, y, 16.0f, boss.enraged ? ORANGE : Color{255, 210, 190, 255});

    float ratio = std::clamp(static_cast<float>(boss.hp) / static_cast<float>(std::max(1, boss.maxHp)), 0.0f, 1.0f);
    DrawRectangleRounded(Rectangle{x, y + 22, barW, 16}, 0.2f, 4, Color{45, 15, 18, 255});
    DrawRectangleRounded(Rectangle{x, y + 22, barW * ratio, 16}, 0.2f, 4, boss.enraged ? Color{240, 95, 25, 255} : Color{205, 35, 45, 255});

    std::string hpTxt = std::to_string(boss.hp) + " / " + std::to_string(boss.maxHp);
    float tw = res.MeasureTextKR(hpTxt, 13.0f);
    res.DrawTextKR(hpTxt, x + (barW - tw) * 0.5f, y + 23, 13.0f, WHITE);
}

void UIManager::RenderQuestTracker() const {
    auto& gm = GameManager::Get();
    const auto& res = gm.Resources();
    const auto& quests = gm.Quests();
    const auto& data = gm.Data();
    int sw = gm.GetScreenWidthPx();

    Rectangle box{static_cast<float>(sw - 280), 14.0f, 266.0f, 135.0f};
    DrawRectangleRounded(box, 0.12f, 8, Color{14, 16, 24, 205});
    DrawRectangleRoundedLinesEx(box, 0.12f, 8, 1.5f, Color{130, 118, 92, 200});

    res.DrawTextKR(data.GetText("hud.quest_tracker_title"), box.x + 12, box.y + 8, 16.0f, GOLD);

    float y = box.y + 32;
    bool shown = false;
    for (const auto& qp : quests.GetAllProgress()) {
        if (qp.status == QuestStatus::Active || qp.status == QuestStatus::ReadyToTurnIn) {
            const QuestSpec* qs = data.GetQuestSpec(qp.questId);
            if (!qs) continue;
            shown = true;
            std::string title = qs->title +
                                (qp.status == QuestStatus::ReadyToTurnIn ? data.GetText("hud.quest_ready_tag") : "");
            res.DrawTextKR(title, box.x + 12, y, 15.0f, qp.status == QuestStatus::ReadyToTurnIn ? LIME : WHITE);
            y += 22;
            for (size_t i = 0; i < qs->objectives.size(); ++i) {
                std::string obj = " - " + qs->objectives[i].targetName + ": " +
                                  std::to_string(qp.counts[i]) + " / " + std::to_string(qs->objectives[i].requiredCount);
                res.DrawTextKR(obj, box.x + 14, y, 14.0f, Color{205, 220, 240, 255});
                y += 20;
            }
            if (qp.status == QuestStatus::ReadyToTurnIn) {
                res.DrawTextKR(data.GetText("hud.quest_turnin_hint"), box.x + 14, y, 13.0f, GOLD);
            }
            break;
        }
    }

    if (!shown) {
        res.DrawTextKR(data.GetText("hud.quest_none"), box.x + 12, y, 14.0f, LIGHTGRAY);
        res.DrawTextKR(data.GetText("hud.quest_none_hint"), box.x + 12, y + 22, 13.0f, SKYBLUE);
    }
}

void UIManager::RenderMinimap() const {
    auto& gm = GameManager::Get();
    const auto& mapMgr = gm.Map();
    const auto& ent = gm.Entities();
    int mw = mapMgr.GetWidth();
    int mh = mapMgr.GetHeight();
    if (mw <= 0 || mh <= 0) return;

    int sw = gm.GetScreenWidthPx();
    float cell = 5.0f;
    float w = mw * cell;
    float h = mh * cell;
    float ox = static_cast<float>(sw) - w - 18.0f;
    float oy = 158.0f;

    DrawRectangleRounded(Rectangle{ox - 5, oy - 5, w + 10, h + 10}, 0.1f, 6, Color{10, 12, 18, 210});
    DrawRectangleRoundedLinesEx(Rectangle{ox - 5, oy - 5, w + 10, h + 10}, 0.1f, 6, 1.2f, Color{130, 118, 92, 200});

    for (int ty = 0; ty < mh; ++ty) {
        for (int tx = 0; tx < mw; ++tx) {
            int tid = mapMgr.GetTileAt(tx, ty);
            Color c{55, 110, 55, 255};
            if (tid == 1) c = Color{150, 120, 75, 255};
            else if (tid == 2) c = Color{150, 145, 135, 255};
            else if (tid == 3) c = Color{75, 80, 95, 255};
            else if (tid == 4) c = Color{155, 35, 40, 255};
            else if (tid == 5) c = Color{135, 90, 50, 255};
            else if (tid == 6 || tid == 7 || tid == 8 || tid == 9) c = Color{35, 38, 46, 255};
            else if (tid == 10) c = Color{45, 120, 175, 255};
            else if (tid == 11) c = Color{215, 75, 25, 255};
            else if (tid == 13) c = Color{80, 195, 235, 255};
            else if (tid == 14) c = Color{70, 200, 255, 255};
            else if (tid == 15) c = Color{255, 50, 55, 255};
            DrawRectangle(static_cast<int>(ox + tx * cell), static_cast<int>(oy + ty * cell),
                          static_cast<int>(cell), static_cast<int>(cell), c);
        }
    }

    float ts = static_cast<float>(mapMgr.GetTileSize());
    for (const auto& npc : ent.GetNpcs()) {
        DrawCircle(static_cast<int>(ox + (npc.pos.x / ts) * cell),
                   static_cast<int>(oy + (npc.pos.y / ts) * cell), 2.5f, YELLOW);
    }
    for (const auto& m : ent.GetMonsters()) {
        if (!m.alive) continue;
        DrawCircle(static_cast<int>(ox + (m.pos.x / ts) * cell),
                   static_cast<int>(oy + (m.pos.y / ts) * cell), 2.2f, RED);
    }
    if (ent.GetBoss().active && ent.GetBoss().alive) {
        DrawCircle(static_cast<int>(ox + (ent.GetBoss().pos.x / ts) * cell),
                   static_cast<int>(oy + (ent.GetBoss().pos.y / ts) * cell), 4.0f, MAGENTA);
    }
    const auto& p = ent.GetPlayer();
    DrawCircle(static_cast<int>(ox + (p.pos.x / ts) * cell),
               static_cast<int>(oy + (p.pos.y / ts) * cell), 3.0f, WHITE);
}

void UIManager::RenderActionBar() const {
    auto& gm = GameManager::Get();
    const auto& data = gm.Data();
    const auto& res = gm.Resources();
    const auto& player = gm.Entities().GetPlayer();
    const auto& pSpec = data.GetPlayerSpec();
    const auto& eco = gm.Economy();

    int sw = gm.GetScreenWidthPx();
    int sh = gm.GetScreenHeightPx();

    Rectangle bar{sw * 0.5f - 315.0f, static_cast<float>(sh - 64), 630.0f, 52.0f};
    DrawRectangleRounded(bar, 0.18f, 8, Color{14, 16, 24, 220});
    DrawRectangleRoundedLinesEx(bar, 0.18f, 8, 1.5f, Color{130, 118, 92, 220});

    struct SlotInfo {
        std::string key;
        std::string name;
        float cd;
        float maxCd;
        int artId;
        int count;
    };

    std::vector<SlotInfo> slots = {
        {data.GetText("action.key_attack"),  data.GetText("action.name_attack"),  player.attackCooldownTimer, pSpec.attackCooldown, 601, -1},
        {data.GetText("action.key_skill"),   data.GetText("action.name_skill"),   player.skillCooldownTimer,  pSpec.skillCooldown,  602, -1},
        {data.GetText("action.key_dash"),    data.GetText("action.name_dash"),    player.dashCooldownTimer,   pSpec.dashCooldown,   102, -1},
        {data.GetText("action.key_potion1"), data.GetText("action.name_potion1"), 0.0f, 1.0f, 501, eco.GetItemCount("potion_hp")},
        {data.GetText("action.key_potion2"), data.GetText("action.name_potion2"), 0.0f, 1.0f, 502, eco.GetItemCount("potion_hi")},
        {data.GetText("action.key_inv"),     data.GetText("action.name_inv"),     0.0f, 1.0f, 504, -1},
        {data.GetText("action.key_quest"),   data.GetText("action.name_quest"),   0.0f, 1.0f, 509, -1}
    };

    float sx = bar.x + 12.0f;
    for (const auto& s : slots) {
        Rectangle r{sx, bar.y + 6.0f, 82.0f, 40.0f};
        DrawRectangleRounded(r, 0.2f, 4, Color{28, 32, 46, 255});
        if (s.cd > 0.0f && s.maxCd > 0.0f) {
            float ratio = std::clamp(s.cd / s.maxCd, 0.0f, 1.0f);
            DrawRectangle(static_cast<int>(r.x), static_cast<int>(r.y + r.height * (1.0f - ratio)),
                          static_cast<int>(r.width), static_cast<int>(r.height * ratio), Color{0, 0, 0, 150});
        }
        DrawRectangleRoundedLinesEx(r, 0.2f, 4, 1.0f, Color{110, 105, 90, 200});

        res.DrawTextKR(s.key, r.x + 5, r.y + 3, 11.0f, GOLD);
        res.DrawTextKR(s.name, r.x + 5, r.y + 20, 13.0f, WHITE);

        if (s.count >= 0) {
            std::string cnt = "x" + std::to_string(s.count);
            res.DrawTextKR(cnt, r.x + 52, r.y + 20, 13.0f, s.count > 0 ? LIME : GRAY);
        } else if (s.cd > 0.0f) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.1f", s.cd);
            res.DrawTextKR(buf, r.x + 52, r.y + 4, 12.0f, ORANGE);
        }
        sx += 88.0f;
    }
}

void UIManager::RenderToasts() const {
    const auto& res = GameManager::Get().Resources();
    int sw = GameManager::Get().GetScreenWidthPx();
    float y = 115.0f;
    for (const auto& t : m_toasts) {
        float fade = 1.0f - std::clamp((t.timer - (t.duration - 0.6f)) / 0.6f, 0.0f, 1.0f);
        unsigned char a = static_cast<unsigned char>(255 * fade);
        float tw = res.MeasureTextKR(t.text, 16.0f);
        Rectangle r{(sw - tw) * 0.5f - 16, y, tw + 32, 30};
        DrawRectangleRounded(r, 0.25f, 6, Color{12, 14, 22, static_cast<unsigned char>(205 * fade)});
        Color c = t.color;
        c.a = a;
        res.DrawTextKR(t.text, r.x + 16, r.y + 7, 16.0f, c);
        y += 36.0f;
    }
}

void UIManager::RenderDialoguePanel() {
    if (!m_dialogueOpen) return;
    auto& gm = GameManager::Get();
    const auto& data = gm.Data();
    const auto& res = gm.Resources();
    int sw = gm.GetScreenWidthPx();
    int sh = gm.GetScreenHeightPx();

    Rectangle box{sw * 0.5f - 350.0f, static_cast<float>(sh - 240), 700.0f, 165.0f};
    DrawRectangleRounded(box, 0.1f, 8, Color{14, 16, 26, 240});
    DrawRectangleRoundedLinesEx(box, 0.1f, 8, 2.0f, GOLD);

    res.DrawTextKR(m_dialogueNpcName, box.x + 22, box.y + 14, 20.0f, GOLD);
    res.DrawTextKR(m_dialogueText, box.x + 22, box.y + 46, 16.0f, RAYWHITE);

    if (m_dialogueQuestAction == 1) {
        if (DrawButton(Rectangle{box.x + box.width - 255, box.y + box.height - 46, 115, 34},
                       data.GetText("ui.btn_accept_quest"), Color{45, 115, 60, 255}, Color{60, 150, 80, 255})) {
            gm.Quests().AcceptQuest(m_dialogueQuestId);
            m_dialogueOpen = false;
        }
    } else if (m_dialogueQuestAction == 2) {
        if (DrawButton(Rectangle{box.x + box.width - 255, box.y + box.height - 46, 115, 34},
                       data.GetText("ui.btn_claim_reward"), Color{165, 120, 30, 255}, Color{205, 155, 45, 255})) {
            gm.Quests().CompleteQuest(m_dialogueQuestId);
            m_dialogueOpen = false;
        }
    }

    if (DrawButton(Rectangle{box.x + box.width - 130, box.y + box.height - 46, 110, 34},
                   data.GetText("ui.btn_close"), Color{65, 65, 78, 255}, Color{95, 95, 112, 255})) {
        m_dialogueOpen = false;
    }
}

void UIManager::RenderShopPanel() {
    if (!m_shopOpen) return;
    auto& gm = GameManager::Get();
    const auto& res = gm.Resources();
    const auto& data = gm.Data();
    auto& eco = gm.Economy();
    const NpcSpec* npc = data.GetNpcSpec(m_activeShopNpcId);
    if (!npc) return;

    int sw = gm.GetScreenWidthPx();
    int sh = gm.GetScreenHeightPx();
    Rectangle panel{sw * 0.5f - 330.0f, sh * 0.5f - 250.0f, 660.0f, 500.0f};
    DrawRectangleRounded(panel, 0.08f, 8, Color{14, 16, 26, 244});
    DrawRectangleRoundedLinesEx(panel, 0.08f, 8, 2.0f, GOLD);

    res.DrawTextKR(data.FormatText("ui.shop_header_fmt", {npc->name, std::to_string(eco.GetGold())}),
                   panel.x + 22, panel.y + 16, 20.0f, GOLD);
    res.DrawTextKR(npc->greeting, panel.x + 22, panel.y + 44, 14.0f, LIGHTGRAY);

    float y = panel.y + 74.0f;
    for (const auto& itemId : npc->shopItems) {
        const ItemSpec* item = data.GetItemSpec(itemId);
        if (!item) continue;

        Rectangle row{panel.x + 20, y, panel.width - 40, 44};
        DrawRectangleRounded(row, 0.15f, 4, Color{25, 28, 42, 255});

        const Texture2D& icon = res.GetTexture(item->artId);
        DrawTexturePro(icon, Rectangle{0, 0, (float)icon.width, (float)icon.height},
                       Rectangle{row.x + 8, row.y + 6, 32, 32}, Vector2{0, 0}, 0.0f, WHITE);

        res.DrawTextKR(item->name, row.x + 48, row.y + 4, 16.0f, WHITE);
        res.DrawTextKR(item->description, row.x + 48, row.y + 24, 13.0f, Color{185, 195, 215, 255});

        bool ownedGear = (item->type == ItemType::Weapon || item->type == ItemType::Armor) && eco.GetItemCount(itemId) > 0;
        std::string btnLabel = ownedGear
            ? data.GetText("ui.shop_owned")
            : data.FormatText("ui.shop_buy_fmt", {std::to_string(item->price)});
        Color btnBg = ownedGear ? Color{60, 65, 75, 255}
                                : (eco.GetGold() >= item->price ? Color{45, 115, 65, 255} : Color{110, 45, 45, 255});
        if (DrawButton(Rectangle{row.x + row.width - 115, row.y + 6, 105, 32}, btnLabel, btnBg, Color{70, 145, 90, 255})) {
            if (!ownedGear) {
                eco.BuyItem(itemId);
            }
        }
        y += 49.0f;
    }

    if (DrawButton(Rectangle{panel.x + panel.width - 130, panel.y + panel.height - 44, 110, 32},
                   data.GetText("ui.btn_close"), Color{65, 65, 78, 255}, Color{95, 95, 112, 255})) {
        m_shopOpen = false;
    }
}

void UIManager::RenderInventoryPanel() {
    if (!m_inventoryOpen) return;
    auto& gm = GameManager::Get();
    const auto& res = gm.Resources();
    const auto& data = gm.Data();
    auto& eco = gm.Economy();

    int sw = gm.GetScreenWidthPx();
    int sh = gm.GetScreenHeightPx();
    Rectangle panel{sw * 0.5f - 300.0f, sh * 0.5f - 230.0f, 600.0f, 460.0f};
    DrawRectangleRounded(panel, 0.08f, 8, Color{14, 16, 26, 244});
    DrawRectangleRoundedLinesEx(panel, 0.08f, 8, 2.0f, SKYBLUE);

    res.DrawTextKR(data.GetText("ui.inv_title"), panel.x + 22, panel.y + 16, 20.0f, SKYBLUE);

    std::string eqW = eco.GetEquippedWeaponId().empty() ? data.GetText("ui.inv_none")
                      : data.GetItemSpec(eco.GetEquippedWeaponId())->name;
    std::string eqA = eco.GetEquippedArmorId().empty() ? data.GetText("ui.inv_none")
                      : data.GetItemSpec(eco.GetEquippedArmorId())->name;
    res.DrawTextKR(data.FormatText("ui.inv_equip_status_fmt", {eqW, eqA}),
                   panel.x + 22, panel.y + 46, 15.0f, GOLD);

    float y = panel.y + 78.0f;
    auto invCopy = eco.GetInventory();
    if (invCopy.empty()) {
        res.DrawTextKR(data.GetText("ui.inv_empty"), panel.x + 24, y, 16.0f, LIGHTGRAY);
    } else {
        for (const auto& slot : invCopy) {
            const ItemSpec* item = data.GetItemSpec(slot.itemId);
            if (!item) continue;
            Rectangle row{panel.x + 20, y, panel.width - 40, 44};
            DrawRectangleRounded(row, 0.15f, 4, Color{25, 28, 42, 255});

            const Texture2D& icon = res.GetTexture(item->artId);
            DrawTexturePro(icon, Rectangle{0, 0, (float)icon.width, (float)icon.height},
                           Rectangle{row.x + 8, row.y + 6, 32, 32}, Vector2{0, 0}, 0.0f, WHITE);

            bool equipped = (slot.itemId == eco.GetEquippedWeaponId() || slot.itemId == eco.GetEquippedArmorId());
            std::string title = item->name + " x" + std::to_string(slot.count) +
                                (equipped ? data.GetText("ui.inv_equipped_tag") : "");
            res.DrawTextKR(title, row.x + 48, row.y + 4, 16.0f, equipped ? GOLD : WHITE);
            res.DrawTextKR(item->description, row.x + 48, row.y + 24, 13.0f, Color{185, 195, 215, 255});

            if (item->type != ItemType::Quest && !equipped) {
                const std::string& act = (item->type == ItemType::Consumable)
                    ? data.GetText("ui.inv_use") : data.GetText("ui.inv_equip");
                if (DrawButton(Rectangle{row.x + row.width - 95, row.y + 6, 85, 32}, act,
                               Color{50, 95, 145, 255}, Color{70, 125, 185, 255})) {
                    eco.UseOrEquipItem(slot.itemId);
                }
            }
            y += 49.0f;
            if (y > panel.y + panel.height - 65.0f) break;
        }
    }

    if (DrawButton(Rectangle{panel.x + panel.width - 130, panel.y + panel.height - 44, 110, 32},
                   data.GetText("ui.btn_close"), Color{65, 65, 78, 255}, Color{95, 95, 112, 255})) {
        m_inventoryOpen = false;
    }
}

void UIManager::RenderQuestLogPanel() {
    if (!m_questLogOpen) return;
    auto& gm = GameManager::Get();
    const auto& res = gm.Resources();
    const auto& data = gm.Data();
    const auto& quests = gm.Quests();

    int sw = gm.GetScreenWidthPx();
    int sh = gm.GetScreenHeightPx();
    Rectangle panel{sw * 0.5f - 310.0f, sh * 0.5f - 220.0f, 620.0f, 440.0f};
    DrawRectangleRounded(panel, 0.08f, 8, Color{14, 16, 26, 244});
    DrawRectangleRoundedLinesEx(panel, 0.08f, 8, 2.0f, GOLD);

    res.DrawTextKR(data.GetText("ui.quest_log_title"), panel.x + 22, panel.y + 16, 20.0f, GOLD);

    float y = panel.y + 56.0f;
    for (const auto& qp : quests.GetAllProgress()) {
        const QuestSpec* qs = data.GetQuestSpec(qp.questId);
        if (!qs) continue;

        Rectangle card{panel.x + 20, y, panel.width - 40, 102};
        DrawRectangleRounded(card, 0.12f, 4, Color{25, 28, 42, 255});

        std::string stKey = "ui.quest_st_not_started";
        Color stColor = GRAY;
        if (qp.status == QuestStatus::Active) { stKey = "ui.quest_st_active"; stColor = SKYBLUE; }
        else if (qp.status == QuestStatus::ReadyToTurnIn) { stKey = "ui.quest_st_ready"; stColor = LIME; }
        else if (qp.status == QuestStatus::Completed) { stKey = "ui.quest_st_completed"; stColor = GOLD; }

        res.DrawTextKR(qs->title + "  " + data.GetText(stKey), card.x + 14, card.y + 8, 17.0f, stColor);
        res.DrawTextKR(qs->description, card.x + 14, card.y + 32, 13.0f, Color{200, 205, 215, 255});

        std::string objLine = data.GetText("ui.quest_progress_prefix");
        for (size_t i = 0; i < qs->objectives.size(); ++i) {
            if (i > 0) objLine += "  |  ";
            objLine += qs->objectives[i].targetName + " " +
                       std::to_string(qp.counts[i]) + "/" + std::to_string(qs->objectives[i].requiredCount);
        }
        res.DrawTextKR(objLine, card.x + 14, card.y + 56, 14.0f, WHITE);
        res.DrawTextKR(data.FormatText("ui.quest_reward_fmt", {
            std::to_string(qs->rewardGold), std::to_string(qs->rewardExp)
        }), card.x + 14, card.y + 78, 13.0f, GOLD);
        y += 110.0f;
    }

    if (DrawButton(Rectangle{panel.x + panel.width - 130, panel.y + panel.height - 44, 110, 32},
                   data.GetText("ui.btn_close"), Color{65, 65, 78, 255}, Color{95, 95, 112, 255})) {
        m_questLogOpen = false;
    }
}

void UIManager::RenderGameOverOverlay() const {
    auto& gm = GameManager::Get();
    const auto& data = gm.Data();
    const auto& res = gm.Resources();
    int sw = gm.GetScreenWidthPx();
    int sh = gm.GetScreenHeightPx();

    DrawRectangle(0, 0, sw, sh, Color{25, 5, 8, 195});
    const std::string& msg = data.GetText("gameover.title");
    float mw = res.MeasureTextKR(msg, 38.0f);
    res.DrawTextKR(msg, (sw - mw) * 0.5f, sh * 0.36f, 38.0f, RED);

    const std::string& sub = data.GetText("gameover.subtitle");
    float sw2 = res.MeasureTextKR(sub, 18.0f);
    res.DrawTextKR(sub, (sw - sw2) * 0.5f, sh * 0.46f, 18.0f, LIGHTGRAY);

    if (DrawButton(Rectangle{sw * 0.5f - 130.0f, sh * 0.56f, 260.0f, 48.0f},
                   data.GetText("gameover.respawn_btn"), Color{140, 45, 45, 255}, Color{185, 65, 65, 255})) {
        GameManager::Get().RespawnPlayerInTown();
    }
}

void UIManager::RenderVictoryOverlay() const {
    auto& gm = GameManager::Get();
    const auto& data = gm.Data();
    const auto& res = gm.Resources();
    int sw = gm.GetScreenWidthPx();
    int sh = gm.GetScreenHeightPx();

    DrawRectangle(0, 0, sw, sh, Color{10, 14, 24, 200});
    const Texture2D& crown = res.GetTexture(509);
    DrawTexturePro(crown, Rectangle{0, 0, (float)crown.width, (float)crown.height},
                   Rectangle{sw * 0.5f - 48.0f, sh * 0.2f, 96.0f, 96.0f}, Vector2{0, 0}, 0.0f, WHITE);

    const std::string& msg = data.GetText("victory.title");
    float mw = res.MeasureTextKR(msg, 34.0f);
    res.DrawTextKR(msg, (sw - mw) * 0.5f, sh * 0.38f, 34.0f, GOLD);

    const std::string& sub = data.GetText("victory.subtitle");
    float sw2 = res.MeasureTextKR(sub, 18.0f);
    res.DrawTextKR(sub, (sw - sw2) * 0.5f, sh * 0.47f, 18.0f, RAYWHITE);

    if (DrawButton(Rectangle{sw * 0.5f - 230.0f, sh * 0.57f, 210.0f, 46.0f},
                   data.GetText("victory.continue_btn"), Color{45, 115, 70, 255}, Color{65, 150, 95, 255})) {
        GameManager::Get().SetScreen(GameScreen::Playing);
    }
    if (DrawButton(Rectangle{sw * 0.5f + 20.0f, sh * 0.57f, 210.0f, 46.0f},
                   data.GetText("victory.restart_btn"), Color{125, 85, 35, 255}, Color{165, 115, 50, 255})) {
        GameManager::Get().StartNewGame();
    }
}

void UIManager::Render() {
    GameScreen screen = GameManager::Get().GetScreen();
    if (screen == GameScreen::Title) {
        RenderTitleScreen();
        return;
    }

    RenderHUD();
    RenderBossBar();
    RenderQuestTracker();
    RenderMinimap();
    RenderActionBar();
    RenderToasts();

    RenderDialoguePanel();
    RenderShopPanel();
    RenderInventoryPanel();
    RenderQuestLogPanel();

    if (screen == GameScreen::GameOver) RenderGameOverOverlay();
    else if (screen == GameScreen::Victory) RenderVictoryOverlay();
}
