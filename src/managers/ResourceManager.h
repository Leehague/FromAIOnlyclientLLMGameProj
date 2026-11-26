#pragma once

#include "core/IManager.h"
#include <raylib.h>
#include <set>
#include <string>
#include <unordered_map>

// [Requirement 11] Reads ONLY data/art_mapping.txt (Art ID <-> relative path)
// and manages Texture2D loading/caching/unloading + Korean TTF font rendering.
class ResourceManager : public IManager {
public:
    const char* GetName() const override { return "ResourceManager"; }
    bool Init() override;
    void Update(float dt) override {}
    void Render() override {}
    void Shutdown() override;

    const Texture2D& GetTexture(int artId) const;
    bool HasTexture(int artId) const;

    const Font& GetFont() const { return m_font; }
    void DrawTextKR(const std::string& text, float x, float y, float fontSize, Color color) const;
    float MeasureTextKR(const std::string& text, float fontSize) const;
    // Dynamically adds any newly typed Unicode/Hangul characters (e.g. custom player name) to the UI font atlas
    void EnsureGlyphsForText(const std::string& utf8Text);

    size_t GetLoadedTextureCount() const { return m_textures.size(); }
    int GetMissingTextureCount() const { return m_missingCount; }

private:
    bool LoadArtMapping(const std::string& mappingRelPath);
    void LoadUIFont();
    void CreateFallbackTexture();

    std::unordered_map<int, std::string> m_artPaths;
    std::unordered_map<int, Texture2D> m_textures;
    Texture2D m_fallbackTex{};
    bool m_hasFallback = false;
    int m_missingCount = 0;

    Font m_font{};
    bool m_customFontLoaded = false;
    std::string m_loadedFontPath;
    int m_loadedFontSize = 28;
    std::set<int> m_loadedCodepoints;
};
