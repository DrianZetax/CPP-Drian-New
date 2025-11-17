#include "AdvancedWorldRenderer.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <random>
#include <filesystem>
#include <cmath>
#include "nlohmann/json.hpp"
#include <iostream>
#include <fstream>
#include <regex>
#include <set>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ============================================================================
// Base64 Encoding Helper
// ============================================================================

static const std::string base64_chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";
static inline bool is_base64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string Base64Encode(const std::vector<unsigned char>& data) {
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    size_t in_len = data.size();
    const unsigned char* bytes_to_encode = data.data();

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; (i < 4); i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; (j < i + 1); j++)
            ret += base64_chars[char_array_4[j]];

        while ((i++ < 3))
            ret += '=';
    }

    return ret;
}

// ============================================================================
// TextureManager Implementation dengan Optimasi Blob URL
// ============================================================================

TextureManager::TextureManager() {
    lut_8bit = std::vector<int>(256, 0);
    std::map<int, int> mappings = {
        {2, 11}, {8, 30}, {10, 44}, {11, 8}, {16, 29}, {18, 43}, {22, 7}, {24, 28},
        {26, 42}, {27, 41}, {30, 40}, {31, 2}, {64, 10}, {66, 9}, {72, 46}, {74, 36},
        {75, 35}, {80, 45}, {82, 33}, {86, 32}, {88, 39}, {90, 27}, {91, 23}, {94, 24},
        {95, 18}, {104, 6}, {106, 34}, {107, 4}, {120, 38}, {122, 25}, {123, 20},
        {126, 21}, {127, 16}, {208, 5}, {210, 31}, {214, 3}, {216, 37}, {218, 26},
        {219, 22}, {222, 19}, {223, 15}, {248, 1}, {250, 17}, {251, 14}, {254, 13}, {0, 12}
    };
    for (const auto& [key, value] : mappings) {
        if (key < 256) lut_8bit[key] = value;
    }

    lut_4bit = { 12, 11, 15, 8, 14, 7, 13, 2, 10, 9, 6, 4, 5, 3, 1, 0 };
}

std::vector<unsigned char> TextureManager::LoadImageToMemory(const std::string& filepath) {
    try {
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            std::cout << "[TextureManager] Cannot open image file: " << filepath << std::endl;
            return {};
        }

        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<unsigned char> buffer(fileSize);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
        file.close();

        std::cout << "[TextureManager] Loaded image: " << filepath << " (" << fileSize << " bytes)" << std::endl;
        return buffer;
    }
    catch (const std::exception& e) {
        std::cout << "[TextureManager] ERROR loading image " << filepath << ": " << e.what() << std::endl;
        return {};
    }
}

bool TextureManager::LoadTexturesFromFolder(const std::string& folderPath) {
    try {
        std::cout << "[TextureManager] Loading textures from: " << folderPath << std::endl;
        if (!fs::exists(folderPath)) {
            std::cout << "[TextureManager] WARNING: Texture folder does not exist: " << folderPath << std::endl;
            return false;
        }

        int loadedCount = 0;
        for (const auto& entry : fs::recursive_directory_iterator(folderPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".png") {
                std::string name = entry.path().stem().string();
                std::string path = entry.path().string();

                texturePaths[name] = path;
                loadedCount++;
            }
        }

        std::cout << "[TextureManager] Loaded " << loadedCount << " texture paths" << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cout << "[TextureManager] ERROR loading textures: " << e.what() << std::endl;
        return false;
    }
}

std::string TextureManager::GetTextureAsBase64(const std::string& name) {
    auto cacheIt = textureCache.find(name);
    if (cacheIt != textureCache.end()) {
        const auto& imageData = cacheIt->second;
        if (!imageData.empty()) {
            std::string base64 = Base64Encode(imageData);
            return "data:image/png;base64," + base64;
        }
    }

    auto pathIt = texturePaths.find(name);
    if (pathIt != texturePaths.end()) {
        auto imageData = LoadImageToMemory(pathIt->second);
        if (!imageData.empty()) {
            textureCache[name] = imageData;
            std::string base64 = Base64Encode(imageData);
            return "data:image/png;base64," + base64;
        }
    }

    std::vector<std::string> possiblePaths = {
        "render_world/cache/" + name + ".png",
        "render_world/cache/" + name,
        name + ".png",
        name,
        "cache/" + name + ".png",
        "../render_world/cache/" + name + ".png"
    };
    for (const auto& path : possiblePaths) {
        if (fs::exists(path)) {
            auto imageData = LoadImageToMemory(path);
            if (!imageData.empty()) {
                textureCache[name] = imageData;
                std::string base64 = Base64Encode(imageData);
                return "data:image/png;base64," + base64;
            }
        }
    }

    std::cout << "[TextureManager] WARNING: Cannot find texture for base64: " << name << std::endl;
    return "";
}

// ============================================================================
// METODE BARU: Blob URL Generation untuk Optimasi Decoding
// ============================================================================

std::string TextureManager::GetTextureAsBlobURL(const std::string& name) {
    // Cek cache Blob URL terlebih dahulu
    auto blobIt = textureBlobURLCache.find(name);
    if (blobIt != textureBlobURLCache.end()) {
        return blobIt->second;
    }

    // Dapatkan data Base64 terlebih dahulu
    std::string base64Data = GetTextureAsBase64(name);
    if (base64Data.empty()) {
        return "";
    }

    // Generate Blob URL JavaScript code
    std::string blobURL = "URL.createObjectURL(new Blob([atob('" +
        base64Data.substr(base64Data.find(",") + 1) +
        "')], {type: 'image/png'}))";

    textureBlobURLCache[name] = blobURL;
    return blobURL;
}

std::string TextureManager::GetTextureAsOptimizedBase64(const std::string& name) {
    // Untuk gambar yang sering digunakan, kita bisa pre-decode dan cache
    auto optIt = optimizedTextureCache.find(name);
    if (optIt != optimizedTextureCache.end()) {
        return optIt->second;
    }

    std::string base64 = GetTextureAsBase64(name);
    if (!base64.empty()) {
        // Simpan dalam cache optimized
        optimizedTextureCache[name] = base64;
    }

    return base64;
}

bool TextureManager::HasTexture(const std::string& name) const {
    return texturePaths.find(name) != texturePaths.end();
}

std::string TextureManager::GetTexturePath(const std::string& name) const {
    auto it = texturePaths.find(name);
    if (it != texturePaths.end()) {
        return it->second;
    }

    std::vector<std::string> possiblePaths = {
        "render_world/cache/" + name + ".png",
        "render_world/cache/" + name,
        name + ".png",
        name,
        "cache/" + name + ".png",
        "../render_world/cache/" + name + ".png"
    };

    for (const auto& path : possiblePaths) {
        if (fs::exists(path)) {
            return path;
        }
    }

    std::cout << "[TextureManager] WARNING: Cannot find texture: " << name << std::endl;
    return "";
}

std::pair<int, int> TextureManager::GetFlagOffset(const AdvancedWorld& world, int index, int flag) {
    int offset_x = 0;
    int offset_y = 0;

    std::vector<int> offsets = { -101, -100, -99, -1, 1, 99, 100, 101 };
    std::vector<int> left = { 0, 3, 5 };
    std::vector<int> right = { 2, 4, 6 };
    int bit = 0;
    for (int i = 0; i < offsets.size(); i++) {
        int offset = offsets[i];
        int neighborIndex = index + offset;
        bool withinBounds = neighborIndex >= 0 && neighborIndex < world.blocks.size();
        int x = index % world.width;
        bool isEdge = (std::find(left.begin(), left.end(), i) != left.end() && x == 0) ||
            (std::find(right.begin(), right.end(), i) != right.end() && x == world.width - 1);
        bool isMatchingNeighbor = withinBounds && (world.blocks[neighborIndex].flags & flag);
        if (isEdge || isMatchingNeighbor) {
            bit |= 1 << i;
        }
    }

    if (!(bit & 8) || !(bit & 2)) bit &= ~1;
    if (!(bit & 16) || !(bit & 2)) bit &= ~4;
    if (!(bit & 8) || !(bit & 64)) bit &= ~32;
    if (!(bit & 16) || !(bit & 64)) bit &= ~128;

    offset_x = lut_8bit[bit] % 8;
    offset_y = lut_8bit[bit] / 8;

    return { offset_x, offset_y };
}

std::pair<int, int> TextureManager::GetOffset(const AdvancedWorld& world, int item_id, int index, int spread_type, bool background) {
    switch (spread_type) {
    case 2: return CalculateSpreadType2(world, item_id, index, background);
    case 3: return CalculateSpreadType3(world, item_id, index, background);
    case 4: return CalculateSpreadType4(world, item_id, index, background);
    case 5: return CalculateSpreadType5(world, item_id, index, background);
    case 7: return CalculateSpreadType7(world, item_id, index, background);
    case 9: return CalculateSpreadType9(world, item_id, index, background);
    default: return { 0, 0 };
    }
}

std::pair<int, int> TextureManager::CalculateSpreadType2(const AdvancedWorld& world, int item_id, int index, bool background) {
    std::vector<int> offsets = { -101, -100, -99, -1, 1, 99, 100, 101 };
    std::vector<int> left = { 0, 3, 5 };
    std::vector<int> right = { 2, 4, 6 };
    int bit = 0;
    for (int i = 0; i < offsets.size(); i++) {
        int offset = offsets[i];
        int neighborIndex = index + offset;
        bool withinBounds = neighborIndex >= 0 && neighborIndex < world.blocks.size();
        int x = index % world.width;
        bool isEdge = (std::find(left.begin(), left.end(), i) != left.end() && x == 0) ||
            (std::find(right.begin(), right.end(), i) != right.end() && x == world.width - 1);

        int neighborItem = 0;
        if (withinBounds) {
            neighborItem = background ?
                world.blocks[neighborIndex].background : world.blocks[neighborIndex].foreground;
        }
        bool isMatchingNeighbor = withinBounds && (neighborItem == item_id);
        if (isEdge || isMatchingNeighbor) {
            bit |= 1 << i;
        }
    }

    if (!(bit & 8) || !(bit & 2)) bit &= ~1;
    if (!(bit & 16) || !(bit & 2)) bit &= ~4;
    if (!(bit & 8) || !(bit & 64)) bit &= ~32;
    if (!(bit & 16) || !(bit & 64)) bit &= ~128;

    int offset_x = lut_8bit[bit] % 8;
    int offset_y = lut_8bit[bit] / 8;

    return { offset_x, offset_y };
}

std::pair<int, int> TextureManager::CalculateSpreadType3(const AdvancedWorld& world, int item_id, int index, bool background) {
    std::vector<int> offsets = { -1, 1 };
    int offset_x = 3;

    for (int i = 0; i < offsets.size(); i++) {
        int offset = offsets[i];
        int neighborIndex = index + offset;
        bool withinBounds = neighborIndex >= 0 && neighborIndex < world.blocks.size();
        int x = index % world.width;
        bool isEdge = (i == 0 && x == 0) ||
            (i == 1 && x == world.width - 1);

        int neighborItem = 0;
        if (withinBounds) {
            neighborItem = background ?
                world.blocks[neighborIndex].background : world.blocks[neighborIndex].foreground;
        }
        bool isMatchingNeighbor = withinBounds && (neighborItem == item_id);
        if (isEdge) {
            offset_x = isMatchingNeighbor ?
                1 : 0;
        }
        else {
            offset_x = isMatchingNeighbor ?
                2 : offset_x;
        }
    }

    return { offset_x, 0 };
}

std::pair<int, int> TextureManager::CalculateSpreadType4(const AdvancedWorld& world, int item_id, int index, bool background) {
    std::vector<int> offsets = { -1, -100, 1, 100 };
    int offset_x = 4;

    for (int i = 0; i < offsets.size(); i++) {
        int offset = offsets[i];
        int neighborIndex = index + offset;
        bool withinBounds = neighborIndex >= 0 && neighborIndex < world.blocks.size();
        int x = index % world.width;
        bool isEdge = (offset == -1 && x == 0) ||
            (offset == 1 && x == world.width - 1);

        int neighborItem = 0;
        if (withinBounds) {
            neighborItem = background ?
                world.blocks[neighborIndex].background : world.blocks[neighborIndex].foreground;
        }
        bool isMatchingNeighbor = withinBounds && (neighborItem == item_id);
        if (isEdge || isMatchingNeighbor) {
            offset_x = i;
        }
    }

    return { offset_x, 0 };
}

std::pair<int, int> TextureManager::CalculateSpreadType5(const AdvancedWorld& world, int item_id, int index, bool background) {
    std::vector<int> offsets = { -100, -1, 1, 100 };
    int bit = 0;

    for (int i = 0; i < offsets.size(); i++) {
        int offset = offsets[i];
        int neighborIndex = index + offset;
        bool withinBounds = neighborIndex >= 0 && neighborIndex < world.blocks.size();
        int x = index % world.width;
        bool isEdge = (i == 1 && x == 0) ||
            (i == 2 && x == world.width - 1);

        int neighborItem = 0;
        if (withinBounds) {
            neighborItem = background ?
                world.blocks[neighborIndex].background : world.blocks[neighborIndex].foreground;
        }
        bool isMatchingNeighbor = withinBounds && (neighborItem == item_id);
        if (isEdge || isMatchingNeighbor) {
            bit |= 1 << i;
        }
    }

    int offset_x = lut_4bit[bit] % 8;
    int offset_y = lut_4bit[bit] / 8;
    return { offset_x, offset_y };
}

std::pair<int, int> TextureManager::CalculateSpreadType7(const AdvancedWorld& world, int item_id, int index, bool background) {
    std::vector<int> offsets = { 100, -100 };
    int offset_x = 3;

    for (int i = 0; i < offsets.size(); i++) {
        int offset = offsets[i];
        int neighborIndex = index + offset;
        bool withinBounds = neighborIndex >= 0 && neighborIndex < world.blocks.size();

        int neighborItem = 0;
        if (withinBounds) {
            neighborItem = background ?
                world.blocks[neighborIndex].background : world.blocks[neighborIndex].foreground;
        }
        bool isMatchingNeighbor = withinBounds && (neighborItem == item_id);
        if (isMatchingNeighbor) {
            if (i == 0) offset_x = 2;
            else if (i == 1) offset_x = 0;
        }
    }

    return { offset_x, 0 };
}

std::pair<int, int> TextureManager::CalculateSpreadType9(const AdvancedWorld& world, int item_id, int index, bool background) {
    std::vector<int> offsets = { 1, -1, 100, -100 };
    int offset_x = 3;

    for (int i = 0; i < offsets.size(); i++) {
        int offset = offsets[i];
        int neighborIndex = index + offset;
        bool withinBounds = neighborIndex >= 0 && neighborIndex < world.blocks.size();
        int x = index % world.width;
        bool isEdge = (offset == -1 && x == 0) ||
            (offset == 1 && x == world.width - 1);

        int neighborItem = 0;
        if (withinBounds) {
            neighborItem = background ?
                world.blocks[neighborIndex].background : world.blocks[neighborIndex].foreground;
        }
        bool isMatchingNeighbor = withinBounds && (neighborItem == item_id);
        if (isEdge || isMatchingNeighbor) {
            offset_x = i;
        }
    }

    return { offset_x, 0 };
}

// ============================================================================
// ItemManager Implementation  
// ============================================================================

ItemManager::ItemManager() {
    locks = { 202, 204, 206, 242, 1796, 2408, 2950, 4428, 4802, 4994, 5260, 5814, 5980, 7188, 8470, 9640, 10410, 11550, 11586, 11902, 12654, 13200, 13636 };
}

bool ItemManager::LoadItems(const std::string& filepath) {
    try {
        std::cout << "[ItemManager] Loading items from: " << filepath << std::endl;
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cout << "[ItemManager] ERROR: Cannot open items file: " << filepath << std::endl;
            return false;
        }

        json itemsData;
        file >> itemsData;

        m_item_count = itemsData.value("m_item_count", 0);
        std::cout << "[ItemManager] Total items in file: " << m_item_count << std::endl;
        if (itemsData.contains("m_items") && itemsData["m_items"].is_array()) {
            int loadedItems = 0;
            for (const auto& itemData : itemsData["m_items"]) {
                ItemData item;
                item.m_id = itemData.value("m_id", 0);
                item.m_texture = itemData.value("m_texture", "");
                item.m_texture_x = itemData.value("m_texture_x", 0);
                item.m_texture_y = itemData.value("m_texture_y", 0);
                item.m_spread_type = itemData.value("m_spread_type", 0);
                item.m_seed_base = itemData.value("m_seed_base", 0);
                item.m_seed_overlay = itemData.value("m_seed_overlay", 0);
                item.m_seed_color = itemData.value("m_seed_color", 0);
                item.m_seed_overlay_color = itemData.value("m_seed_overlay_color", 0);
                if (item.m_id > 0) {
                    items[item.m_id] = item;
                    loadedItems++;
                }
            }
            std::cout << "[ItemManager] Successfully loaded " << loadedItems << " items" << std::endl;
        }
        else {
            std::cout << "[ItemManager] WARNING: No m_items array found in items.json" << std::endl;
        }

        return true;
    }
    catch (const std::exception& e) {
        std::cout << "[ItemManager] ERROR parsing items.json: " << e.what() << std::endl;
        return false;
    }
}

ItemData ItemManager::GetItem(int itemID) {
    auto it = items.find(itemID);
    if (it != items.end()) {
        return it->second;
    }

    ItemData defaultItem;
    defaultItem.m_id = itemID;
    defaultItem.m_texture = std::to_string(itemID);
    return defaultItem;
}

bool ItemManager::ItemExists(int itemID) const {
    return items.find(itemID) != items.end();
}

std::pair<int, int> ItemManager::GetDefaultTexture(int itemID) {
    ItemData item = GetItem(itemID);
    int m_default_texture_x = 0;
    int m_default_texture_y = 0;
    switch (item.m_spread_type) {
    case 2:
    case 5: {
        m_default_texture_x = item.m_texture_x + 4;
        m_default_texture_y = item.m_texture_y + 1;
        break;
    }
    case 3:
    case 7: {
        m_default_texture_x = item.m_texture_x + 3;
        m_default_texture_y = item.m_texture_y;
        break;
    }
    default: {
        m_default_texture_x = item.m_texture_x;
        m_default_texture_y = item.m_texture_y;
        break;
    }
    }
    return { m_default_texture_x, m_default_texture_y };
}

bool ItemManager::IsLock(int itemID) const {
    return std::find(locks.begin(), locks.end(), itemID) != locks.end();
}

// ============================================================================
// AdvancedWorld Implementation
// ============================================================================

AdvancedWorld AdvancedWorld::LoadWorld(const std::string& name, const std::string& worldPath, int itemCount) {
    AdvancedWorld world;
    world.name = name;

    std::string sanitizedName = name;
    std::replace_if(sanitizedName.begin(), sanitizedName.end(),
        [](char c) { return !std::isalnum(c) && c != '_' && c != '-'; }, '_');

    std::string databasePath = "database/worlds";
    std::string filename = databasePath + "/" + sanitizedName + "_.json";
    std::vector<std::string> possibleFilenames = { filename };
    std::string filepath;
    std::ifstream file;

    for (const auto& filename : possibleFilenames) {
        std::cout << "[WorldLoader] Trying to open: " << filename << std::endl;
        file.open(filename);
        if (file.is_open()) {
            filepath = filename;
            std::cout << "[WorldLoader] Successfully opened: " << filename << std::endl;
            break;
        }
        else {
            std::cout << "[WorldLoader] Failed to open: " << filename << " (Error: " << strerror(errno) << ")" << std::endl;
        }
    }

    if (!file.is_open()) {
        std::cout << "[WorldLoader] ERROR: Cannot open world file for: " << name << " (sanitized: " << sanitizedName << ")" << std::endl;
        std::cout << "[WorldLoader] Expected path: " << databasePath << "/" << sanitizedName << "_.json" << std::endl;

        try {
            if (fs::exists(databasePath)) {
                for (const auto& entry : fs::directory_iterator(databasePath)) {
                    if (entry.is_regular_file()) {
                        std::cout << "  - " << entry.path().filename() << std::endl;
                    }
                }
            }
            else {
                std::cout << "[WorldLoader] Database directory does not exist: " << databasePath << std::endl;
            }
        }
        catch (const std::exception& e) {
            std::cout << "[WorldLoader] Error listing directory: " << e.what() << std::endl;
        }

        return world;
    }

    try {
        json worldData;
        file >> worldData;
        file.close();

        world.weather = worldData.value("weather", 0);
        std::cout << "[WorldLoader] Weather: " << world.weather << std::endl;

        if (worldData.contains("blocks") && worldData["blocks"].is_array()) {
            const auto& blocksArray = worldData["blocks"];
            int totalBlocks = blocksArray.size();
            std::cout << "[WorldLoader] Found " << totalBlocks << " blocks in JSON array" << std::endl;

            world.blocks.resize(totalBlocks);
            int validBlocks = 0;
            for (int i = 0; i < totalBlocks; i++) {
                const auto& blockData = blocksArray[i];
                AdvancedBlock& block = world.blocks[i];

                if (blockData.is_null()) {
                    continue;
                }

                if (blockData.is_object()) {
                    if (blockData.contains("f")) {
                        auto fgValue = blockData["f"];
                        if (fgValue.is_number()) {
                            int fg = fgValue.get<int>();
                            if (fg >= 0 && fg < itemCount) {
                                block.foreground = fg;
                                validBlocks++;
                            }
                        }
                    }

                    if (blockData.contains("b")) {
                        auto bgValue = blockData["b"];
                        if (bgValue.is_number()) {
                            int bg = bgValue.get<int>();
                            if (bg >= 0 && bg < itemCount) {
                                block.background = bg;
                            }
                        }
                    }

                    if (blockData.contains("fl")) {
                        auto flValue = blockData["fl"];
                        if (flValue.is_number()) {
                            block.flags = flValue.get<int>();
                        }
                    }
                }
            }

            std::cout << "[WorldLoader] Valid blocks loaded: " << validBlocks << std::endl;

            if (totalBlocks == 6000) {
                world.width = 100;
                world.height = 60;
            }
            else if (totalBlocks == 12000) {
                world.width = 100;
                world.height = 120;
            }
            else {
                world.width = 100;
                world.height = totalBlocks / world.width;
                if (totalBlocks % world.width != 0) {
                    world.height++;
                }
            }

            std::cout << "[WorldLoader] World dimensions: " << world.width << "x" << world.height << std::endl;
        }
        else {
            std::cout << "[WorldLoader] WARNING: No blocks array found or blocks is not an array" << std::endl;
        }

        std::vector<std::string> objectFields = { "drop_new", "objects", "obj", "drops" };
        for (const auto& field : objectFields) {
            if (worldData.contains(field) && worldData[field].is_array()) {
                int objectCount = 0;
                for (const auto& objectData : worldData[field]) {
                    if (objectData.is_array() && objectData.size() >= 5) {
                        AdvancedWorldObject obj;
                        obj.itemid = objectData[0].is_null() ? 0 : objectData[0].get<int>();
                        obj.count = objectData[1].is_null() ? 0 : objectData[1].get<int>();
                        obj.uid = objectData[2].is_null() ? 0 : objectData[2].get<int>();
                        obj.x = objectData[3].is_null() ? 0 : objectData[3].get<int>();
                        obj.y = objectData[4].is_null() ? 0 : objectData[4].get<int>();
                        if (obj.itemid != 0) {
                            world.objects.push_back(obj);
                            objectCount++;
                        }
                    }
                }
                if (objectCount > 0) {
                    std::cout << "[WorldLoader] Loaded " << objectCount << " objects from field: " << field << std::endl;
                    break;
                }
            }
        }

        world.loaded = true;
        std::cout << "[WorldLoader] Successfully loaded world: " << name << " from " << filepath << std::endl;
    }
    catch (const std::exception& e) {
        std::cout << "[WorldLoader] ERROR parsing world file: " << e.what() << std::endl;
    }

    return world;
}

// ============================================================================
// AdvancedWorldRenderer Implementation dengan Optimasi Blob URL
// ============================================================================

AdvancedWorldRenderer::AdvancedWorldRenderer(const std::string& serverName, const std::string& assetsPath, const std::string& worldPath)
    : serverName(serverName), assetsPath(assetsPath), worldPath(worldPath) {}

bool AdvancedWorldRenderer::Initialize() {
    std::cout << "[Renderer] Initializing AdvancedWorldRenderer..." << std::endl;

    std::string actualAssetsPath = "render_world/";
    std::string actualCachePath = "render_world/cache/";
    std::string actualItemsPath = "render_world/items.json";
    std::string actualWorldPath = "database/worlds";

    if (!fs::exists(actualWorldPath)) {
        if (!fs::create_directories(actualWorldPath)) {
            std::cout << "[Renderer] WARNING: Failed to create database/worlds directory: " << actualWorldPath << std::endl;
        }
        else {
            std::cout << "[Renderer] Created database/worlds directory: " << actualWorldPath << std::endl;
        }
    }

    if (!fs::exists(actualAssetsPath)) {
        std::cout << "[Renderer] Creating assets directory: " << actualAssetsPath << std::endl;
        if (!fs::create_directories(actualAssetsPath)) {
            std::cout << "[Renderer] ERROR: Failed to create assets directory: " << actualAssetsPath << std::endl;
            return false;
        }
        else {
            std::cout << "[Renderer] Created assets directory: " << actualAssetsPath << std::endl;
        }
    }

    if (!fs::exists(actualCachePath)) {
        std::cout << "[Renderer] Creating cache directory: " << actualCachePath << std::endl;
        if (!fs::create_directories(actualCachePath)) {
            std::cout << "[Renderer] WARNING: Failed to create cache directory: " << actualCachePath << std::endl;
        }
        else {
            std::cout << "[Renderer] Created cache directory: " << actualCachePath << std::endl;
        }
    }

    std::cout << "[Renderer] Loading items from: " << actualItemsPath << std::endl;
    if (!itemManager.LoadItems(actualItemsPath)) {
        std::cout << "[Renderer] ERROR: Failed to load items.json!" << std::endl;
        return false;
    }

    std::cout << "[Renderer] Loading textures from: " << actualCachePath << std::endl;
    if (!textureManager.LoadTexturesFromFolder(actualCachePath)) {
        std::cout << "[Renderer] WARNING: Could not load textures from cache" << std::endl;
    }

    this->worldPath = actualWorldPath;
    std::cout << "[Renderer] World path set to: " << this->worldPath << std::endl;
    std::cout << "[Renderer] Assets path set to: " << actualAssetsPath << std::endl;
    std::cout << "[Renderer] Successfully loaded " << itemManager.GetItemCount() << " items" << std::endl;
    std::cout << "[Renderer] Initialization completed successfully!" << std::endl;
    return true;
}

std::string AdvancedWorldRenderer::EscapeHTML(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    for (char c : input) {
        switch (c) {
        case '&': output += "&amp;"; break;
        case '<': output += "&lt;"; break;
        case '>': output += "&gt;"; break;
        case '"': output += "&quot;"; break;
        case '\'': output += "&#39;"; break;
        default: output += c; break;
        }
    }
    return output;
}

std::string AdvancedWorldRenderer::SanitizeFilename(const std::string& filename) {
    std::string sanitized = filename;
    std::replace_if(sanitized.begin(), sanitized.end(),
        [](char c) { return !std::isalnum(c) && c != '_' && c != '-' && c != '.'; }, '_');
    return sanitized;
}

std::string AdvancedWorldRenderer::ApplyColorFilter(int flags, int itemID) {
    if (!flags && itemID != 2590) return "";

    try {
        if (itemID == 2590) {
            return "animation: rainbow 2s infinite;";
        }

        int red = (flags & 0x20000000) ? 1 : 0;
        int green = (flags & 0x40000000) ? 1 : 0;
        int blue = (flags & 0x80000000) ? 1 : 0;

        if (red && !green && !blue) {
            return "filter: hue-rotate(0deg) saturate(2);";
        }
        else if (!red && green && !blue) {
            return "filter: hue-rotate(120deg) saturate(2);";
        }
        else if (!red && !green && blue) {
            return "filter: hue-rotate(240deg) saturate(2);";
        }
        else if (red && green && !blue) {
            return "filter: hue-rotate(60deg) saturate(2);";
        }
        else if (red && !green && blue) {
            return "filter: hue-rotate(300deg) saturate(2);";
        }
        else if (!red && green && blue) {
            return "filter: hue-rotate(180deg) saturate(2);";
        }
        else if (red && green && blue) {
            return "";
        }

        return "";
    }
    catch (...) {
        return "";
    }
}

std::string AdvancedWorldRenderer::GetTextureAsBase64(const std::string& textureName) {
    return textureManager.GetTextureAsBase64(textureName);
}

std::string AdvancedWorldRenderer::GetTextureAsBlobURL(const std::string& textureName) {
    return textureManager.GetTextureAsBlobURL(textureName);
}

std::string AdvancedWorldRenderer::GetItemImageAsBase64(int itemID) {
    if (itemID == 0) return "";
    try {
        ItemData item = itemManager.GetItem(itemID);
        std::vector<std::string> possibleNames = {
            item.m_texture,
            std::to_string(itemID),
            "item_" + std::to_string(itemID),
            "block_" + std::to_string(itemID)
        };
        for (const auto& textureName : possibleNames) {
            if (textureName.empty()) continue;
            std::string base64 = GetTextureAsBase64(textureName);
            if (!base64.empty()) {
                return base64;
            }
        }

        std::cout << "[Renderer] WARNING: No base64 texture found for item " << itemID << std::endl;
        return "";

    }
    catch (const std::exception& e) {
        std::cout << "[Renderer] ERROR in GetItemImageAsBase64 for item " << itemID << ": " << e.what() << std::endl;
        return "";
    }
}

std::string AdvancedWorldRenderer::GetWeatherTextureAsBase64(int weather) {
    std::vector<std::string> weatherTextures = { "0", "1", "2", "3" };
    if (weather >= 0 && weather < weatherTextures.size()) {
        return GetTextureAsBase64(weatherTextures[weather]);
    }
    return "";
}

// ============================================================================
// OPTIMIZED HTML Generation Methods dengan Blob URL Support
// ============================================================================

std::string AdvancedWorldRenderer::GenerateWorldHTML(const AdvancedWorld& world) {
    try {
        if (!world.loaded) {
            std::cout << "[HTML] ERROR: World not loaded properly" << std::endl;
            return "<html><body><h1>Error: World not loaded properly</h1></body></html>";
        }

        std::cout << "[HTML] Starting BINARY-OPTIMIZED HTML generation for world: " << world.name << std::endl;
        std::cout << "[HTML] World size: " << world.width << "x" << world.height << std::endl;
        std::cout << "[HTML] Total blocks: " << world.blocks.size() << std::endl;
        std::cout << "[HTML] Total objects: " << world.objects.size() << std::endl;

        auto startTime = std::chrono::high_resolution_clock::now();

        // --- PHASE 1: Collect Unique Items ---
        std::set<int> uniqueItemIds;

        for (const auto& block : world.blocks) {
            if (block.foreground != 0) uniqueItemIds.insert(block.foreground);
            if (block.background != 0) uniqueItemIds.insert(block.background);
        }

        for (const auto& obj : world.objects) {
            if (obj.itemid != 0) uniqueItemIds.insert(obj.itemid);
        }

        std::cout << "[HTML] Found " << uniqueItemIds.size() << " unique items" << std::endl;

        // --- PHASE 2: Load binary image data langsung ---
        std::map<int, std::vector<unsigned char>> itemBinaryCache;
        std::map<std::string, std::vector<unsigned char>> textureBinaryCache;

        // Helper function untuk load binary data langsung
        auto loadBinaryData = [&](const std::string& textureName) -> std::vector<unsigned char> {
            std::string path = textureManager.GetTexturePath(textureName);
            if (!path.empty()) {
                return textureManager.LoadImageToMemory(path);
            }
            return {};
            };

        // Load binary data untuk semua item unik
        for (int itemId : uniqueItemIds) {
            ItemData item = itemManager.GetItem(itemId);
            std::vector<std::string> possibleNames = {
                item.m_texture,
                std::to_string(itemId),
                "item_" + std::to_string(itemId),
                "block_" + std::to_string(itemId)
            };

            for (const auto& textureName : possibleNames) {
                if (textureName.empty()) continue;
                auto binaryData = loadBinaryData(textureName);
                if (!binaryData.empty()) {
                    itemBinaryCache[itemId] = binaryData;
                    break;
                }
            }
        }

        // Load binary data untuk texture khusus
        textureBinaryCache["weather"] = loadBinaryData(std::to_string(world.weather));
        textureBinaryCache["seed"] = loadBinaryData("seed");
        textureBinaryCache["water"] = loadBinaryData("water");
        textureBinaryCache["fire"] = loadBinaryData("fire");

        std::cout << "[HTML] Loaded " << itemBinaryCache.size() << " item binaries and "
            << textureBinaryCache.size() << " special texture binaries" << std::endl;

        // --- PHASE 3: Generate HTML dengan JavaScript Binary Loader ---
        std::stringstream html;

        html << "<!DOCTYPE html>"
            << "<html>"
            << "<head>"
            << "<title>World Render - " << EscapeHTML(world.name) << "</title>"
            << "<meta charset=\"UTF-8\">"
            << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
            << "<style>"
            << "body { margin: 0; padding: 0; overflow: auto; background: #000; font-family: Arial, sans-serif; }"
            << ".world-container { "
            << "  width: " << (world.width * 32) << "px; "
            << "  height: " << (world.height * 32) << "px; "
            << "  position: relative; "
            << "  background: #1a1a2e; "
            << "  margin: 0 auto; "
            << "  image-rendering: pixelated; "
            << "}"
            << ".layer { position: absolute; top: 0; left: 0; width: 100%; height: 100%; pointer-events: none; }"
            << ".block { "
            << "  position: absolute; "
            << "  width: 32px; "
            << "  height: 32px; "
            << "  image-rendering: pixelated; "
            << "  pointer-events: none; "
            << "  background-size: auto !important;"
            << "}"
            << ".background-block { z-index: 1; }"
            << ".shadow-block { filter: brightness(0) opacity(0.56); z-index: 2; }"
            << ".foreground-block { z-index: 3; }"
            << ".object { "
            << "  position: absolute; "
            << "  image-rendering: pixelated; "
            << "  pointer-events: none; "
            << "  z-index: 4; "
            << "  background-repeat: no-repeat;"
            << "  background-position: center;"
            << "  background-size: contain;"
            << "}"
            << ".object-shadow { filter: brightness(0) opacity(0.56); z-index: 2; }"
            << ".item-count { "
            << "  position: absolute; "
            << "  color: white; "
            << "  font-size: 10px; "
            << "  font-weight: bold; "
            << "  text-shadow: 1px 1px 2px black; "
            << "  z-index: 6; "
            << "  pointer-events: none; "
            << "}"
            << ".water-overlay, .fire-overlay { "
            << "  position: absolute; "
            << "  width: 32px; "
            << "  height: 32px; "
            << "  opacity: 0.57; "
            << "  z-index: 4; "
            << "  pointer-events: none; "
            << "  background-size: contain;"
            << "}"
            << "@keyframes rainbow { "
            << "  0% { filter: hue-rotate(0deg) saturate(2); }"
            << "  100% { filter: hue-rotate(360deg) saturate(2); }"
            << "}"
            << ".rainbow-block { animation: rainbow 3s infinite linear; }"
            << ".flipped { transform: scaleX(-1); }"
            << ".footer { "
            << "  position: fixed; "
            << "  bottom: 20px; "
            << "  left: 50%; "
            << "  transform: translateX(-50%); "
            << "  text-align: center; "
            << "  color: white; "
            << "  background: rgba(0,0,0,0.5); "
            << "  padding: 10px 20px; "
            << "  border-radius: 10px; "
            << "  z-index: 1000;"
            << "}"
            << ".stats { "
            << "  position: fixed; "
            << "  top: 10px; "
            << "  right: 10px; "
            << "  background: rgba(0,0,0,0.7); "
            << "  color: white; "
            << "  padding: 10px; "
            << "  border-radius: 5px; "
            << "  z-index: 1000; "
            << "}"
            << ".loading { "
            << "  position: fixed; "
            << "  top: 50%; "
            << "  left: 50%; "
            << "  transform: translate(-50%, -50%); "
            << "  background: rgba(0,0,0,0.8); "
            << "  color: white; "
            << "  padding: 20px; "
            << "  border-radius: 10px; "
            << "  text-align: center; "
            << "  z-index: 10000; "
            << "}"
            << "</style>"
            << "</head>"
            << "<body>"
            << "<div class='loading' id='loading'>"
            << "  <h3>Loading World Render...</h3>"
            << "  <div id='progress'>Initializing textures (0/" << itemBinaryCache.size() << ")</div>"
            << "</div>"
            << "<div class='world-container' id='world-container' style='display:none;'>";

        // Statistics
        int foregroundBlocks = 0;
        int backgroundBlocks = 0;
        int emptyBlocks = 0;

        for (const auto& block : world.blocks) {
            if (block.foreground > 0) foregroundBlocks++;
            if (block.background > 0) backgroundBlocks++;
            if (block.isEmpty()) emptyBlocks++;
        }

        html << "<div class='stats'>"
            << "<strong>World Statistics</strong><br>"
            << "Size: " << world.width << "x" << world.height << "<br>"
            << "Foreground: " << foregroundBlocks << "<br>"
            << "Background: " << backgroundBlocks << "<br>"
            << "Empty: " << emptyBlocks << "<br>"
            << "Objects: " << world.objects.size() << "<br>"
            << "Weather: " << world.weather << "<br>"
            << "Textures: " << itemBinaryCache.size()
            << "</div>";

        // Render semua layer (akan diisi oleh JavaScript)
        html << "<div class='layer' id='background-layer'></div>"
            << "<div class='layer' id='shadow-layer'></div>"
            << "<div class='layer' id='foreground-layer'></div>"
            << "<div class='layer' id='objects-layer'></div>"
            << "<div class='layer' id='effects-layer'></div>";

        // --- JavaScript untuk Binary Data Loading dan Rendering ---
        html << "<script>"
            << "class WorldRenderer {"
            << "  constructor() {"
            << "    this.textureCache = new Map();"
            << "    this.blobURLs = new Set();"
            << "    this.totalTextures = " << itemBinaryCache.size() << ";"
            << "    this.loadedTextures = 0;"
            << "  }"

            << "  // Convert binary data ke Blob URL"
            << "  createBlobURL(binaryData) {"
            << "    const blob = new Blob([new Uint8Array(binaryData)], {type: 'image/png'});"
            << "    const url = URL.createObjectURL(blob);"
            << "    this.blobURLs.add(url);"
            << "    return url;"
            << "  }"

            << "  // Load texture dari binary data"
            << "  async loadTexture(itemId, binaryData) {"
            << "    return new Promise((resolve) => {"
            << "      const blobURL = this.createBlobURL(binaryData);"
            << "      const img = new Image();"
            << "      img.onload = () => {"
            << "        this.textureCache.set(itemId, { blobURL, img });"
            << "        this.loadedTextures++;"
            << "        this.updateProgress();"
            << "        resolve();"
            << "      };"
            << "      img.src = blobURL;"
            << "    });"
            << "  }"

            << "  // Update progress loading"
            << "  updateProgress() {"
            << "    const progress = document.getElementById('progress');"
            << "    if (progress) {"
            << "      progress.textContent = `Loading textures (${this.loadedTextures}/${this.totalTextures})`;"
            << "    }"
            << "  }"

            << "  // Render block dengan texture yang sudah diload"
            << "  renderBlock(layer, x, y, itemId, offsetX = 0, offsetY = 0, flip = false, filter = '') {"
            << "    const texture = this.textureCache.get(itemId);"
            << "    if (!texture) return;"
            << "    "
            << "    const block = document.createElement('div');"
            << "    block.className = 'block ' + layer;"
            << "    block.style.cssText = `"
            << "      left: ${x}px; "
            << "      top: ${y}px; "
            << "      background-image: url('${texture.blobURL}');"
            << "      background-position: -${offsetX}px -${offsetY}px;"
            << "      ${flip ? 'transform: scaleX(-1);' : ''}"
            << "      ${filter}"
            << "    `;"
            << "    "
            << "    document.getElementById(layer + '-layer').appendChild(block);"
            << "  }"

            << "  // Render object"
            << "  renderObject(x, y, itemId, count = 0) {"
            << "    const texture = this.textureCache.get(itemId);"
            << "    if (!texture) return;"
            << "    "
            << "    const obj = document.createElement('div');"
            << "    obj.className = 'object';"
            << "    obj.style.cssText = `"
            << "      left: ${x}px; "
            << "      top: ${y}px; "
            << "      background-image: url('${texture.blobURL}');"
            << "    `;"
            << "    "
            << "    document.getElementById('objects-layer').appendChild(obj);"
            << "    "
            << "    if (count > 1 && itemId !== 112) {"
            << "      const countDiv = document.createElement('div');"
            << "      countDiv.className = 'item-count';"
            << "      countDiv.style.cssText = `left: ${x + 8}px; top: ${y + 12}px;`;"
            << "      countDiv.textContent = count;"
            << "      document.getElementById('objects-layer').appendChild(countDiv);"
            << "    }"
            << "  }"

            << "  // Cleanup"
            << "  cleanup() {"
            << "    this.blobURLs.forEach(url => URL.revokeObjectURL(url));"
            << "  }"
            << "}"

            << "// World data untuk rendering"
            << "const worldData = {"
            << "  width: " << world.width << ","
            << "  height: " << world.height << ","
            << "  blocks: [";

        // Serialize block data
        for (int i = 0; i < world.blocks.size(); i++) {
            const auto& block = world.blocks[i];
            if (i > 0) html << ",";
            html << "{"
                << "fg:" << block.foreground << ","
                << "bg:" << block.background << ","
                << "flags:" << block.flags << ","
                << "x:" << (i % world.width) * 32 << ","
                << "y:" << (i / world.width) * 32
                << "}";
        }

        html << "],"
            << "objects: [";

        // Serialize object data
        for (int i = 0; i < world.objects.size(); i++) {
            const auto& obj = world.objects[i];
            if (i > 0) html << ",";
            html << "{"
                << "id:" << obj.itemid << ","
                << "count:" << obj.count << ","
                << "x:" << obj.x << ","
                << "y:" << obj.y
                << "}";
        }

        html << "]"
            << "};"

            << "// Binary texture data"
            << "const textureData = {";

        // Serialize binary texture data
        bool first = true;
        for (const auto& [itemId, binaryData] : itemBinaryCache) {
            if (!first) html << ",";
            html << itemId << ": [";
            for (size_t i = 0; i < binaryData.size(); i++) {
                if (i > 0) html << ",";
                html << static_cast<int>(binaryData[i]);
            }
            html << "]";
            first = false;
        }

        html << "};"

            << "// Main rendering function"
            << "async function startRendering() {"
            << "  const renderer = new WorldRenderer();"
            << "  "
            << "  try {"
            << "    // Load semua texture"
            << "    const loadPromises = [];"
            << "    for (const [itemId, binaryArray] of Object.entries(textureData)) {"
            << "      loadPromises.push(renderer.loadTexture(parseInt(itemId), binaryArray));"
            << "    }"
            << "    "
            << "    await Promise.all(loadPromises);"
            << "    "
            << "    // Render blocks"
            << "    worldData.blocks.forEach(block => {"
            << "      if (block.bg > 0) {"
            << "        renderer.renderBlock('background', block.x, block.y, block.bg);"
            << "      }"
            << "      if (block.fg > 0) {"
            << "        renderer.renderBlock('shadow', block.x - 4, block.y + 4, block.fg);"
            << "        renderer.renderBlock('foreground', block.x, block.y, block.fg);"
            << "      }"
            << "    });"
            << "    "
            << "    // Render objects"
            << "    worldData.objects.forEach(obj => {"
            << "      if (obj.id > 0) {"
            << "        renderer.renderObject(obj.x, obj.y, obj.id, obj.count);"
            << "      }"
            << "    });"
            << "    "
            << "    // Hide loading, show world"
            << "    document.getElementById('loading').style.display = 'none';"
            << "    document.getElementById('world-container').style.display = 'block';"
            << "    "
            << "    // Cleanup on unload"
            << "    window.addEventListener('beforeunload', () => renderer.cleanup());"
            << "    "
            << "  } catch (error) {"
            << "    console.error('Rendering error:', error);"
            << "    document.getElementById('progress').textContent = 'Error: ' + error.message;"
            << "  }"
            << "}"
            << "// Start rendering ketika page loaded"
            << "window.addEventListener('load', startRendering);"
            << "</script>";

        html << "<div class='footer'>"
            << "Visit \"" << EscapeHTML(world.name) << "\" in " << EscapeHTML(this->serverName)
            << "</div>"
            << "</div>"
            << "</body>"
            << "</html>";

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        std::string result = html.str();
        std::cout << "[HTML] BINARY-OPTIMIZED HTML generation completed. Size: " << result.size()
            << " bytes, Time: " << duration.count() << "ms" << std::endl;

        // Update render stats
        lastRenderStats.htmlGenerationTime = duration.count();
        lastRenderStats.uniqueTextures = itemBinaryCache.size();
        lastRenderStats.totalBlocks = world.blocks.size();
        lastRenderStats.totalObjects = world.objects.size();
        lastRenderStats.htmlFileSize = result.size();

        return result;

    }
    catch (const std::exception& e) {
        std::cout << "[HTML] ERROR in GenerateWorldHTML: " << e.what() << std::endl;
        return "<html><body><h1>Error generating HTML</h1></body></html>";
    }
}

void AdvancedWorldRenderer::RenderOptimizedBackgroundLayer(std::stringstream& html, const AdvancedWorld& world,
    const std::map<int, std::string>& itemCssVars) {
    html << "<div class='layer' id='background-layer'>";
    int bgCount = 0;

    for (int i = 0; i < world.blocks.size(); i++) {
        const AdvancedBlock& block = world.blocks[i];
        if (block.background != 0) {
            auto it = itemCssVars.find(block.background);
            if (it != itemCssVars.end() && it->second != "none") {
                int x = (i % world.width) * 32;
                int y = (i / world.width) * 32;

                ItemData item = itemManager.GetItem(block.background);
                auto offset = textureManager.GetOffset(world, block.background, i, item.m_spread_type, true);
                int spriteX = (item.m_texture_x + offset.first) * 32;
                int spriteY = (item.m_texture_y + offset.second) * 32;

                html << "<div class='block background-block' style='"
                    << "left:" << x << "px;top:" << y << "px;"
                    << "background-image:" << it->second << ";"
                    << "background-position:-" << spriteX << "px -" << spriteY << "px;'"
                    << "></div>";
                bgCount++;
            }
        }
    }
    html << "</div>";
    std::cout << "[HTML] Background layer: " << bgCount << " blocks" << std::endl;
}

void AdvancedWorldRenderer::RenderOptimizedShadowLayer(std::stringstream& html, const AdvancedWorld& world,
    const std::map<int, std::string>& itemCssVars,
    const std::map<std::string, std::string>& specialCssVars) {
    html << "<div class='layer' id='block-shadow-layer'>";
    int shadowCount = 0;

    for (int i = 0; i < world.blocks.size(); i++) {
        const AdvancedBlock& block = world.blocks[i];
        if (block.foreground != 0) {
            auto it = itemCssVars.find(block.foreground);
            if (it != itemCssVars.end() && it->second != "none") {
                int x = (i % world.width) * 32;
                int y = (i / world.width) * 32;

                ItemData item = itemManager.GetItem(block.foreground);
                auto offset = textureManager.GetOffset(world, block.foreground, i, item.m_spread_type, false);
                int spriteX = (item.m_texture_x + offset.first) * 32;
                int spriteY = (item.m_texture_y + offset.second) * 32;

                std::string flipStyle = (block.flags & 0x00200000) ? "transform:scaleX(-1);" : "";

                html << "<div class='block shadow-block' style='"
                    << "left:" << (x - 4) << "px;top:" << (y + 4) << "px;"
                    << "background-image:" << it->second << ";"
                    << "background-position:-" << spriteX << "px -" << spriteY << "px;"
                    << flipStyle << "'"
                    << "></div>";
                shadowCount++;
            }
        }
    }
    html << "</div>";
    std::cout << "[HTML] Block shadow layer: " << shadowCount << " blocks" << std::endl;

    html << "<div class='layer' id='object-shadow-layer'>";
    int objShadowCount = 0;
    for (const auto& obj : world.objects) {
        if (obj.itemid != 0) {
            RenderOptimizedObjectShadow(html, obj, itemCssVars, specialCssVars);
            objShadowCount++;
        }
    }
    html << "</div>";
    std::cout << "[HTML] Object shadow layer: " << objShadowCount << " objects" << std::endl;
}

void AdvancedWorldRenderer::RenderOptimizedForegroundLayer(std::stringstream& html, const AdvancedWorld& world,
    const std::map<int, std::string>& itemCssVars) {
    html << "<div class='layer' id='foreground-layer'>";
    int fgCount = 0;

    for (int i = 0; i < world.blocks.size(); i++) {
        const AdvancedBlock& block = world.blocks[i];
        if (block.foreground != 0) {
            auto it = itemCssVars.find(block.foreground);
            if (it != itemCssVars.end() && it->second != "none") {
                int x = (i % world.width) * 32;
                int y = (i / world.width) * 32;

                ItemData item = itemManager.GetItem(block.foreground);
                auto offset = textureManager.GetOffset(world, block.foreground, i, item.m_spread_type, false);
                int spriteX = (item.m_texture_x + offset.first) * 32;
                int spriteY = (item.m_texture_y + offset.second) * 32;

                std::string flipStyle = (block.flags & 0x00200000) ? "transform:scaleX(-1);" : "";
                std::string filterStyle = ApplyColorFilter(block.flags, block.foreground);
                std::string rainbowClass = (block.foreground == 2590) ? "rainbow-block " : "";

                html << "<div class='block foreground-block " << rainbowClass << "' style='"
                    << "left:" << x << "px;top:" << y << "px;"
                    << "background-image:" << it->second << ";"
                    << "background-position:-" << spriteX << "px -" << spriteY << "px;"
                    << flipStyle << filterStyle << "'"
                    << "></div>";
                fgCount++;
            }
        }
    }
    html << "</div>";
    std::cout << "[HTML] Foreground layer: " << fgCount << " blocks" << std::endl;
}

void AdvancedWorldRenderer::RenderOptimizedObjectsLayer(std::stringstream& html, const AdvancedWorld& world,
    const std::map<int, std::string>& itemCssVars,
    const std::map<std::string, std::string>& specialCssVars) {
    html << "<div class='layer' id='objects-layer'>";
    int objCount = 0;

    for (const auto& obj : world.objects) {
        if (obj.itemid != 0) {
            if (obj.itemid % 2 == 0) {
                RenderOptimizedRegularObject(html, obj, itemCssVars);
            }
            else {
                RenderOptimizedSeedObject(html, obj, specialCssVars);
            }
            objCount++;
        }
    }
    html << "</div>";
    std::cout << "[HTML] Objects layer: " << objCount << " objects" << std::endl;
}

void AdvancedWorldRenderer::RenderOptimizedEffectsLayer(std::stringstream& html, const AdvancedWorld& world,
    const std::map<std::string, std::string>& specialCssVars) {
    html << "<div class='layer' id='effects-layer'>";
    int effectCount = 0;

    for (int i = 0; i < world.blocks.size(); i++) {
        const AdvancedBlock& block = world.blocks[i];
        if (block.flags) {
            int x = (i % world.width) * 32;
            int y = (i / world.width) * 32;

            if (block.flags & 0x04000000) {
                auto it = specialCssVars.find("water");
                if (it != specialCssVars.end() && it->second != "none") {
                    html << "<div class='water-overlay' style='"
                        << "left:" << x << "px;top:" << y << "px;"
                        << "background-image:" << it->second << ";'"
                        << "></div>";
                    effectCount++;
                }
            }

            if (block.flags & 0x10000000) {
                auto it = specialCssVars.find("fire");
                if (it != specialCssVars.end() && it->second != "none") {
                    html << "<div class='fire-overlay' style='"
                        << "left:" << x << "px;top:" << y << "px;"
                        << "background-image:" << it->second << ";'"
                        << "></div>";
                    effectCount++;
                }
            }
        }
    }
    html << "</div>";
    std::cout << "[HTML] Effects layer: " << effectCount << " blocks" << std::endl;
}

void AdvancedWorldRenderer::RenderOptimizedRegularObject(std::stringstream& html, const AdvancedWorldObject& obj,
    const std::map<int, std::string>& itemCssVars) {
    auto it = itemCssVars.find(obj.itemid);
    if (it != itemCssVars.end() && it->second != "none") {
        html << "<div class='object' style='"
            << "left:" << obj.x << "px;top:" << obj.y << "px;width:19px;height:19px;"
            << "background-image:" << it->second << ";'"
            << "></div>";

        if (obj.count > 1 && obj.itemid != 112) {
            html << "<div class='item-count' style='"
                << "left:" << (obj.x + 8) << "px;top:" << (obj.y + 12) << "px;'>"
                << obj.count << "</div>";
        }
    }
}

void AdvancedWorldRenderer::RenderOptimizedSeedObject(std::stringstream& html, const AdvancedWorldObject& obj,
    const std::map<std::string, std::string>& specialCssVars) {
    auto it = specialCssVars.find("seed");
    if (it != specialCssVars.end() && it->second != "none") {
        ItemData item = itemManager.GetItem(obj.itemid);
        std::string baseColorStyle = "";
        if (item.m_seed_color != 0) {
            int hue = (item.m_seed_color & 0xFF);
            baseColorStyle = "filter: hue-rotate(" + std::to_string(hue) + "deg) saturate(2);";
        }

        html << "<div class='object' style='"
            << "left:" << obj.x << "px;top:" << obj.y << "px;width:16px;height:16px;"
            << "background-image:" << it->second << ";"
            << baseColorStyle << "'"
            << "></div>";
    }
}

void AdvancedWorldRenderer::RenderOptimizedObjectShadow(std::stringstream& html, const AdvancedWorldObject& obj,
    const std::map<int, std::string>& itemCssVars,
    const std::map<std::string, std::string>& specialCssVars) {
    if (obj.itemid % 2 == 0) {
        auto it = itemCssVars.find(obj.itemid);
        if (it != itemCssVars.end() && it->second != "none") {
            html << "<div class='object object-shadow' style='"
                << "left:" << (obj.x - 4) << "px;top:" << (obj.y + 4) << "px;width:19px;height:19px;"
                << "background-image:" << it->second << ";'"
                << "></div>";
        }
    }
    else {
        auto it = specialCssVars.find("seed");
        if (it != specialCssVars.end() && it->second != "none") {
            html << "<div class='object object-shadow' style='"
                << "left:" << (obj.x - 4) << "px;top:" << (obj.y + 4) << "px;width:16px;height:16px;"
                << "background-image:" << it->second << ";'"
                << "></div>";
        }
    }
}

bool AdvancedWorldRenderer::RenderWorldToHTML(const AdvancedWorld& world, const std::string& outputPath) {
    try {
        std::cout << "[Renderer] Starting HTML generation for world: " << world.name << std::endl;
        std::cout << "[Renderer] Output path: " << outputPath << std::endl;
        if (!world.loaded) {
            std::cout << "[Renderer] ERROR: World is not loaded properly" << std::endl;
            return false;
        }

        std::string outputDir = outputPath.substr(0, outputPath.find_last_of("/\\"));
        if (!fs::exists(outputDir)) {
            if (!fs::create_directories(outputDir)) {
                std::cout << "[Renderer] ERROR: Failed to create output directory: " << outputDir << std::endl;
                return false;
            }
            std::cout << "[Renderer] Created output directory: " << outputDir << std::endl;
        }

        std::ofstream file(outputPath);
        if (!file.is_open()) {
            std::cout << "[Renderer] ERROR: Cannot open output file: " << outputPath << std::endl;
            return false;
        }

        std::cout << "[Renderer] Generating ULTRA-OPTIMIZED HTML content..." << std::endl;
        std::string htmlContent = GenerateWorldHTML(world);

        if (htmlContent.empty()) {
            std::cout << "[Renderer] ERROR: Generated HTML content is empty!" << std::endl;
            return false;
        }

        std::cout << "[Renderer] Writing HTML to file..." << std::endl;
        file << htmlContent;
        file.close();

        std::cout << "[Renderer] Successfully rendered world to: " << outputPath << std::endl;
        std::cout << "[Renderer] HTML file size: " << htmlContent.size() << " bytes" << std::endl;

        return true;
    }
    catch (const std::exception& e) {
        std::cout << "[Renderer] ERROR in RenderWorldToHTML: " << e.what() << std::endl;
        return false;
    }
}

bool AdvancedWorldRenderer::RenderWorldToPNG(const AdvancedWorld& world, const std::string& outputPath) {
    try {
        std::cout << "[Renderer] Starting PNG generation for world: " << world.name << std::endl;

        if (!world.loaded) {
            std::cout << "[Renderer] ERROR: World is not loaded properly" << std::endl;
            return false;
        }

        auto startTime = std::chrono::high_resolution_clock::now();

        std::cout << "[Renderer] Generating HTML content for PNG conversion..." << std::endl;
        std::string htmlContent = GenerateWorldHTML(world);

        if (htmlContent.empty()) {
            std::cout << "[Renderer] ERROR: Generated HTML content is empty!" << std::endl;
            return false;
        }

        std::string outputDir = fs::path(outputPath).parent_path().string();
        if (!outputDir.empty() && !fs::exists(outputDir)) {
            fs::create_directories(outputDir);
        }

        std::string tempHtmlPath = outputDir + "/temp_render.html";
        std::ofstream htmlFile(tempHtmlPath);
        if (!htmlFile.is_open()) {
            std::cout << "[Renderer] ERROR: Cannot create temporary HTML file" << std::endl;
            return false;
        }
        htmlFile << htmlContent;
        htmlFile.close();

        std::cout << "[Renderer] Converting HTML to PNG using wkhtmltoimage..." << std::endl;
        bool success = HTMLToPNGConverter::ConvertHTMLToPNGDirect(tempHtmlPath, outputPath);

        std::remove(tempHtmlPath.c_str());

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        if (success) {
            std::cout << "[Renderer] Successfully rendered world to PNG: " << outputPath << std::endl;
            lastRenderStats.pngConversionTime = duration.count();
            lastRenderStats.totalRenderTime = lastRenderStats.htmlGenerationTime + lastRenderStats.pngConversionTime;

            if (fs::exists(outputPath)) {
                auto fileSize = fs::file_size(outputPath);
                lastRenderStats.pngFileSize = fileSize;
                std::cout << "[Renderer] PNG file size: " << fileSize << " bytes" << std::endl;
            }
        }
        else {
            std::cout << "[Renderer] ERROR: Failed to convert HTML to PNG" << std::endl;
        }

        return success;
    }
    catch (const std::exception& e) {
        std::cout << "[Renderer] ERROR in RenderWorldToPNG: " << e.what() << std::endl;
        return false;
    }
}

bool AdvancedWorldRenderer::RenderWorldToHTMLAndPNG(const AdvancedWorld& world, const std::string& baseOutputPath) {
    std::cout << "[Renderer] RenderWorldToHTMLAndPNG not implemented" << std::endl;
    return false;
}