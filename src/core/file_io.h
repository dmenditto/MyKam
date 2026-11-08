#pragma once

#include <cstddef>
#include <string>

// Plain file access used by the plugin adapter. Pure C++ (no X-Plane calls) so it is unit tested.
// Paths are UTF-8 strings. Nothing here throws.

namespace mykam {

enum class FileStatus {
    Ok,
    Missing,   // the file does not exist
    TooLarge,  // the file is bigger than the allowed size and was not read (NFR2.6)
    Error      // exists but could not be read, or is not a regular file
};

struct FileReadResult {
    FileStatus status = FileStatus::Error;
    std::string text;
};

// Read a whole file as bytes. Files larger than maxBytes are not read.
FileReadResult readTextFile(const std::string& path, std::size_t maxBytes);

// Write text so that a crash or failure part-way never leaves a half-written file at `path`:
// write `path` + ".tmp", then rename it over `path`. Missing parent folders are created.
// On failure returns false, fills `error` and leaves any previous file untouched.
bool writeTextFileAtomic(const std::string& path, const std::string& text, std::string& error);

// Join two path parts with a single '/'.
std::string joinPath(const std::string& folder, const std::string& name);

// The folder part of a full file path ("C:\X-Plane 12\Aircraft\A320\a320.acf" gives
// "C:\X-Plane 12\Aircraft\A320"). Empty when the path has no folder part.
std::string folderOfPath(const std::string& filePath);

// The plugin folder from the full path of the loaded binary, e.g.
// ".../plugins/MyKam/win_x64/MyKam.xpl" gives ".../plugins/MyKam".
std::string pluginFolderFromBinaryPath(const std::string& binaryPath);

}  // namespace mykam
