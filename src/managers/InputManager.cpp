#include "managers/InputManager.h"
#include "managers/DataManager.h"
#include "core/GameManager.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using json = nlohmann::json;

bool InputManager::ParseActionId(const std::string& id, InputAction& outAction) {
    if (id == "move_up")    { outAction = InputAction::MoveUp;    return true; }
    if (id == "move_down")  { outAction = InputAction::MoveDown;  return true; }
    if (id == "move_left")  { outAction = InputAction::MoveLeft;  return true; }
    if (id == "move_right") { outAction = InputAction::MoveRight; return true; }
    if (id == "attack")     { outAction = InputAction::Attack;    return true; }
    if (id == "skill")      { outAction = InputAction::Skill;     return true; }
    if (id == "dash")       { outAction = InputAction::Dash;      return true; }
    if (id == "interact")   { outAction = InputAction::Interact;  return true; }
    if (id == "potion_1")   { outAction = InputAction::Potion1;   return true; }
    if (id == "potion_2")   { outAction = InputAction::Potion2;   return true; }
    if (id == "inventory")  { outAction = InputAction::Inventory; return true; }
    if (id == "quest_log")  { outAction = InputAction::QuestLog;  return true; }
    if (id == "quick_save") { outAction = InputAction::QuickSave; return true; }
    if (id == "settings")   { outAction = InputAction::Settings;  return true; }
    return false;
}

fs::path InputManager::GetUserBindingsPath() const {
    return fs::path(GameManager::Get().GetBasePath()) / "saves" / "keybindings.json";
}

bool InputManager::LoadDefaultBindings() {
    m_defaultBindings.clear();
    m_defaultBindings.resize(static_cast<size_t>(InputAction::Count));

    std::string fullPath = GameManager::Get().ResolvePath("data/keybindings.json");
    std::ifstream in(fullPath, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "[InputManager] Cannot open default keybindings: " << fullPath << "\n";
        return false;
    }

    try {
        json root;
        in >> root;
        for (const auto& b : root.at("bindings")) {
            std::string actId = b.at("action").get<std::string>();
            InputAction act{};
            if (!ParseActionId(actId, act)) continue;
            KeyBindingEntry e;
            e.action = act;
            e.actionId = actId;
            e.labelKey = b.value("label_key", actId);
            e.primaryKey = b.value("primary", 0);
            e.secondaryKey = b.value("secondary", 0);
            m_defaultBindings[static_cast<size_t>(act)] = std::move(e);
        }
        m_bindings = m_defaultBindings;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[InputManager] Error parsing data/keybindings.json: " << e.what() << "\n";
        return false;
    }
}

bool InputManager::Init() {
    if (!LoadDefaultBindings()) return false;
    LoadUserBindings();
    return true;
}

bool InputManager::IsActionDown(InputAction action) const {
    size_t idx = static_cast<size_t>(action);
    if (idx >= m_bindings.size()) return false;
    const auto& b = m_bindings[idx];
    if (b.primaryKey > 0 && IsKeyDown(b.primaryKey)) return true;
    if (b.secondaryKey > 0 && IsKeyDown(b.secondaryKey)) return true;
    return false;
}

bool InputManager::IsActionPressed(InputAction action) const {
    size_t idx = static_cast<size_t>(action);
    if (idx >= m_bindings.size()) return false;
    const auto& b = m_bindings[idx];
    if (b.primaryKey > 0 && IsKeyPressed(b.primaryKey)) return true;
    if (b.secondaryKey > 0 && IsKeyPressed(b.secondaryKey)) return true;
    return false;
}

int InputManager::GetPrimaryKey(InputAction action) const {
    size_t idx = static_cast<size_t>(action);
    return idx < m_bindings.size() ? m_bindings[idx].primaryKey : 0;
}

int InputManager::GetSecondaryKey(InputAction action) const {
    size_t idx = static_cast<size_t>(action);
    return idx < m_bindings.size() ? m_bindings[idx].secondaryKey : 0;
}

const std::string& InputManager::GetActionLabelKey(InputAction action) const {
    static const std::string kEmpty;
    size_t idx = static_cast<size_t>(action);
    return idx < m_bindings.size() ? m_bindings[idx].labelKey : kEmpty;
}

bool InputManager::IsBindableKey(int k) {
    if (k <= 0 || k == KEY_ESCAPE || k == KEY_PRINT_SCREEN || k == KEY_CAPS_LOCK) return false;
    return true;
}

bool InputManager::SetPrimaryKey(InputAction action, int raylibKey, bool persist) {
    if (!IsBindableKey(raylibKey)) return false;
    size_t idx = static_cast<size_t>(action);
    if (idx >= m_bindings.size()) return false;

    int oldKey = m_bindings[idx].primaryKey;
    if (oldKey == raylibKey) return true;

    // Swap automatically if another action already uses this primary key
    for (size_t i = 0; i < m_bindings.size(); ++i) {
        if (i != idx && m_bindings[i].primaryKey == raylibKey) {
            m_bindings[i].primaryKey = oldKey;
            break;
        }
    }
    m_bindings[idx].primaryKey = raylibKey;

    if (persist) {
        SaveUserBindings();
    }
    return true;
}

bool InputManager::ResetToDefaults(bool persist) {
    m_bindings = m_defaultBindings;
    if (persist) {
        SaveUserBindings();
    }
    return true;
}

bool InputManager::SaveUserBindings(const std::string& overridePath) const {
    fs::path path = overridePath.empty() ? GetUserBindingsPath() : fs::path(overridePath);
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);

    json root;
    json arr = json::array();
    for (const auto& b : m_bindings) {
        arr.push_back({
            {"action", b.actionId},
            {"primary", b.primaryKey},
            {"secondary", b.secondaryKey}
        });
    }
    root["bindings"] = std::move(arr);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;
    out << root.dump(2) << "\n";
    return out.good();
}

bool InputManager::LoadUserBindings(const std::string& overridePath) {
    fs::path path = overridePath.empty() ? GetUserBindingsPath() : fs::path(overridePath);
    std::error_code ec;
    if (!fs::exists(path, ec)) return false;

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;
    try {
        json root;
        in >> root;
        if (!root.contains("bindings") || !root["bindings"].is_array()) return false;
        for (const auto& b : root["bindings"]) {
            std::string actId = b.value("action", "");
            InputAction act{};
            if (!ParseActionId(actId, act)) continue;
            int pk = b.value("primary", 0);
            if (IsBindableKey(pk)) {
                m_bindings[static_cast<size_t>(act)].primaryKey = pk;
            }
            if (b.contains("secondary")) {
                m_bindings[static_cast<size_t>(act)].secondaryKey = b.value("secondary", 0);
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

std::string InputManager::GetKeyDisplayName(int k) {
    if (k >= KEY_A && k <= KEY_Z) return std::string(1, static_cast<char>('A' + (k - KEY_A)));
    if (k >= KEY_ZERO && k <= KEY_NINE) return std::string(1, static_cast<char>('0' + (k - KEY_ZERO)));
    if (k >= KEY_F1 && k <= KEY_F12) return "F" + std::to_string(1 + (k - KEY_F1));
    switch (k) {
        case KEY_SPACE:         return "Space";
        case KEY_LEFT_SHIFT:    return "LShift";
        case KEY_RIGHT_SHIFT:   return "RShift";
        case KEY_LEFT_CONTROL:  return "LCtrl";
        case KEY_RIGHT_CONTROL: return "RCtrl";
        case KEY_LEFT_ALT:      return "LAlt";
        case KEY_RIGHT_ALT:     return "RAlt";
        case KEY_TAB:           return "Tab";
        case KEY_ENTER:         return "Enter";
        case KEY_BACKSPACE:     return "Bksp";
        case KEY_UP:            return "Up";
        case KEY_DOWN:          return "Down";
        case KEY_LEFT:          return "Left";
        case KEY_RIGHT:         return "Right";
        case KEY_MINUS:         return "-";
        case KEY_EQUAL:         return "=";
        case KEY_LEFT_BRACKET:  return "[";
        case KEY_RIGHT_BRACKET: return "]";
        case KEY_SEMICOLON:     return ";";
        case KEY_APOSTROPHE:    return "'";
        case KEY_COMMA:         return ",";
        case KEY_PERIOD:        return ".";
        case KEY_SLASH:         return "/";
        case KEY_GRAVE:         return "`";
        default:                return k > 0 ? ("#" + std::to_string(k)) : "-";
    }
}
