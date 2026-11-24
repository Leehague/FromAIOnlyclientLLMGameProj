#pragma once

#include "core/IManager.h"
#include "core/Types.h"
#include <string>
#include <vector>

// [Requirement 5, 10] Parses TXT tilemap files and tiles.json, renders tile layers,
// provides tile collision checks and warp portal queries.
class MapManager : public IManager {
public:
    const char* GetName() const override { return "MapManager"; }
    bool Init() override;
    void Update(float dt) override;
    void Render() override;
    void Shutdown() override;

    bool LoadMap(const std::string& mapId);

    const std::string& GetCurrentMapId() const { return m_currentMapId; }
    const MapSpec* GetCurrentMapSpec() const { return m_currentMapSpec; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    int GetTileSize() const { return m_tileSize; }
    int GetTileAt(int tx, int ty) const;

    bool IsTileWalkable(int tx, int ty) const;
    bool IsCircleWalkable(Vector2 worldPos, float radius) const;
    Vector2 TileToWorldCenter(int tx, int ty) const;
    void WorldToTile(Vector2 worldPos, int& outTx, int& outTy) const;

    // 8-direction tile-based A* pathfinding with corner-cut prevention & entity clearance
    std::vector<Vector2> FindPath(Vector2 startWorld, Vector2 goalWorld, float entityRadius = 13.0f) const;

    const WarpSpec* FindWarpAtWorldPos(Vector2 worldPos) const;
    bool IsNearSpecialTile(Vector2 worldPos, const std::string& specialType, float rangeTiles = 1.7f) const;

private:
    std::string m_currentMapId;
    const MapSpec* m_currentMapSpec = nullptr;
    int m_width = 0;
    int m_height = 0;
    int m_tileSize = 32;
    std::vector<std::vector<int>> m_grid;
    float m_animTime = 0.0f;
};
