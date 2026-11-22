#include "utils.hpp"

std::string Utils::narrow(const wchar_t* str) {
    if (!str) {
        return "";
    }

    std::string result;
    size_t len = wcslen(str);
    
    if (len == 0) {
        return result;
    }
    
    result.reserve(len);

    for (size_t i = 0; i < len; ++i) {
        if (str[i] > 0x7F) {
            return "";
        }
        result.push_back(static_cast<char>(str[i]));
    }

    return result;

}

std::wstring Utils::widen(const char* str) {

    std::wstring result;
    result.reserve(strlen(str));

    for (size_t i = 0; i < strlen(str); ++i) {
        result.push_back(static_cast<wchar_t>(str[i]));
    }

    return result;

}

std::string Utils::toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}

std::time_t Utils::getFileCreationTime(const std::filesystem::path& path) {
    std::time_t ret;
    return ret;
}

std::string Utils::formatTime(std::time_t time) {
    std::tm tm = *std::localtime(&time);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

int Utils::copyFile(const std::string& sourcePath, const std::string& destinationPath) {
    std::ifstream source(sourcePath, std::ios::binary);
    std::ofstream destination(destinationPath, std::ios::binary);

    if (!source)
        return 1;

    if (!destination)
        return 2;

    destination << source.rdbuf();

    return 0;
}

std::vector<std::string> Utils::splitByChar(std::string str, char splitChar) {
    std::vector<std::string> strs;
    strs.reserve(std::count(str.begin(), str.end(), splitChar) + 1);

    size_t start = 0;
    size_t end = str.find(splitChar);
    while (end != std::string::npos) {
        strs.emplace_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find(splitChar, start);
    }
    strs.emplace_back(str.substr(start));

    return strs;
}

std::string Utils::getTexture() {
    cocos2d::ccColor3B color = Mod::get()->getSettingValue<cocos2d::ccColor3B>("background_color");
    
	std::string texture = color == ccc3(51, 68, 153) ? "GJ_square02.png" : "GJ_square06.png";

    return texture;
}

std::string Utils::getSimplifiedString(std::string str) {
    if (str.find(".") == std::string::npos) return str;

    while(str.back() == '0') {
        str.pop_back();
        if (str.empty()) break;
    }

    if (!str.empty())
        if (str.back() == '.') str.pop_back();

    return str;
}

void Utils::setBackgroundColor(geode::NineSlice* bg) {
    cocos2d::ccColor3B color = Mod::get()->getSettingValue<cocos2d::ccColor3B>("background_color");

	if (color == ccc3(51, 68, 153))
		color = ccc3(255, 255, 255);

	bg->setColor(color);
}