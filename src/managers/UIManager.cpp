#include "managers/UIManager.h"
#include "managers/DataManager.h"
#include "managers/ResourceManager.h"
#include "managers/MapManager.h"
#include "managers/EntityManager.h"
#include "managers/EconomyManager.h"
#include "managers/QuestManager.h"
#include "managers/InputManager.h"
#include "managers/SaveManager.h"
#include "core/GameManager.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

static Rectangle WidgetRect(const HudWidgetSpec* w, Rectangle fallback) {
    if (!w) return fallback;
    return Rectangle{w->x, w->y, w->width, w->height};
}

bool UIManager::Init() {
    m_lobbyPlayerName = GameManager::Get().Data().GetPlayerSpec().name;
    m_nameInputFocused = false;
    m_settingsOpen = false;
    m_waitingRebind = false;
    return true;
}

void UIManager::PopLastUtf8Char(std::string& s) {
    if (s.empty()) return;
    size_t i = s.size() - 1;
    while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) {
        --i;
    }
    s.erase(i);
}

size_t UIManager::CountUtf8Chars(const std::string& s) {
    size_t count = 0;
    for (unsigned char c : s) {
        if ((c & 0xC0) != 0x80) ++count;
    }
    return count;
}

void UIManager::CloseAllPanels() {
    m_dialogueOpen = false;
    m_shopOpen = false;
    m_inventoryOpen = false;
    m_questLogOpen = false;
    m_settingsOpen = false;
    m_waitingRebind = false;
}

bool UIManager::IsBlockingGameplayInput() const {
    return m_dialogueOpen || m_shopOpen || m_inventoryOpen || m_questLogOpen || m_settingsOpen;
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
        GameManager::Get().Save().TriggerAutoSave();
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

void UIManager::UpdateLobbyNameInput(float dt) {
    m_cursorBlinkTimer += dt;
    if (!m_nameInputFocused) return;

    if (IsKeyPressed(KEY_ESCAPE)) {
        m_nameInputFocused = false;
        return;
    }

    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        PopLastUtf8Char(m_lobbyPlayerName);
    }

    bool added = false;
    while (int cp = GetCharPressed()) {
        if (cp < 32) continue;
        if (CountUtf8Chars(m_lobbyPlayerName) >= 14) break;
        int byteSize = 0;
        const char* utf8 = CodepointToUTF8(cp, &byteSize);
        if (utf8 && byteSize > 0) {
            m_lobbyPlayerName.append(utf8, static_cast<size_t>(byteSize));
            added = true;
        }
    }
    if (added) {
        GameManager::Get().Resources().EnsureGlyphsForText(m_lobbyPlayerName);
    }
}

void UIManager::Update(float dt) {
    // Update toast timers
    for (auto it = m_toasts.begin(); it != m_toasts.end();) {
        it->timer += dt;
        if (it->timer >= it->duration) it = m_toasts.erase(it);
        else ++it;
    }

    auto& gm = GameManager::Get();
    auto& input = gm.Input();

    // Settings panel input capture (works on BOTH Title screen and Playing screen)
    if (m_settingsOpen) {
        if (m_waitingRebind) {
            if (IsKeyPressed(KEY_ESCAPE)) {
                m_waitingRebind = false;
                return;
            }
            int key = GetKeyPressed();
            if (key > 0 && InputManager::IsBindableKey(key)) {
                input.SetPrimaryKey(m_rebindTargetAction, key, true);
                m_waitingRebind = false;
                PushToast(gm.Data().GetText("toast.keybind_saved"), LIME, 2.0f);
            }
            return;
        }
        if (IsKeyPressed(KEY_ESCAPE) || input.IsActionPressed(InputAction::Settings)) {
            m_settingsOpen = false;
        }
        return;
    }

    if (gm.GetScreen() == GameScreen::Title) {
        UpdateLobbyNameInput(dt);
        return;
    }

    if (gm.GetScreen() != GameScreen::Playing) return;

    if (IsKeyPressed(KEY_ESCAPE)) {
        CloseAllPanels();
        return;
    }

    if (!m_dialogueOpen && !m_shopOpen) {
        if (input.IsActionPressed(InputAction::Inventory)) {
            m_inventoryOpen = !m_inventoryOpen;
            if (m_inventoryOpen) { m_questLogOpen = false; m_settingsOpen = false; }
        }
        if (input.IsActionPressed(InputAction::QuestLog)) {
            m_questLogOpen = !m_questLogOpen;
            if (m_questLogOpen) { m_inventoryOpen = false; m_settingsOpen = false; }
        }
        if (input.IsActionPressed(InputAction::Settings)) {
            m_settingsOpen = !m_settingsOpen;
            if (m_settingsOpen) { m_inventoryOpen = false; m_questLogOpen = false; }
        }
    }

    // Quick keyboard confirm inside dialogue
    if (m_dialogueOpen && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))) {
        if (m_dialogueQuestAction == 1) {
            gm.Quests().AcceptQuest(m_dialogueQuestId);
        } else if (m_dialogueQuestAction == 2) {
            gm.Quests().CompleteQuest(m_dialogueQuestId);
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

void UIManager::RenderTitleScreen() {
    auto& gm = GameManager::Get();
    const auto& res = gm.Resources();
    const auto& data = gm.Data();
    const auto& lobby = data.GetUILayout().lobby;
    int sw = gm.GetScreenWidthPx();
    int sh = gm.GetScreenHeightPx();

    DrawRectangleGradientV(0, 0, sw, sh, lobby.bgTopColor, lobby.bgBottomColor);

    auto resolveElemText = [&](const LobbyElementSpec& e) -> std::string {
        if (!e.text.empty()) return e.text;
        if (!e.textKey.empty()) return data.GetText(e.textKey);
        return "";
    };

    bool clickedInsideNameBox = false;
    bool mousePressed = !m_settingsOpen && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    Vector2 mp = GetMousePosition();

    for (const auto& elem : lobby.elements) {
        if (!elem.visible) continue;
        Rectangle r{elem.x, elem.y, elem.width, elem.height};

        if (elem.type == "sprite") {
            const Texture2D& tex = res.GetTexture(elem.artId);
            float srcW = (elem.frameW > 0) ? static_cast<float>(elem.frameW) : static_cast<float>(tex.width);
            float srcH = (elem.frameH > 0) ? static_cast<float>(elem.frameH) : static_cast<float>(tex.height);
            DrawTexturePro(tex, Rectangle{0, 0, srcW, srcH}, r, Vector2{0, 0}, 0.0f, elem.color);
        } else if (elem.type == "panel") {
            DrawRectangleRounded(r, 0.1f, 8, elem.bgColor);
            DrawRectangleRoundedLinesEx(r, 0.1f, 8, 1.5f, elem.borderColor);
        } else if (elem.type == "text") {
            std::string txt = resolveElemText(elem);
            float tw = res.MeasureTextKR(txt, elem.fontSize);
            float drawX = r.x;
            if (elem.align == "center") drawX = r.x + (r.width - tw) * 0.5f;
            else if (elem.align == "right") drawX = r.x + r.width - tw;
            res.DrawTextKR(txt, drawX, r.y, elem.fontSize, elem.color);
        } else if (elem.type == "guide_card") {
            DrawRectangleRounded(r, 0.1f, 8, elem.bgColor);
            DrawRectangleRoundedLinesEx(r, 0.1f, 8, 1.5f, elem.borderColor);
            float lineY = r.y + 16.0f;
            float stepY = elem.textKeys.empty() ? 28.0f
                : std::max(22.0f, (r.height - 24.0f) / static_cast<float>(elem.textKeys.size()));
            for (size_t i = 0; i < elem.textKeys.size(); ++i) {
                Color lineCol = elem.color;
                if (i == 3) lineCol = Color{180, 225, 255, 255};
                else if (i >= 4) lineCol = Color{190, 210, 180, 255};
                res.DrawTextKR(data.GetText(elem.textKeys[i]), r.x + 22.0f, lineY, elem.fontSize, lineCol);
                lineY += stepY;
            }
        } else if (elem.type == "name_input") {
            std::string lbl = resolveElemText(elem);
            float lblW = res.MeasureTextKR(lbl, elem.fontSize);
            Rectangle inputBox{r.x + lblW + 14.0f, r.y + 4.0f, std::max(140.0f, r.width - lblW - 22.0f), r.height - 8.0f};

            DrawRectangleRounded(r, 0.18f, 6, Color{15, 18, 28, 200});
            DrawRectangleRoundedLinesEx(r, 0.18f, 6, 1.2f, Color{120, 110, 88, 180});
            res.DrawTextKR(lbl, r.x + 12.0f, r.y + (r.height - elem.fontSize) * 0.5f, elem.fontSize, GOLD);

            Color boxBorder = m_nameInputFocused ? SKYBLUE : elem.borderColor;
            DrawRectangleRounded(inputBox, 0.18f, 6, elem.bgColor);
            DrawRectangleRoundedLinesEx(inputBox, 0.18f, 6, m_nameInputFocused ? 2.0f : 1.5f, boxBorder);

            if (m_lobbyPlayerName.empty()) {
                res.DrawTextKR(data.GetText("title.name_placeholder"),
                               inputBox.x + 10.0f, inputBox.y + (inputBox.height - elem.fontSize) * 0.5f,
                               elem.fontSize, GRAY);
            } else {
                res.DrawTextKR(m_lobbyPlayerName,
                               inputBox.x + 10.0f, inputBox.y + (inputBox.height - elem.fontSize) * 0.5f,
                               elem.fontSize, elem.color);
            }

            if (m_nameInputFocused && static_cast<int>(m_cursorBlinkTimer * 2.2f) % 2 == 0) {
                float cw = res.MeasureTextKR(m_lobbyPlayerName, elem.fontSize);
                float cx = inputBox.x + 11.0f + cw;
                DrawLineEx(Vector2{cx, inputBox.y + 7.0f}, Vector2{cx, inputBox.y + inputBox.height - 7.0f}, 2.0f, GOLD);
            }

            if (mousePressed && CheckCollisionPointRec(mp, r)) {
                m_nameInputFocused = true;
                clickedInsideNameBox = true;
            }
        } else if (elem.type == "button") {
            std::string label = resolveElemText(elem);
            bool disabled = (elem.action == "continue_game" && !gm.Save().HasSaveFile(1));
            bool hover = !m_settingsOpen && !disabled && CheckCollisionPointRec(mp, r);
            Color bgCol = disabled ? Color{48, 52, 64, 180} : (hover ? elem.hoverColor : elem.bgColor);
            Color bdCol = disabled ? Color{90, 92, 105, 160} : elem.borderColor;
            Color txtCol = disabled ? Color{140, 145, 160, 200} : elem.color;

            DrawRectangleRounded(r, 0.22f, 6, bgCol);
            DrawRectangleRoundedLinesEx(r, 0.22f, 6, 1.5f, bdCol);
            float tw = res.MeasureTextKR(label, elem.fontSize);
            res.DrawTextKR(label, r.x + (r.width - tw) * 0.5f, r.y + (r.height - elem.fontSize) * 0.5f, elem.fontSize, txtCol);
            if (!disabled && hover && mousePressed) {
                if (elem.action == "start_game") {
                    gm.StartNewGame();
                } else if (elem.action == "continue_game") {
                    gm.ContinueSavedGame(1);
                } else if (elem.action == "open_settings") {
                    m_nameInputFocused = false;
                    m_settingsOpen = true;
                    m_waitingRebind = false;
                }
            }
        }
    }

    if (mousePressed && !clickedInsideNameBox) {
        m_nameInputFocused = false;
    }

    RenderSettingsPanel();
    RenderToasts();
}

void UIManager::RenderHUD() const {
    auto& gm = GameManager::Get();
    const auto& data = gm.Data();
    const auto& res = gm.Resources();
    const auto& ent = gm.Entities();
    const auto& player = ent.GetPlayer();
    const auto& eco = gm.Economy();
    const auto& mapMgr = gm.Map();

    // --- Player Status Card ---
    const HudWidgetSpec* statusSpec = data.GetHudWidget("player_status");
    if (!statusSpec || statusSpec->visible) {
        Rectangle card = WidgetRect(statusSpec, Rectangle{14, 14, 285, 134});
        Color bg = statusSpec ? statusSpec->bgColor : Color{14, 16, 24, 215};
        Color border = statusSpec ? statusSpec->borderColor : Color{130, 118, 92, 220};
        Color accent = statusSpec ? statusSpec->accentColor : GOLD;

        DrawRectangleRounded(card, 0.12f, 8, bg);
        DrawRectangleRoundedLinesEx(card, 0.12f, 8, 1.5f, border);

        float innerW = std::max(120.0f, card.width - 24.0f);
        std::string displayName = player.name.empty() ? data.GetPlayerSpec().name : player.name;
        std::string header = "Lv." + std::to_string(player.level) + "  " + displayName;
        res.DrawTextKR(header, card.x + 12, card.y + 8, 18.0f, accent);

        // HP Bar
        float hpRatio = std::clamp(static_cast<float>(player.hp) / static_cast<float>(std::max(1, player.maxHp)), 0.0f, 1.0f);
        DrawRectangleRounded(Rectangle{card.x + 12, card.y + 34, innerW, 18}, 0.25f, 4, Color{45, 20, 22, 255});
        DrawRectangleRounded(Rectangle{card.x + 12, card.y + 34, innerW * hpRatio, 18}, 0.25f, 4, Color{215, 48, 52, 255});
        res.DrawTextKR("HP " + std::to_string(player.hp) + " / " + std::to_string(player.maxHp), card.x + 18, card.y + 35, 14.0f, WHITE);

        // EXP Bar
        int needExp = ent.GetExpToNextLevel();
        float expRatio = std::clamp(static_cast<float>(player.exp) / static_cast<float>(std::max(1, needExp)), 0.0f, 1.0f);
        DrawRectangleRounded(Rectangle{card.x + 12, card.y + 58, innerW, 14}, 0.25f, 4, Color{24, 30, 48, 255});
        DrawRectangleRounded(Rectangle{card.x + 12, card.y + 58, innerW * expRatio, 14}, 0.25f, 4, Color{75, 155, 245, 255});
        res.DrawTextKR("EXP " + std::to_string(player.exp) + " / " + std::to_string(needExp), card.x + 18, card.y + 58, 12.0f, WHITE);

        // Stats & Gold
        std::string statLine = "ATK " + std::to_string(player.atk) + "   DEF " + std::to_string(player.def);
        res.DrawTextKR(statLine, card.x + 12, card.y + 80, 15.0f, RAYWHITE);

        const Texture2D& coinTex = res.GetTexture(604);
        float goldX = card.x + card.width - 110.0f;
        DrawTexturePro(coinTex, Rectangle{0, 0, (float)coinTex.width, (float)coinTex.height},
                       Rectangle{goldX, card.y + 78, 18, 18}, Vector2{0, 0}, 0.0f, WHITE);
        res.DrawTextKR(std::to_string(eco.GetGold()) + " G", goldX + 23.0f, card.y + 79, 16.0f, accent);

        // 8-Direction Live Compass Indicator
        std::string dirLine = data.FormatText("hud.direction_fmt", {data.GetDirectionName(player.dir)});
        res.DrawTextKR(dirLine, card.x + 12, card.y + 106, 14.0f, SKYBLUE);
        Vector2 cc{card.x + card.width - 33.0f, card.y + 114.0f};
        DrawCircleLines(static_cast<int>(cc.x), static_cast<int>(cc.y), 11.0f, Color{120, 160, 210, 180});
        DrawCircle(static_cast<int>(cc.x + player.facingVec.x * 8.0f),
                   static_cast<int>(cc.y + player.facingVec.y * 8.0f), 3.5f, accent);
    }

    // --- Current Map Banner ---
    const HudWidgetSpec* bannerSpec = data.GetHudWidget("map_banner");
    if (!bannerSpec || bannerSpec->visible) {
        if (const MapSpec* ms = mapMgr.GetCurrentMapSpec()) {
            std::string zoneTag = ms->safeZone
                ? data.GetText("hud.zone_safe")
                : (ms->type == "boss" ? data.GetText("hud.zone_boss") : data.GetText("hud.zone_field"));
            std::string mapTitle = ms->name + zoneTag;
            float mw = res.MeasureTextKR(mapTitle, 18.0f);

            Rectangle base = WidgetRect(bannerSpec, Rectangle{510, 14, 260, 32});
            float drawW = std::max(base.width, mw + 36.0f);
            Rectangle banner{base.x + (base.width - drawW) * 0.5f, base.y, drawW, base.height};
            Color bg = bannerSpec ? bannerSpec->bgColor : Color{14, 16, 24, 215};
            Color border = (ms->type == "boss") ? RED : (bannerSpec ? bannerSpec->borderColor : Color{130, 118, 92, 220});

            DrawRectangleRounded(banner, 0.25f, 6, bg);
            DrawRectangleRoundedLinesEx(banner, 0.25f, 6, 1.5f, border);
            res.DrawTextKR(mapTitle, banner.x + (banner.width - mw) * 0.5f, banner.y + (banner.height - 18.0f) * 0.5f,
                           18.0f, ms->type == "boss" ? Color{255, 130, 120, 255} : WHITE);
        }
    }
}

void UIManager::RenderBossBar() const {
    const auto& boss = GameManager::Get().Entities().GetBoss();
    if (!boss.active || !boss.alive) return;
    const auto& data = GameManager::Get().Data();
    const HudWidgetSpec* specWidget = data.GetHudWidget("boss_bar");
    if (specWidget && !specWidget->visible) return;

    const BossSpec* spec = data.GetBossSpec(boss.specId);
    if (!spec) return;

    const auto& res = GameManager::Get().Resources();
    Rectangle box = WidgetRect(specWidget, Rectangle{360, 52, 560, 48});
    Color bg = specWidget ? specWidget->bgColor : Color{18, 10, 12, 225};
    Color border = boss.enraged ? ORANGE : (specWidget ? specWidget->borderColor : RED);
    Color fill = boss.enraged ? Color{240, 95, 25, 255} : (specWidget ? specWidget->accentColor : Color{205, 35, 45, 255});

    DrawRectangleRounded(box, 0.15f, 6, bg);
    DrawRectangleRoundedLinesEx(box, 0.15f, 6, 1.5f, border);

    std::string label = data.FormatText("hud.boss_label_fmt", {spec->name, spec->title}) +
                        (boss.enraged ? data.GetText("hud.boss_enraged_tag") : "");
    res.DrawTextKR(label, box.x + 10.0f, box.y + 4.0f, 16.0f, boss.enraged ? ORANGE : Color{255, 210, 190, 255});

    float barW = std::max(80.0f, box.width - 20.0f);
    float ratio = std::clamp(static_cast<float>(boss.hp) / static_cast<float>(std::max(1, boss.maxHp)), 0.0f, 1.0f);
    DrawRectangleRounded(Rectangle{box.x + 10.0f, box.y + 26.0f, barW, 16.0f}, 0.2f, 4, Color{45, 15, 18, 255});
    DrawRectangleRounded(Rectangle{box.x + 10.0f, box.y + 26.0f, barW * ratio, 16.0f}, 0.2f, 4, fill);

    std::string hpTxt = std::to_string(boss.hp) + " / " + std::to_string(boss.maxHp);
    float tw = res.MeasureTextKR(hpTxt, 13.0f);
    res.DrawTextKR(hpTxt, box.x + 10.0f + (barW - tw) * 0.5f, box.y + 27.0f, 13.0f, WHITE);
}

void UIManager::RenderQuestTracker() const {
    auto& gm = GameManager::Get();
    const auto& res = gm.Resources();
    const auto& quests = gm.Quests();
    const auto& data = gm.Data();

    const HudWidgetSpec* spec = data.GetHudWidget("quest_tracker");
    if (spec && !spec->visible) return;

    Rectangle box = WidgetRect(spec, Rectangle{1000.0f, 14.0f, 266.0f, 135.0f});
    Color bg = spec ? spec->bgColor : Color{14, 16, 24, 205};
    Color border = spec ? spec->borderColor : Color{130, 118, 92, 200};
    Color accent = spec ? spec->accentColor : GOLD;

    DrawRectangleRounded(box, 0.12f, 8, bg);
    DrawRectangleRoundedLinesEx(box, 0.12f, 8, 1.5f, border);

    res.DrawTextKR(data.GetText("hud.quest_tracker_title"), box.x + 12, box.y + 8, 16.0f, accent);

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
                res.DrawTextKR(data.GetText("hud.quest_turnin_hint"), box.x + 14, y, 13.0f, accent);
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
    const auto& data = gm.Data();
    const HudWidgetSpec* spec = data.GetHudWidget("minimap");
    if (spec && !spec->visible) return;

    const auto& mapMgr = gm.Map();
    const auto& ent = gm.Entities();
    int mw = mapMgr.GetWidth();
    int mh = mapMgr.GetHeight();
    if (mw <= 0 || mh <= 0) return;

    Rectangle box = WidgetRect(spec, Rectangle{1100.0f, 158.0f, 166.0f, 126.0f});
    Color bg = spec ? spec->bgColor : Color{10, 12, 18, 210};
    Color border = spec ? spec->borderColor : Color{130, 118, 92, 200};

    float maxCellW = (box.width - 10.0f) / static_cast<float>(mw);
    float maxCellH = (box.height - 10.0f) / static_cast<float>(mh);
    float preferred = spec ? spec->cellSize : 5.0f;
    float cell = std::max(2.0f, std::min({preferred, maxCellW, maxCellH}));
    float w = mw * cell;
    float h = mh * cell;
    float ox = box.x + (box.width - w) * 0.5f;
    float oy = box.y + (box.height - h) * 0.5f;

    DrawRectangleRounded(box, 0.1f, 6, bg);
    DrawRectangleRoundedLinesEx(box, 0.1f, 6, 1.2f, border);

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
                          static_cast<int>(std::ceil(cell)), static_cast<int>(std::ceil(cell)), c);
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

void UIManager::RenderActionBar() {
    auto& gm = GameManager::Get();
    const auto& data = gm.Data();
    const HudWidgetSpec* spec = data.GetHudWidget("action_bar");
    if (spec && !spec->visible) return;

    const auto& res = gm.Resources();
    const auto& input = gm.Input();
    const auto& player = gm.Entities().GetPlayer();
    const auto& pSpec = data.GetPlayerSpec();
    const auto& eco = gm.Economy();

    Rectangle bar = WidgetRect(spec, Rectangle{325.0f, 656.0f, 630.0f, 52.0f});
    Color bg = spec ? spec->bgColor : Color{14, 16, 24, 220};
    Color border = spec ? spec->borderColor : Color{130, 118, 92, 220};
    Color accent = spec ? spec->accentColor : GOLD;

    DrawRectangleRounded(bar, 0.18f, 8, bg);
    DrawRectangleRoundedLinesEx(bar, 0.18f, 8, 1.5f, border);

    struct SlotInfo {
        std::string key;
        std::string name;
        float cd;
        float maxCd;
        int count;
        int clickAction; // 0=none, 1=inv, 2=quest, 3=save, 4=settings
    };

    auto kName = [&](InputAction act) { return InputManager::GetKeyDisplayName(input.GetPrimaryKey(act)); };
    std::vector<SlotInfo> slots = {
        {kName(InputAction::Attack),    data.GetText("action.name_attack"),   player.attackCooldownTimer, pSpec.attackCooldown, -1, 0},
        {kName(InputAction::Skill),     data.GetText("action.name_skill"),    player.skillCooldownTimer,  pSpec.skillCooldown,  -1, 0},
        {kName(InputAction::Dash),      data.GetText("action.name_dash"),     player.dashCooldownTimer,   pSpec.dashCooldown,   -1, 0},
        {kName(InputAction::Potion1),   data.GetText("action.name_potion1"),  0.0f, 1.0f, eco.GetItemCount("potion_hp"), 0},
        {kName(InputAction::Potion2),   data.GetText("action.name_potion2"),  0.0f, 1.0f, eco.GetItemCount("potion_hi"), 0},
        {kName(InputAction::Inventory), data.GetText("action.name_inv"),      0.0f, 1.0f, -1, 1},
        {kName(InputAction::QuestLog),  data.GetText("action.name_quest"),    0.0f, 1.0f, -1, 2},
        {kName(InputAction::QuickSave), data.GetText("action.name_save"),     0.0f, 1.0f, -1, 3},
        {kName(InputAction::Settings),  data.GetText("action.name_settings"), 0.0f, 1.0f, -1, 4}
    };

    float gap = 5.0f;
    float padX = 10.0f;
    float slotW = std::max(48.0f, (bar.width - padX * 2.0f - gap * static_cast<float>(slots.size() - 1)) / static_cast<float>(slots.size()));
    float slotH = std::max(30.0f, bar.height - 12.0f);
    float sx = bar.x + padX;
    Vector2 mp = GetMousePosition();

    for (const auto& s : slots) {
        Rectangle r{sx, bar.y + 6.0f, slotW, slotH};
        bool hover = s.clickAction > 0 && !IsBlockingGameplayInput() && CheckCollisionPointRec(mp, r);
        DrawRectangleRounded(r, 0.2f, 4, hover ? Color{44, 52, 74, 255} : Color{28, 32, 46, 255});
        if (s.cd > 0.0f && s.maxCd > 0.0f) {
            float ratio = std::clamp(s.cd / s.maxCd, 0.0f, 1.0f);
            DrawRectangle(static_cast<int>(r.x), static_cast<int>(r.y + r.height * (1.0f - ratio)),
                          static_cast<int>(r.width), static_cast<int>(r.height * ratio), Color{0, 0, 0, 150});
        }
        DrawRectangleRoundedLinesEx(r, 0.2f, 4, 1.0f, Color{110, 105, 90, 200});

        res.DrawTextKR(s.key, r.x + 4, r.y + 3, 11.0f, accent);
        res.DrawTextKR(s.name, r.x + 4, r.y + slotH * 0.48f, 12.0f, WHITE);

        if (s.count >= 0) {
            std::string cnt = "x" + std::to_string(s.count);
            res.DrawTextKR(cnt, r.x + slotW - 24.0f, r.y + slotH * 0.48f, 12.0f, s.count > 0 ? LIME : GRAY);
        } else if (s.cd > 0.0f) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.1f", s.cd);
            res.DrawTextKR(buf, r.x + slotW - 26.0f, r.y + 4, 11.0f, ORANGE);
        }

        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (s.clickAction == 1) { CloseAllPanels(); m_inventoryOpen = true; }
            else if (s.clickAction == 2) { CloseAllPanels(); m_questLogOpen = true; }
            else if (s.clickAction == 3) { gm.Save().SaveGame(1, false); }
            else if (s.clickAction == 4) { CloseAllPanels(); m_settingsOpen = true; }
        }
        sx += slotW + gap;
    }
}

void UIManager::RenderToasts() const {
    const auto& data = GameManager::Get().Data();
    const HudWidgetSpec* spec = data.GetHudWidget("toast_area");
    if (spec && !spec->visible) return;

    const auto& res = GameManager::Get().Resources();
    Rectangle area = WidgetRect(spec, Rectangle{390.0f, 115.0f, 500.0f, 32.0f});
    Color baseBg = spec ? spec->bgColor : Color{12, 14, 22, 205};
    float centerX = area.x + area.width * 0.5f;
    float y = area.y;

    for (const auto& t : m_toasts) {
        float fade = 1.0f - std::clamp((t.timer - (t.duration - 0.6f)) / 0.6f, 0.0f, 1.0f);
        unsigned char a = static_cast<unsigned char>(255 * fade);
        float tw = res.MeasureTextKR(t.text, 16.0f);
        Rectangle r{centerX - tw * 0.5f - 16.0f, y, tw + 32.0f, std::max(28.0f, area.height)};
        Color bg = baseBg;
        bg.a = static_cast<unsigned char>(baseBg.a * fade);
        DrawRectangleRounded(r, 0.25f, 6, bg);
        Color c = t.color;
        c.a = a;
        res.DrawTextKR(t.text, r.x + 16.0f, r.y + (r.height - 16.0f) * 0.5f, 16.0f, c);
        y += r.height + 6.0f;
    }
}

void UIManager::RenderDialoguePanel() {
    if (!m_dialogueOpen) return;
    auto& gm = GameManager::Get();
    const auto& data = gm.Data();
    const auto& res = gm.Resources();
    const HudWidgetSpec* spec = data.GetHudWidget("dialogue_panel");

    Rectangle box = WidgetRect(spec, Rectangle{290.0f, 480.0f, 700.0f, 165.0f});
    Color bg = spec ? spec->bgColor : Color{14, 16, 26, 240};
    Color border = spec ? spec->borderColor : GOLD;
    Color accent = spec ? spec->accentColor : GOLD;

    DrawRectangleRounded(box, 0.1f, 8, bg);
    DrawRectangleRoundedLinesEx(box, 0.1f, 8, 2.0f, border);

    res.DrawTextKR(m_dialogueNpcName, box.x + 22, box.y + 14, 20.0f, accent);
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

    const HudWidgetSpec* spec = data.GetHudWidget("shop_panel");
    Rectangle panel = WidgetRect(spec, Rectangle{310.0f, 110.0f, 660.0f, 500.0f});
    Color bg = spec ? spec->bgColor : Color{14, 16, 26, 244};
    Color border = spec ? spec->borderColor : GOLD;
    Color accent = spec ? spec->accentColor : GOLD;

    DrawRectangleRounded(panel, 0.08f, 8, bg);
    DrawRectangleRoundedLinesEx(panel, 0.08f, 8, 2.0f, border);

    res.DrawTextKR(data.FormatText("ui.shop_header_fmt", {npc->name, std::to_string(eco.GetGold())}),
                   panel.x + 22, panel.y + 16, 20.0f, accent);
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
        if (y > panel.y + panel.height - 62.0f) break;
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

    const HudWidgetSpec* spec = data.GetHudWidget("inventory_panel");
    Rectangle panel = WidgetRect(spec, Rectangle{340.0f, 130.0f, 600.0f, 460.0f});
    Color bg = spec ? spec->bgColor : Color{14, 16, 26, 244};
    Color border = spec ? spec->borderColor : SKYBLUE;
    Color accent = spec ? spec->accentColor : SKYBLUE;

    DrawRectangleRounded(panel, 0.08f, 8, bg);
    DrawRectangleRoundedLinesEx(panel, 0.08f, 8, 2.0f, border);

    res.DrawTextKR(data.GetText("ui.inv_title"), panel.x + 22, panel.y + 16, 20.0f, accent);

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

    const HudWidgetSpec* spec = data.GetHudWidget("quest_log_panel");
    Rectangle panel = WidgetRect(spec, Rectangle{330.0f, 140.0f, 620.0f, 440.0f});
    Color bg = spec ? spec->bgColor : Color{14, 16, 26, 244};
    Color border = spec ? spec->borderColor : GOLD;
    Color accent = spec ? spec->accentColor : GOLD;

    DrawRectangleRounded(panel, 0.08f, 8, bg);
    DrawRectangleRoundedLinesEx(panel, 0.08f, 8, 2.0f, border);

    res.DrawTextKR(data.GetText("ui.quest_log_title"), panel.x + 22, panel.y + 16, 20.0f, accent);

    float y = panel.y + 56.0f;
    for (const auto& qp : quests.GetAllProgress()) {
        const QuestSpec* qs = data.GetQuestSpec(qp.questId);
        if (!qs) continue;

        Rectangle card{panel.x + 20, y, panel.width - 40, 86};
        DrawRectangleRounded(card, 0.12f, 4, Color{25, 28, 42, 255});

        std::string stKey = "ui.quest_st_not_started";
        Color stColor = GRAY;
        if (qp.status == QuestStatus::Active) { stKey = "ui.quest_st_active"; stColor = SKYBLUE; }
        else if (qp.status == QuestStatus::ReadyToTurnIn) { stKey = "ui.quest_st_ready"; stColor = LIME; }
        else if (qp.status == QuestStatus::Completed) { stKey = "ui.quest_st_completed"; stColor = GOLD; }

        res.DrawTextKR(qs->title + "  " + data.GetText(stKey), card.x + 14, card.y + 6, 16.0f, stColor);
        res.DrawTextKR(qs->description, card.x + 14, card.y + 26, 12.5f, Color{200, 205, 215, 255});

        std::string objLine = data.GetText("ui.quest_progress_prefix");
        for (size_t i = 0; i < qs->objectives.size(); ++i) {
            if (i > 0) objLine += "  |  ";
            objLine += qs->objectives[i].targetName + " " +
                       std::to_string(qp.counts[i]) + "/" + std::to_string(qs->objectives[i].requiredCount);
        }
        res.DrawTextKR(objLine, card.x + 14, card.y + 46, 13.0f, WHITE);
        res.DrawTextKR(data.FormatText("ui.quest_reward_fmt", {
            std::to_string(qs->rewardGold), std::to_string(qs->rewardExp)
        }), card.x + 14, card.y + 64, 12.5f, accent);
        y += 92.0f;
        if (y > panel.y + panel.height - 62.0f) break;
    }

    if (DrawButton(Rectangle{panel.x + panel.width - 130, panel.y + panel.height - 44, 110, 32},
                   data.GetText("ui.btn_close"), Color{65, 65, 78, 255}, Color{95, 95, 112, 255})) {
        m_questLogOpen = false;
    }
}

void UIManager::RenderSettingsPanel() {
    if (!m_settingsOpen) return;
    auto& gm = GameManager::Get();
    const auto& data = gm.Data();
    const auto& res = gm.Resources();
    auto& input = gm.Input();

    int sw = gm.GetScreenWidthPx();
    int sh = gm.GetScreenHeightPx();
    DrawRectangle(0, 0, sw, sh, Color{8, 10, 16, 165});

    const HudWidgetSpec* spec = data.GetHudWidget("settings_panel");
    Rectangle panel = WidgetRect(spec, Rectangle{300.0f, 76.0f, 680.0f, 568.0f});
    Color bg = spec ? spec->bgColor : Color{14, 16, 26, 246};
    Color border = spec ? spec->borderColor : SKYBLUE;
    Color accent = spec ? spec->accentColor : GOLD;

    DrawRectangleRounded(panel, 0.06f, 8, bg);
    DrawRectangleRoundedLinesEx(panel, 0.06f, 8, 2.0f, border);

    res.DrawTextKR(data.GetText("settings.title"), panel.x + 22, panel.y + 16, 21.0f, accent);
    res.DrawTextKR(data.GetText("settings.hint"), panel.x + 22, panel.y + 44, 13.5f, LIGHTGRAY);

    const auto& bindings = input.GetAllBindings();
    const int cols = 2;
    const float colGap = 16.0f;
    const float colW = (panel.width - 44.0f - colGap) / static_cast<float>(cols);
    const float rowH = 54.0f;
    const float startY = panel.y + 74.0f;

    for (size_t i = 0; i < bindings.size(); ++i) {
        const auto& b = bindings[i];
        int col = static_cast<int>(i % cols);
        int row = static_cast<int>(i / cols);
        float rx = panel.x + 22.0f + col * (colW + colGap);
        float ry = startY + row * (rowH + 6.0f);

        Rectangle card{rx, ry, colW, rowH};
        DrawRectangleRounded(card, 0.16f, 4, Color{24, 28, 42, 255});
        res.DrawTextKR(data.GetText(b.labelKey), card.x + 12, card.y + 17, 15.0f, RAYWHITE);

        bool listening = (m_waitingRebind && m_rebindTargetAction == b.action);
        std::string keyText = listening
            ? data.GetText("settings.waiting_key")
            : InputManager::GetKeyDisplayName(b.primaryKey);
        Color btnBg = listening ? Color{175, 118, 28, 255} : Color{48, 62, 92, 255};
        Color btnHv = listening ? Color{205, 142, 40, 255} : Color{68, 88, 128, 255};

        Rectangle keyBtn{card.x + card.width - 122.0f, card.y + 9.0f, 112.0f, 36.0f};
        if (DrawButton(keyBtn, keyText, btnBg, btnHv)) {
            m_waitingRebind = true;
            m_rebindTargetAction = b.action;
        }
    }

    if (DrawButton(Rectangle{panel.x + 22, panel.y + panel.height - 48, 150, 34},
                   data.GetText("settings.reset_btn"), Color{118, 56, 46, 255}, Color{155, 74, 60, 255})) {
        m_waitingRebind = false;
        input.ResetToDefaults(true);
        PushToast(data.GetText("toast.keybind_reset"), GOLD, 2.2f);
    }

    if (DrawButton(Rectangle{panel.x + panel.width - 134, panel.y + panel.height - 48, 112, 34},
                   data.GetText("ui.btn_close"), Color{65, 65, 78, 255}, Color{95, 95, 112, 255})) {
        m_waitingRebind = false;
        m_settingsOpen = false;
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
    RenderSettingsPanel();

    if (screen == GameScreen::GameOver) RenderGameOverOverlay();
    else if (screen == GameScreen::Victory) RenderVictoryOverlay();
}
