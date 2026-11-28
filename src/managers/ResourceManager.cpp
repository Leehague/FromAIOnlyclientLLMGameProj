#include "managers/ResourceManager.h"
#include "managers/DataManager.h"
#include "core/GameManager.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

static void CollectUtf8Codepoints(const std::string& utf8, std::set<int>& outSet) {
    const char* ptr = utf8.c_str();
    const char* end = ptr + utf8.size();
    while (ptr < end) {
        int bytes = 0;
        int cp = GetCodepointNext(ptr, &bytes);
        if (bytes <= 0) break;
        if (cp >= 32) {
            outSet.insert(cp);
        }
        ptr += bytes;
    }
}

void ResourceManager::CreateFallbackTexture() {
    Image img = GenImageChecked(32, 32, 8, 8, MAGENTA, BLACK);
    m_fallbackTex = LoadTextureFromImage(img);
    SetTextureFilter(m_fallbackTex, TEXTURE_FILTER_POINT);
    UnloadImage(img);
    m_hasFallback = true;
}

bool ResourceManager::LoadArtMapping(const std::string& mappingRelPath) {
    std::string fullPath = GameManager::Get().ResolvePath(mappingRelPath);
    std::ifstream file(fullPath);
    if (!file.is_open()) {
        std::cerr << "[ResourceManager] Cannot open art mapping file: " << fullPath << "\n";
        return false;
    }

    m_missingCount = 0;
    std::string line;
    while (std::getline(file, line)) {
        // Trim leading whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos || line[start] == '#') continue;

        std::istringstream iss(line.substr(start));
        int artId = 0;
        std::string relPath;
        if (!(iss >> artId >> relPath)) continue;

        m_artPaths[artId] = relPath;
        std::string texPath = GameManager::Get().ResolvePath(relPath);
        if (fs::exists(texPath)) {
            Texture2D tex = LoadTexture(texPath.c_str());
            if (tex.id != 0) {
                SetTextureFilter(tex, TEXTURE_FILTER_POINT);
                m_textures[artId] = tex;
            } else {
                ++m_missingCount;
                std::cerr << "[ResourceManager] Failed to load texture ID " << artId << ": " << texPath << "\n";
            }
        } else {
            ++m_missingCount;
            std::cerr << "[ResourceManager] Missing texture file for ID " << artId << ": " << texPath << "\n";
        }
    }

    std::cout << "[ResourceManager] Loaded " << m_textures.size() << " textures from "
              << mappingRelPath << " (missing: " << m_missingCount << ")\n";
    return !m_artPaths.empty();
}

void ResourceManager::LoadUIFont() {
    const auto& data = GameManager::Get().Data();
    m_loadedCodepoints.clear();
    // Basic ASCII printable characters
    for (int c = 32; c <= 126; ++c) m_loadedCodepoints.insert(c);

    // Automatically collect EVERY glyph used across all loaded data/*.json files
    // (including data/strings.json, items, monsters, bosses, npcs, quests, maps, tiles, player, ui_layout)
    CollectUtf8Codepoints(data.GetAllLoadedJsonText(), m_loadedCodepoints);
    CollectUtf8Codepoints(data.GetFontExtraGlyphs(), m_loadedCodepoints);

    std::vector<int> codepoints(m_loadedCodepoints.begin(), m_loadedCodepoints.end());
    m_loadedFontSize = data.GetFontSize();

    for (const auto& candidate : data.GetFontCandidates()) {
        if (fs::exists(candidate)) {
            m_font = LoadFontEx(candidate.c_str(), m_loadedFontSize, codepoints.data(), static_cast<int>(codepoints.size()));
            if (m_font.texture.id != 0) {
                GenTextureMipmaps(&m_font.texture);
                SetTextureFilter(m_font.texture, TEXTURE_FILTER_TRILINEAR);
                m_customFontLoaded = true;
                m_loadedFontPath = candidate;
                std::cout << "[ResourceManager] Loaded font from data/strings.json config: " << candidate
                          << " (" << codepoints.size() << " glyphs, size " << m_loadedFontSize << "px)\n";
                return;
            }
        }
    }

    m_font = GetFontDefault();
    m_customFontLoaded = false;
}

void ResourceManager::EnsureGlyphsForText(const std::string& utf8Text) {
    if (!m_customFontLoaded || m_loadedFontPath.empty() || utf8Text.empty()) return;
    size_t before = m_loadedCodepoints.size();
    CollectUtf8Codepoints(utf8Text, m_loadedCodepoints);
    if (m_loadedCodepoints.size() == before) return;

    std::vector<int> codepoints(m_loadedCodepoints.begin(), m_loadedCodepoints.end());
    Font newFont = LoadFontEx(m_loadedFontPath.c_str(), m_loadedFontSize, codepoints.data(), static_cast<int>(codepoints.size()));
    if (newFont.texture.id != 0) {
        UnloadFont(m_font);
        m_font = newFont;
        GenTextureMipmaps(&m_font.texture);
        SetTextureFilter(m_font.texture, TEXTURE_FILTER_TRILINEAR);
    }
}

bool ResourceManager::Init() {
    CreateFallbackTexture();
    LoadUIFont();
    return LoadArtMapping("data/art_mapping.txt");
}

void ResourceManager::Shutdown() {
    for (auto& kv : m_textures) {
        UnloadTexture(kv.second);
    }
    m_textures.clear();
    m_artPaths.clear();

    if (m_hasFallback) {
        UnloadTexture(m_fallbackTex);
        m_hasFallback = false;
    }
    if (m_customFontLoaded) {
        UnloadFont(m_font);
        m_customFontLoaded = false;
    }
}

const Texture2D& ResourceManager::GetTexture(int artId) const {
    auto it = m_textures.find(artId);
    if (it != m_textures.end()) {
        return it->second;
    }
    return m_fallbackTex;
}

bool ResourceManager::HasTexture(int artId) const {
    return m_textures.find(artId) != m_textures.end();
}

void ResourceManager::DrawTextKR(const std::string& text, float x, float y, float fontSize, Color color) const {
    float sx = std::round(x);
    float sy = std::round(y);
    int luminance = static_cast<int>(color.r) + static_cast<int>(color.g) + static_cast<int>(color.b);
    if (luminance > 70 && color.a > 20) {
        unsigned char shadowA = static_cast<unsigned char>(std::min(210, static_cast<int>(color.a) * 4 / 5));
        DrawTextEx(m_font, text.c_str(), Vector2{sx + 1.0f, sy + 1.0f}, fontSize, 1.0f, Color{6, 8, 14, shadowA});
    }
    DrawTextEx(m_font, text.c_str(), Vector2{sx, sy}, fontSize, 1.0f, color);
}

float ResourceManager::MeasureTextKR(const std::string& text, float fontSize) const {
    return MeasureTextEx(m_font, text.c_str(), fontSize, 1.0f).x;
}
