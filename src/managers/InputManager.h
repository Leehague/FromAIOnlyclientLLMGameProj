#pragma once

#include "core/IManager.h"
#include "core/Types.h"
#include <array>
#include <filesystem>
#include <string>
#include <vector>

// Customizable keyboard input mapping manager.
// Loads default bindings from data/keybindings.json and persists user overrides in saves/keybindings.json.
class InputManager : public IManager {
public:
    const char* GetName() const override { return "InputManager"; }
    bool Init() override;
    void Update(float dt) override {}
    void Render() override {}
    void Shutdown() override {}

    bool IsActionDown(InputAction action) const;
    bool IsActionPressed(InputAction action) const;

    int GetPrimaryKey(InputAction action) const;
    int GetSecondaryKey(InputAction action) const;
    const std::string& GetActionLabelKey(InputAction action) const;
    const std::vector<KeyBindingEntry>& GetAllBindings() const { return m_bindings; }

    // Rebinds an action's primary key (swaps automatically on conflict) and saves to disk
    bool SetPrimaryKey(InputAction action, int raylibKey, bool persist = true);
    bool ResetToDefaults(bool persist = true);

    bool SaveUserBindings(const std::string& overridePath = "") const;
    bool LoadUserBindings(const std::string& overridePath = "");

    static std::string GetKeyDisplayName(int raylibKey);
    static bool IsBindableKey(int raylibKey);

private:
    bool LoadDefaultBindings();
    std::filesystem::path GetUserBindingsPath() const;
    static bool ParseActionId(const std::string& id, InputAction& outAction);

    std::vector<KeyBindingEntry> m_defaultBindings;
    std::vector<KeyBindingEntry> m_bindings;
};
