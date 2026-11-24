#include "managers/MapManager.h"
#include "managers/DataManager.h"
#include "managers/ResourceManager.h"
#include "core/GameManager.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <sstream>

bool MapManager::Init() {
    m_tileSize = GameManager::Get().Data().GetTileSize();
    return true;
}

void MapManager::Update(float dt) {
    m_animTime += dt;
}

void MapManager::Shutdown() {
    m_grid.clear();
    m_currentMapSpec = nullptr;
    m_currentMapId.clear();
}

bool MapManager::LoadMap(const std::string& mapId) {
    const MapSpec* spec = GameManager::Get().Data().GetMapSpec(mapId);
    if (!spec) {
        std::cerr << "[MapManager] Unknown map ID: " << mapId << "\n";
        return false;
    }

    std::string fullPath = GameManager::Get().ResolvePath(spec->txtPath);
    std::ifstream file(fullPath);
    if (!file.is_open()) {
        std::cerr << "[MapManager] Cannot open map txt file: " << fullPath << "\n";
        return false;
    }

    std::vector<std::vector<int>> parsedGrid;
    std::string line;
    while (std::getline(file, line)) {
        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos || line[first] == '#') continue;

        std::istringstream iss(line.substr(first));
        std::vector<int> row;
        int tileId = 0;
        while (iss >> tileId) {
            row.push_back(tileId);
        }
        if (!row.empty()) {
            parsedGrid.push_back(std::move(row));
        }
    }

    if (parsedGrid.empty()) {
        std::cerr << "[MapManager] Empty tilemap in: " << fullPath << "\n";
        return false;
    }

    m_grid = std::move(parsedGrid);
    m_height = static_cast<int>(m_grid.size());
    m_width = static_cast<int>(m_grid[0].size());
    m_currentMapId = mapId;
    m_currentMapSpec = spec;
    return true;
}

int MapManager::GetTileAt(int tx, int ty) const {
    if (ty < 0 || ty >= m_height || tx < 0 || tx >= m_width) return -1;
    if (tx >= static_cast<int>(m_grid[ty].size())) return -1;
    return m_grid[ty][tx];
}

bool MapManager::IsTileWalkable(int tx, int ty) const {
    int tid = GetTileAt(tx, ty);
    if (tid < 0) return false;
    const TileSpec* ts = GameManager::Get().Data().GetTileSpec(tid);
    return ts ? ts->walkable : false;
}

bool MapManager::IsCircleWalkable(Vector2 worldPos, float radius) const {
    const float samples[4][2] = {
        { worldPos.x - radius, worldPos.y - radius },
        { worldPos.x + radius, worldPos.y - radius },
        { worldPos.x - radius, worldPos.y + radius },
        { worldPos.x + radius, worldPos.y + radius }
    };
    for (int i = 0; i < 4; ++i) {
        int tx = static_cast<int>(std::floor(samples[i][0] / static_cast<float>(m_tileSize)));
        int ty = static_cast<int>(std::floor(samples[i][1] / static_cast<float>(m_tileSize)));
        if (!IsTileWalkable(tx, ty)) return false;
    }
    return true;
}

Vector2 MapManager::TileToWorldCenter(int tx, int ty) const {
    return Vector2{
        (tx + 0.5f) * static_cast<float>(m_tileSize),
        (ty + 0.5f) * static_cast<float>(m_tileSize)
    };
}

void MapManager::WorldToTile(Vector2 worldPos, int& outTx, int& outTy) const {
    outTx = static_cast<int>(std::floor(worldPos.x / static_cast<float>(m_tileSize)));
    outTy = static_cast<int>(std::floor(worldPos.y / static_cast<float>(m_tileSize)));
}

std::vector<Vector2> MapManager::FindPath(Vector2 startWorld, Vector2 goalWorld, float entityRadius) const {
    std::vector<Vector2> result;
    if (m_width <= 0 || m_height <= 0) return result;

    const bool useRadiusClearance = (entityRadius >= static_cast<float>(m_tileSize) * 0.5f);
    auto isPassable = [&](int tx, int ty, bool strictClearance) -> bool {
        if (!IsTileWalkable(tx, ty)) return false;
        if (!strictClearance) return true;
        return IsCircleWalkable(TileToWorldCenter(tx, ty), entityRadius);
    };

    auto findNearestPassable = [&](int& tx, int& ty, bool strictClearance) -> bool {
        tx = std::clamp(tx, 0, m_width - 1);
        ty = std::clamp(ty, 0, m_height - 1);
        if (isPassable(tx, ty, strictClearance)) return true;

        for (int r = 1; r <= 3; ++r) {
            int bestX = -1, bestY = -1;
            int bestD2 = std::numeric_limits<int>::max();
            for (int dy = -r; dy <= r; ++dy) {
                for (int dx = -r; dx <= r; ++dx) {
                    int nx = tx + dx;
                    int ny = ty + dy;
                    if (nx < 0 || nx >= m_width || ny < 0 || ny >= m_height) continue;
                    if (!isPassable(nx, ny, strictClearance)) continue;
                    int d2 = dx * dx + dy * dy;
                    if (d2 < bestD2) {
                        bestD2 = d2;
                        bestX = nx;
                        bestY = ny;
                    }
                }
            }
            if (bestX >= 0) {
                tx = bestX;
                ty = bestY;
                return true;
            }
        }
        return false;
    };

    auto runAStar = [&](bool strictClearance) -> std::vector<Vector2> {
        std::vector<Vector2> path;
        int sx = 0, sy = 0, gx = 0, gy = 0;
        WorldToTile(startWorld, sx, sy);
        WorldToTile(goalWorld, gx, gy);

        if (!findNearestPassable(sx, sy, strictClearance)) return path;
        if (!findNearestPassable(gx, gy, strictClearance)) return path;

        if (sx == gx && sy == gy) {
            path.push_back(goalWorld);
            return path;
        }

        const int totalNodes = m_width * m_height;
        const int startIdx = sy * m_width + sx;
        const int goalIdx = gy * m_width + gx;

        std::vector<int> gScore(totalNodes, std::numeric_limits<int>::max());
        std::vector<int> cameFrom(totalNodes, -1);
        std::vector<uint8_t> closed(totalNodes, 0);

        struct OpenNode {
            int f;
            int g;
            int idx;
            bool operator>(const OpenNode& o) const {
                return (f != o.f) ? (f > o.f) : (g > o.g);
            }
        };

        auto octileHeuristic = [&](int x, int y) -> int {
            int dx = std::abs(x - gx);
            int dy = std::abs(y - gy);
            return 10 * (dx + dy) + (14 - 20) * std::min(dx, dy);
        };

        std::priority_queue<OpenNode, std::vector<OpenNode>, std::greater<OpenNode>> open;
        gScore[startIdx] = 0;
        open.push(OpenNode{octileHeuristic(sx, sy), 0, startIdx});

        static constexpr int kDirs[8][3] = {
            { 0, -1, 10}, { 1,  0, 10}, { 0,  1, 10}, {-1,  0, 10},
            { 1, -1, 14}, { 1,  1, 14}, {-1,  1, 14}, {-1, -1, 14}
        };

        bool found = false;
        while (!open.empty()) {
            OpenNode cur = open.top();
            open.pop();
            if (closed[cur.idx]) continue;
            closed[cur.idx] = 1;

            if (cur.idx == goalIdx) {
                found = true;
                break;
            }

            int cx = cur.idx % m_width;
            int cy = cur.idx / m_width;

            for (int d = 0; d < 8; ++d) {
                int dx = kDirs[d][0];
                int dy = kDirs[d][1];
                int stepCost = kDirs[d][2];
                int nx = cx + dx;
                int ny = cy + dy;

                if (nx < 0 || nx >= m_width || ny < 0 || ny >= m_height) continue;
                int nIdx = ny * m_width + nx;
                if (closed[nIdx]) continue;
                if (!isPassable(nx, ny, strictClearance)) continue;

                // Prevent diagonal corner cutting across solid tile edges
                if (dx != 0 && dy != 0) {
                    if (!isPassable(cx + dx, cy, strictClearance) ||
                        !isPassable(cx, cy + dy, strictClearance)) {
                        continue;
                    }
                }

                int tentativeG = cur.g + stepCost;
                if (tentativeG < gScore[nIdx]) {
                    gScore[nIdx] = tentativeG;
                    cameFrom[nIdx] = cur.idx;
                    int f = tentativeG + octileHeuristic(nx, ny);
                    open.push(OpenNode{f, tentativeG, nIdx});
                }
            }
        }

        if (!found) return path;

        // Reconstruct tile centers (excluding starting tile)
        std::vector<int> revIndices;
        int trace = goalIdx;
        while (trace != -1 && trace != startIdx) {
            revIndices.push_back(trace);
            trace = cameFrom[trace];
        }
        std::reverse(revIndices.begin(), revIndices.end());

        path.reserve(revIndices.size());
        for (int idx : revIndices) {
            path.push_back(TileToWorldCenter(idx % m_width, idx / m_width));
        }
        if (!path.empty() && IsCircleWalkable(goalWorld, entityRadius)) {
            path.back() = goalWorld;
        }
        return path;
    };

    result = runAStar(useRadiusClearance);
    if (result.empty() && useRadiusClearance) {
        // Fallback to standard single-tile walkability if tight space prevents wide clearance
        result = runAStar(false);
    }
    return result;
}

const WarpSpec* MapManager::FindWarpAtWorldPos(Vector2 worldPos) const {
    if (!m_currentMapSpec) return nullptr;
    int tx = static_cast<int>(std::floor(worldPos.x / static_cast<float>(m_tileSize)));
    int ty = static_cast<int>(std::floor(worldPos.y / static_cast<float>(m_tileSize)));
    for (const auto& w : m_currentMapSpec->warps) {
        if (w.tileX == tx && w.tileY == ty) {
            return &w;
        }
    }
    return nullptr;
}

bool MapManager::IsNearSpecialTile(Vector2 worldPos, const std::string& specialType, float rangeTiles) const {
    int cx = static_cast<int>(std::floor(worldPos.x / static_cast<float>(m_tileSize)));
    int cy = static_cast<int>(std::floor(worldPos.y / static_cast<float>(m_tileSize)));
    float maxDistPx = rangeTiles * static_cast<float>(m_tileSize);
    int r = static_cast<int>(std::ceil(rangeTiles)) + 1;

    for (int ty = cy - r; ty <= cy + r; ++ty) {
        for (int tx = cx - r; tx <= cx + r; ++tx) {
            int tid = GetTileAt(tx, ty);
            if (tid < 0) continue;
            const TileSpec* ts = GameManager::Get().Data().GetTileSpec(tid);
            if (ts && ts->special == specialType) {
                Vector2 c = TileToWorldCenter(tx, ty);
                float dx = c.x - worldPos.x;
                float dy = c.y - worldPos.y;
                if (dx * dx + dy * dy <= maxDistPx * maxDistPx) {
                    return true;
                }
            }
        }
    }
    return false;
}

void MapManager::Render() {
    if (!m_currentMapSpec) return;

    auto& data = GameManager::Get().Data();
    auto& res = GameManager::Get().Resources();
    Color tint = m_currentMapSpec->ambientTint;

    for (int ty = 0; ty < m_height; ++ty) {
        for (int tx = 0; tx < m_width; ++tx) {
            int tid = m_grid[ty][tx];
            const TileSpec* ts = data.GetTileSpec(tid);
            if (!ts) continue;

            const Texture2D& tex = res.GetTexture(ts->artId);
            float px = static_cast<float>(tx * m_tileSize);
            float py = static_cast<float>(ty * m_tileSize);

            Rectangle src{0.0f, 0.0f, static_cast<float>(tex.width), static_cast<float>(tex.height)};
            Rectangle dst{px, py, static_cast<float>(m_tileSize), static_cast<float>(m_tileSize)};
            DrawTexturePro(tex, src, dst, Vector2{0, 0}, 0.0f, tint);

            // Subtle dynamic glow on special animated tiles
            if (ts->special == "portal") {
                float pulse = 0.5f + 0.5f * std::sin(m_animTime * 4.5f + tx + ty);
                Color glow = (tid == 15)
                    ? Color{255, 70, 60, static_cast<unsigned char>(50 + 55 * pulse)}
                    : Color{70, 190, 255, static_cast<unsigned char>(50 + 55 * pulse)};
                DrawCircleLines(static_cast<int>(px + m_tileSize * 0.5f),
                                static_cast<int>(py + m_tileSize * 0.5f),
                                9.0f + pulse * 4.0f, glow);
            } else if (ts->special == "lava") {
                float pulse = 0.5f + 0.5f * std::sin(m_animTime * 3.0f + tx * 0.7f + ty * 1.1f);
                DrawRectangle(static_cast<int>(px), static_cast<int>(py), m_tileSize, m_tileSize,
                              Color{255, 120, 20, static_cast<unsigned char>(25 * pulse)});
            } else if (ts->special == "heal") {
                float pulse = 0.5f + 0.5f * std::sin(m_animTime * 3.5f);
                DrawCircleLines(static_cast<int>(px + m_tileSize * 0.5f),
                                static_cast<int>(py + m_tileSize * 0.5f),
                                18.0f + pulse * 5.0f,
                                Color{100, 230, 255, static_cast<unsigned char>(40 + 40 * pulse)});
            }
        }
    }
}
