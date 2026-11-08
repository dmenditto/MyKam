#include "file_io.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace mykam {

namespace {

fs::path toPath(const std::string& utf8) {
    return fs::u8path(utf8);
}

bool equalsIgnoreCase(const std::string& a, const char* b) {
    std::size_t i = 0;
    for (; i < a.size() && b[i] != '\0'; ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return i == a.size() && b[i] == '\0';
}

}  // namespace

FileReadResult readTextFile(const std::string& path, std::size_t maxBytes) {
    FileReadResult result;
    try {
        const fs::path p = toPath(path);
        std::error_code ec;
        const fs::file_status st = fs::status(p, ec);
        if (ec || st.type() == fs::file_type::not_found) {
            result.status = FileStatus::Missing;
            return result;
        }
        if (st.type() != fs::file_type::regular) {
            result.status = FileStatus::Error;
            return result;
        }
        const std::uintmax_t size = fs::file_size(p, ec);
        if (ec) {
            result.status = FileStatus::Error;
            return result;
        }
        if (size > maxBytes) {
            result.status = FileStatus::TooLarge;
            return result;
        }
        std::ifstream in(p, std::ios::binary);
        if (!in) {
            result.status = FileStatus::Error;
            return result;
        }
        std::string text(static_cast<std::size_t>(size), '\0');
        if (size > 0) {
            in.read(&text[0], static_cast<std::streamsize>(size));
            if (in.gcount() != static_cast<std::streamsize>(size)) {
                result.status = FileStatus::Error;
                return result;
            }
        }
        result.text = std::move(text);
        result.status = FileStatus::Ok;
    } catch (...) {
        result.text.clear();
        result.status = FileStatus::Error;
    }
    return result;
}

bool writeTextFileAtomic(const std::string& path, const std::string& text, std::string& error) {
    try {
        const fs::path target = toPath(path);
        std::error_code ec;
        if (target.has_parent_path()) {
            fs::create_directories(target.parent_path(), ec);
            if (ec) {
                error = "cannot create folder: " + ec.message();
                return false;
            }
        }
        fs::path temp = target;
        temp += ".tmp";
        {
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            if (!out) {
                error = "cannot open temporary file for writing";
                return false;
            }
            out.write(text.data(), static_cast<std::streamsize>(text.size()));
            out.flush();
            if (!out) {
                out.close();
                fs::remove(temp, ec);
                error = "write to temporary file failed";
                return false;
            }
        }
        fs::rename(temp, target, ec);
        if (ec) {
            std::error_code ignored;
            fs::remove(temp, ignored);
            error = "cannot replace file: " + ec.message();
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    } catch (...) {
        error = "unexpected error";
        return false;
    }
}

std::string joinPath(const std::string& folder, const std::string& name) {
    if (folder.empty()) {
        return name;
    }
    const char last = folder.back();
    if (last == '/' || last == '\\') {
        return folder + name;
    }
    return folder + "/" + name;
}

std::string folderOfPath(const std::string& filePath) {
    const std::size_t cut = filePath.find_last_of("/\\");
    if (cut == std::string::npos) {
        return std::string();
    }
    return filePath.substr(0, cut);
}

std::string pluginFolderFromBinaryPath(const std::string& binaryPath) {
    std::string path = binaryPath;
    auto stripLastComponent = [](std::string& p) -> std::string {
        const std::size_t cut = p.find_last_of("/\\");
        if (cut == std::string::npos) {
            std::string last = p;
            p.clear();
            return last;
        }
        std::string last = p.substr(cut + 1);
        p.erase(cut);
        return last;
    };
    stripLastComponent(path);  // the binary's file name
    std::string folderName = path;
    const std::size_t cut = folderName.find_last_of("/\\");
    if (cut != std::string::npos) {
        folderName = folderName.substr(cut + 1);
    }
    if (equalsIgnoreCase(folderName, "win_x64") || equalsIgnoreCase(folderName, "64")) {
        stripLastComponent(path);
    }
    return path;
}

}  // namespace mykam
