#include "managers/ResourceManager.h"
#include "managers/DataManager.h"
#include "core/GameManager.h"

#include <algorithm>
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
    std::set<int> cpSet;
    // Basic ASCII printable characters
    for (int c = 32; c <= 126; ++c) cpSet.insert(c);

    // Automatically collect EVERY glyph used across all loaded data/*.json files
    // (including data/strings.json, items, monsters, bosses, npcs, quests, maps, tiles, player)
    CollectUtf8Codepoints(data.GetAllLoadedJsonText(), cpSet);
    CollectUtf8Codepoints(data.GetFontExtraGlyphs(), cpSet);

    std::vector<int> codepoints(cpSet.begin(), cpSet.end());
    int fontSize = data.GetFontSize();

    for (const auto& candidate : data.GetFontCandidates()) {
        if (fs::exists(candidate)) {
            m_font = LoadFontEx(candidate.c_str(), fontSize, codepoints.data(), static_cast<int>(codepoints.size()));
            if (m_font.texture.id != 0) {
                SetTextureFilter(m_font.texture, TEXTURE_FILTER_BILINEAR);
                m_customFontLoaded = true;
                std::cout << "[ResourceManager] Loaded font from data/strings.json config: " << candidate
                          << " (" << codepoints.size() << " glyphs)\n";
                return;
            }
        }
    }

    m_font = GetFontDefault();
    m_customFontLoaded = false;
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
    DrawTextEx(m_font, text.c_str(), Vector2{x, y}, fontSize, 1.0f, color);
}

float ResourceManager::MeasureTextKR(const std::string& text, float fontSize) const {
    return MeasureTextEx(m_font, text.c_str(), fontSize, 1.0f).x;
}
