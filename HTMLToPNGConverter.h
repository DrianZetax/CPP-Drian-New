#pragma once

#include <string>
#include <filesystem>

namespace fs = std::filesystem;

class HTMLToPNGConverter {
private:
    std::string wkhtmlPath;
    bool isInitialized;

    bool FindWkhtmlToImage();

public:
    HTMLToPNGConverter();
    ~HTMLToPNGConverter();

    bool Initialize();
    bool ConvertHTMLToPNG(const std::string& htmlContent,
        const std::string& outputPath,
        int width = 0,
        int height = 0);
    bool ConvertHTMLFileToPNG(const std::string& htmlFilePath,
        const std::string& outputPath,
        int width = 0,
        int height = 0);

    std::string GetWkhtmlPath() const { return wkhtmlPath; }
    bool IsInitialized() const { return isInitialized; }

    // Static function untuk konversi langsung
    static bool ConvertHTMLToPNGDirect(const std::string& htmlFilePath,
        const std::string& pngFilePath);
};