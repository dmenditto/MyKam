#pragma once

// Test helper: a unique temporary folder under build/tests/tmp/ that is removed when the test ends.

#include <atomic>
#include <filesystem>
#include <string>
#include <system_error>

class TempDir {
public:
    TempDir() {
        static std::atomic<int> counter{0};
        path_ = std::filesystem::path("build") / "tests" / "tmp" / ("t" + std::to_string(++counter));
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
        std::filesystem::create_directories(path_, ec);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    // Folder path in UTF-8 with forward slashes.
    std::string str() const { return path_.generic_string(); }
    std::string file(const std::string& name) const { return (path_ / name).generic_string(); }

private:
    std::filesystem::path path_;
};
