#pragma once
#include "HTMLToPNGConverter.h"
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <sstream>
#include <set>
#include <chrono>

struct AdvancedBlock {
    int foreground = 0;
    int background = 0;
    int flags = 0;

    bool isEmpty() const {
        return foreground == 0 && background == 0;
    }
};

struct AdvancedWorldObject {
    int itemid = 0;
    int count = 0;
    int uid = 0;
    int x = 0;
    int y = 0;
};

struct AdvancedWorld {
    std::string name;
    int weather = 0;
    int width = 0;
    int height = 0;
    std::vector<AdvancedBlock> blocks;
    std::vector<AdvancedWorldObject> objects;
    bool loaded = false;
    static AdvancedWorld LoadWorld(const std::string& name, const std::string& worldPath, int itemCount);
};

struct ItemData {
    int m_id = 0;
    std::string m_texture;
    int m_texture_x = 0;
    int m_texture_y = 0;
    int m_spread_type = 0;
    int m_seed_base = 0;
    int m_seed_overlay = 0;
    int m_seed_color = 0;
    int m_seed_overlay_color = 0;
};

class ItemManager {
private:
    std::map<int, ItemData> items;
    std::vector<int> locks;
    int m_item_count = 0;
public:
    ItemManager();
    bool LoadItems(const std::string& filepath);
    ItemData GetItem(int itemID);
    bool ItemExists(int itemID) const;
    std::pair<int, int> GetDefaultTexture(int itemID);
    bool IsLock(int itemID) const;
    int GetItemCount() const { return m_item_count; }
};

class TextureManager {
private:
    std::vector<int> lut_8bit;
    std::vector<int> lut_4bit;
    std::unordered_map<std::string, std::vector<unsigned char>> textureCache;
    std::unordered_map<std::string, std::string> texturePaths;

    // Blob URL Optimization
    std::unordered_map<std::string, std::string> textureBlobURLCache;
    std::unordered_map<std::string, std::string> optimizedTextureCache;

    std::pair<int, int> CalculateSpreadType2(const AdvancedWorld& world, int item_id, int index, bool background);
    std::pair<int, int> CalculateSpreadType3(const AdvancedWorld& world, int item_id, int index, bool background);
    std::pair<int, int> CalculateSpreadType4(const AdvancedWorld& world, int item_id, int index, bool background);
    std::pair<int, int> CalculateSpreadType5(const AdvancedWorld& world, int item_id, int index, bool background);
    std::pair<int, int> CalculateSpreadType7(const AdvancedWorld& world, int item_id, int index, bool background);
    std::pair<int, int> CalculateSpreadType9(const AdvancedWorld& world, int item_id, int index, bool background);


public:
    std::vector<unsigned char> LoadImageToMemory(const std::string& filepath);
    TextureManager();
    bool LoadTexturesFromFolder(const std::string& folderPath);
    bool HasTexture(const std::string& name) const;
    std::string GetTexturePath(const std::string& name) const;
    std::string GetTextureAsBase64(const std::string& name);

    // Blob URL Methods untuk Optimasi
    std::string GetTextureAsBlobURL(const std::string& name);
    std::string GetTextureAsOptimizedBase64(const std::string& name);

    std::pair<int, int> GetOffset(const AdvancedWorld& world, int item_id, int index, int spread_type, bool background);
    std::pair<int, int> GetFlagOffset(const AdvancedWorld& world, int index, int flag);
};

class AdvancedWorldRenderer {
private:

    std::string serverName;
    std::string assetsPath;
    std::string worldPath;
    ItemManager itemManager;
    TextureManager textureManager;

    std::string EscapeHTML(const std::string& input);
    std::string SanitizeFilename(const std::string& filename);
    std::string ApplyColorFilter(int flags, int itemID);
    std::string GetItemImageAsBase64(int itemID);
    std::string GetWeatherTextureAsBase64(int weather);
    std::string GetTextureAsBase64(const std::string& textureName);
    std::string GetTextureAsBlobURL(const std::string& textureName);
    HTMLToPNGConverter pngConverter;

    // --- OPTIMIZED RENDERING SYSTEM ---
    using CssVarMap = std::map<std::string, std::string>;
    using ItemCssVarMap = std::map<int, std::string>;

    // Optimized rendering methods
    void RenderOptimizedBackgroundLayer(std::stringstream& html, const AdvancedWorld& world, const ItemCssVarMap& itemCssVars);
    void RenderOptimizedShadowLayer(std::stringstream& html, const AdvancedWorld& world,
        const ItemCssVarMap& itemCssVars, const CssVarMap& specialCssVars);
    void RenderOptimizedForegroundLayer(std::stringstream& html, const AdvancedWorld& world, const ItemCssVarMap& itemCssVars);
    void RenderOptimizedObjectsLayer(std::stringstream& html, const AdvancedWorld& world,
        const ItemCssVarMap& itemCssVars, const CssVarMap& specialCssVars);
    void RenderOptimizedEffectsLayer(std::stringstream& html, const AdvancedWorld& world, const CssVarMap& specialCssVars);

    // Optimized object rendering
    void RenderOptimizedRegularObject(std::stringstream& html, const AdvancedWorldObject& obj, const ItemCssVarMap& itemCssVars);
    void RenderOptimizedSeedObject(std::stringstream& html, const AdvancedWorldObject& obj, const CssVarMap& specialCssVars);
    void RenderOptimizedObjectShadow(std::stringstream& html, const AdvancedWorldObject& obj,
        const ItemCssVarMap& itemCssVars, const CssVarMap& specialCssVars);

    // Simple rendering methods (NEW - tambahkan ini)
    void RenderSimpleBackgroundLayer(std::stringstream& html, const AdvancedWorld& world, const ItemCssVarMap& itemCssVars);
    void RenderSimpleShadowLayer(std::stringstream& html, const AdvancedWorld& world,
        const ItemCssVarMap& itemCssVars, const CssVarMap& specialCssVars);
    void RenderSimpleForegroundLayer(std::stringstream& html, const AdvancedWorld& world, const ItemCssVarMap& itemCssVars);
    void RenderSimpleObjectsLayer(std::stringstream& html, const AdvancedWorld& world,
        const ItemCssVarMap& itemCssVars, const CssVarMap& specialCssVars);
    void RenderSimpleEffectsLayer(std::stringstream& html, const AdvancedWorld& world, const CssVarMap& specialCssVars);

public:
    AdvancedWorldRenderer(const std::string& serverName, const std::string& assetsPath, const std::string& worldPath);
    bool Initialize();
    std::string GenerateWorldHTML(const AdvancedWorld& world);
    bool RenderWorldToHTML(const AdvancedWorld& world, const std::string& outputPath);
    bool RenderWorldToPNG(const AdvancedWorld& world, const std::string& outputPath);
    bool RenderWorldToHTMLAndPNG(const AdvancedWorld& world, const std::string& baseOutputPath);
    ItemManager& GetItemManager() { return itemManager; }
    TextureManager& GetTextureManager() { return textureManager; }

    // Performance monitoring
    struct RenderStats {
        long htmlGenerationTime = 0;
        long pngConversionTime = 0;
        long totalRenderTime = 0;
        int uniqueTextures = 0;
        int totalBlocks = 0;
        int totalObjects = 0;
        size_t htmlFileSize = 0;
        size_t pngFileSize = 0;
    };

    RenderStats GetLastRenderStats() const { return lastRenderStats; }

private:
    RenderStats lastRenderStats;
};