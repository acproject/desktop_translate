#include "Config.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>

namespace DesktopTranslate {

Config& Config::instance() {
    static Config config;
    return config;
}

Config::Config() {
    load();
}

std::filesystem::path Config::getConfigPath() const {
    if (const char* appData = std::getenv("APPDATA")) {
        return std::filesystem::path(appData) / "desktop_translate" / "config.json";
    }

    if (const char* xdgConfigHome = std::getenv("XDG_CONFIG_HOME")) {
        return std::filesystem::path(xdgConfigHome) / "desktop_translate" / "config.json";
    }

    const char* home = std::getenv("HOME");
    if (home) {
        return std::filesystem::path(home) / ".config" / "desktop_translate" / "config.json";
    }

    return std::filesystem::temp_directory_path() / "desktop_translate_config.json";
}

std::string Config::getApiEndpoint() const {
    return api_endpoint_;
}

std::string Config::getApiKey() const {
    return api_key_;
}

std::string Config::getModel() const {
    return model_;
}

int Config::getApiPort() const {
    return api_port_;
}

int Config::getApiTimeout() const {
    return api_timeout_;
}

std::string Config::getSourceLanguage() const {
    return source_language_;
}

std::string Config::getTargetLanguage() const {
    return target_language_;
}

int Config::getSelectionOpacity() const {
    return selection_opacity_;
}

std::string Config::getSelectionColor() const {
    return selection_color_;
}

void Config::setApiEndpoint(const std::string& endpoint) {
    api_endpoint_ = endpoint;
}

void Config::setApiPort(int port) {
    api_port_ = port;
}

void Config::setApiKey(const std::string& key) {
    api_key_ = key;
}

void Config::setModel(const std::string& model) {
    model_ = model;
}

void Config::setSourceLanguage(const std::string& lang) {
    source_language_ = lang;
}

void Config::setTargetLanguage(const std::string& lang) {
    target_language_ = lang;
}

void Config::setApiTimeout(int timeout) {
    api_timeout_ = timeout;
}

// OCR 配置方法
std::string Config::getOcrEndpoint() const {
    return ocr_endpoint_;
}

int Config::getOcrPort() const {
    return ocr_port_;
}

std::string Config::getOcrApiKey() const {
    return ocr_api_key_;
}

std::string Config::getOcrModel() const {
    return ocr_model_;
}

void Config::setOcrEndpoint(const std::string& endpoint) {
    ocr_endpoint_ = endpoint;
}

void Config::setOcrPort(int port) {
    ocr_port_ = port;
}

void Config::setOcrApiKey(const std::string& key) {
    ocr_api_key_ = key;
}

void Config::setOcrModel(const std::string& model) {
    ocr_model_ = model;
}

std::string Config::getShortcutSelectTranslate() const {
    return shortcut_select_translate_;
}

std::string Config::getShortcutClipboardTranslate() const {
    return shortcut_clipboard_translate_;
}

std::string Config::getShortcutHoverTranslationToggle() const {
    return shortcut_hover_translation_toggle_;
}

void Config::setShortcutSelectTranslate(const std::string& shortcut) {
    shortcut_select_translate_ = shortcut;
}

void Config::setShortcutClipboardTranslate(const std::string& shortcut) {
    shortcut_clipboard_translate_ = shortcut;
}

void Config::setShortcutHoverTranslationToggle(const std::string& shortcut) {
    shortcut_hover_translation_toggle_ = shortcut;
}

bool Config::load() {
    try {
        auto path = getConfigPath();
        
        // 确保目录存在
        auto dir = path.parent_path();
        if (!std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }

        if (!std::filesystem::exists(path)) {
            save(); // 创建默认配置文件
            return true;
        }

        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        file.close();

        // 简单的 JSON 解析（不依赖外部库）
        auto getJsonValue = [&content](const std::string& key) -> std::string {
            std::string searchKey = "\"" + key + "\"";
            size_t pos = content.find(searchKey);
            if (pos == std::string::npos) return "";
            
            pos = content.find(":", pos);
            if (pos == std::string::npos) return "";
            
            pos = content.find("\"", pos);
            if (pos == std::string::npos) return "";
            
            size_t end = content.find("\"", pos + 1);
            if (end == std::string::npos) return "";
            
            return content.substr(pos + 1, end - pos - 1);
        };

        auto getIntJsonValue = [&content](const std::string& key) -> int {
            std::string searchKey = "\"" + key + "\"";
            size_t pos = content.find(searchKey);
            if (pos == std::string::npos) return 0;
            
            pos = content.find(":", pos);
            if (pos == std::string::npos) return 0;
            
            while (pos < content.size() && (content[pos] < '0' || content[pos] > '9')) {
                pos++;
            }
            
            size_t end = pos;
            while (end < content.size() && content[end] >= '0' && content[end] <= '9') {
                end++;
            }
            
            if (pos >= content.size()) return 0;
            return std::stoi(content.substr(pos, end - pos));
        };

        api_endpoint_ = getJsonValue("api_endpoint");
        if (api_endpoint_.empty()) api_endpoint_ = "http://127.0.0.1";

        api_key_ = getJsonValue("api_key");
        model_ = getJsonValue("model");
        if (model_.empty()) model_ = "HY-MT1.5-1.8B-Q8_0";

        source_language_ = getJsonValue("source_language");
        if (source_language_.empty()) source_language_ = "auto";

        target_language_ = getJsonValue("target_language");
        if (target_language_.empty()) target_language_ = "zh";

        selection_color_ = getJsonValue("selection_color");
        if (selection_color_.empty()) selection_color_ = "#4A90D9";

        api_port_ = getIntJsonValue("api_port");
        if (api_port_ == 0) api_port_ = 8110;

        api_timeout_ = getIntJsonValue("api_timeout");
        if (api_timeout_ == 0) api_timeout_ = 60;

        selection_opacity_ = getIntJsonValue("selection_opacity");
        if (selection_opacity_ == 0) selection_opacity_ = 50;

        // 快捷键配置
        shortcut_select_translate_ = getJsonValue("shortcut_select_translate");
        if (shortcut_select_translate_.empty()) shortcut_select_translate_ = "Ctrl+F3";

        shortcut_clipboard_translate_ = getJsonValue("shortcut_clipboard_translate");
        if (shortcut_clipboard_translate_.empty()) shortcut_clipboard_translate_ = "Ctrl+F4";

        shortcut_hover_translation_toggle_ = getJsonValue("shortcut_hover_translation_toggle");
        if (shortcut_hover_translation_toggle_.empty()) shortcut_hover_translation_toggle_ = "Ctrl+F8";

        // OCR 配置
        ocr_endpoint_ = getJsonValue("ocr_endpoint");
        if (ocr_endpoint_.empty()) ocr_endpoint_ = "http://127.0.0.1";

        ocr_port_ = getIntJsonValue("ocr_port");
        if (ocr_port_ == 0) ocr_port_ = 8111;

        ocr_api_key_ = getJsonValue("ocr_api_key");

        ocr_model_ = getJsonValue("ocr_model");
        if (ocr_model_.empty()) ocr_model_ = "PaddleOCR-VL-1.5-GGUF";
        
        std::cout << "Config loaded successfully:" << std::endl;
        std::cout << "  API Port: " << api_port_ << std::endl;
        std::cout << "  API Timeout: " << api_timeout_ << std::endl;
        std::cout << "  OCR Port: " << ocr_port_ << std::endl;

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load config: " << e.what() << std::endl;
        return false;
    }
}

bool Config::save() {
    try {
        auto path = getConfigPath();
        auto dir = path.parent_path();
        
        if (!std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }

        std::ofstream file(path);
        if (!file.is_open()) {
            return false;
        }

        file << "{\n";
        file << "  \"api_endpoint\": \"" << api_endpoint_ << "\",\n";
        file << "  \"api_port\": " << api_port_ << ",\n";
        file << "  \"api_key\": \"" << api_key_ << "\",\n";
        file << "  \"model\": \"" << model_ << "\",\n";
        file << "  \"api_timeout\": " << api_timeout_ << ",\n";
        file << "  \"source_language\": \"" << source_language_ << "\",\n";
        file << "  \"target_language\": \"" << target_language_ << "\",\n";
        file << "  \"selection_opacity\": " << selection_opacity_ << ",\n";
        file << "  \"selection_color\": \"" << selection_color_ << "\",\n";
        file << "  \"shortcut_select_translate\": \"" << shortcut_select_translate_ << "\",\n";
        file << "  \"shortcut_clipboard_translate\": \"" << shortcut_clipboard_translate_ << "\",\n";
        file << "  \"shortcut_hover_translation_toggle\": \"" << shortcut_hover_translation_toggle_ << "\",\n";
        file << "  \"ocr_endpoint\": \"" << ocr_endpoint_ << "\",\n";
        file << "  \"ocr_port\": " << ocr_port_ << ",\n";
        file << "  \"ocr_api_key\": \"" << ocr_api_key_ << "\",\n";
        file << "  \"ocr_model\": \"" << ocr_model_ << "\"\n";
        file << "}\n";

        file.close();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to save config: " << e.what() << std::endl;
        return false;
    }
}

} // namespace DesktopTranslate
