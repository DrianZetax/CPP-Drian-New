#include "HTMLToPNGConverter.h"
#include <fstream>
#include <iostream>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif

HTMLToPNGConverter::HTMLToPNGConverter()
    : isInitialized(false) {
}

HTMLToPNGConverter::~HTMLToPNGConverter() {
    // Cleanup jika diperlukan
}

bool HTMLToPNGConverter::FindWkhtmlToImage() {
    std::vector<std::string> possiblePaths;

#ifdef _WIN32
    // Windows paths
    possiblePaths = {
        "wkhtmltoimage.exe",
        "wkhtmltoimage",
        "C:\\Program Files\\wkhtmltopdf\\bin\\wkhtmltoimage.exe",
        "C:\\wkhtmltopdf\\bin\\wkhtmltoimage.exe"
    };
#elif __APPLE__
    // macOS paths
    possiblePaths = {
        "wkhtmltoimage",
        "/usr/local/bin/wkhtmltoimage",
        "/opt/local/bin/wkhtmltoimage"
    };
#else
    // Linux paths
    possiblePaths = {
        "wkhtmltoimage",
        "/usr/bin/wkhtmltoimage",
        "/usr/local/bin/wkhtmltoimage"
    };
#endif

    for (const auto& path : possiblePaths) {
        // Test if command exists and works
        std::string testCmd = path + " --version >nul 2>&1";
#ifdef _WIN32
        testCmd = path + " --version >nul 2>&1";
#else
        testCmd = path + " --version >/dev/null 2>&1";
#endif

        if (system(testCmd.c_str()) == 0) {
            wkhtmlPath = path;
            std::cout << "[PNGConverter] Found wkhtmltoimage at: " << wkhtmlPath << std::endl;
            return true;
        }
    }

    std::cout << "[PNGConverter] ERROR: wkhtmltoimage not found. Please install wkhtmltopdf." << std::endl;
    return false;
}

bool HTMLToPNGConverter::Initialize() {
    std::cout << "[PNGConverter] Initializing HTML to PNG converter..." << std::endl;

    if (!FindWkhtmlToImage()) {
        std::cout << "[PNGConverter] ERROR: Cannot find wkhtmltoimage" << std::endl;
        return false;
    }

    isInitialized = true;
    std::cout << "[PNGConverter] Successfully initialized" << std::endl;
    return true;
}

bool HTMLToPNGConverter::ConvertHTMLToPNG(const std::string& htmlContent,
    const std::string& outputPath,
    int width,
    int height) {
    if (!isInitialized) {
        std::cout << "[PNGConverter] ERROR: Converter not initialized" << std::endl;
        return false;
    }

    // Create temporary HTML file
    std::string tempHtmlFile = "./DATA_BASE/render/temp_" + std::to_string(std::time(nullptr)) + ".html";

    try {
        std::ofstream file(tempHtmlFile);
        if (!file.is_open()) {
            std::cout << "[PNGConverter] ERROR: Cannot create temporary HTML file: " << tempHtmlFile << std::endl;
            return false;
        }

        file << htmlContent;
        file.close();

        std::cout << "[PNGConverter] Created temporary HTML file: " << tempHtmlFile << std::endl;

        // Convert the HTML file to PNG
        bool result = ConvertHTMLFileToPNG(tempHtmlFile, outputPath, width, height);

        // Cleanup temporary file
        std::remove(tempHtmlFile.c_str());

        return result;
    }
    catch (const std::exception& e) {
        std::cout << "[PNGConverter] ERROR: " << e.what() << std::endl;
        return false;
    }
}

bool HTMLToPNGConverter::ConvertHTMLFileToPNG(const std::string& htmlFilePath,
    const std::string& outputPath,
    int width,
    int height) {
    return ConvertHTMLToPNGDirect(htmlFilePath, outputPath);
}

// Static function untuk konversi langsung
bool HTMLToPNGConverter::ConvertHTMLToPNGDirect(const std::string& htmlFilePath,
    const std::string& pngFilePath) {
    // Build command
    std::string command = "wkhtmltoimage --quality 100 --disable-smart-width ";

    // Add HTML file and PNG file paths
    command += "\"" + htmlFilePath + "\" \"" + pngFilePath + "\"";

    std::cout << "[PNGConverter] Executing: " << command << std::endl;

    // Execute command
    int result = system(command.c_str());

    if (result == 0) {
        // Check if file was created
        if (fs::exists(pngFilePath)) {
            auto fileSize = fs::file_size(pngFilePath);
            std::cout << "[PNGConverter] Successfully created PNG: " << pngFilePath
                << " (" << fileSize << " bytes)" << std::endl;
            return true;
        }
        else {
            std::cout << "[PNGConverter] ERROR: PNG file was not created: " << pngFilePath << std::endl;
            return false;
        }
    }
    else {
        std::cout << "[PNGConverter] ERROR: wkhtmltoimage failed with code: " << result << std::endl;
        return false;
    }
}
