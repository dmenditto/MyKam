#include "doctest.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "file_io.h"
#include "slot_store.h"
#include "temp_dir.h"

namespace fs = std::filesystem;

TEST_CASE("file io: write then read returns the same text and leaves no temp file") {
    TempDir dir;
    const std::string path = dir.file("a.slots.txt");
    std::string error;
    REQUIRE(mykam::writeTextFileAtomic(path, "hello\nworld\n", error));
    CHECK(error.empty());

    const mykam::FileReadResult r = mykam::readTextFile(path, mykam::kMaxSlotFileBytes);
    CHECK(r.status == mykam::FileStatus::Ok);
    CHECK(r.text == "hello\nworld\n");
    CHECK_FALSE(fs::exists(fs::path(path + ".tmp")));
}

TEST_CASE("file io: a second write replaces the first (temp file then rename)") {
    TempDir dir;
    const std::string path = dir.file("a.txt");
    std::string error;
    REQUIRE(mykam::writeTextFileAtomic(path, "first", error));
    REQUIRE(mykam::writeTextFileAtomic(path, "second", error));
    CHECK(mykam::readTextFile(path, 100).text == "second");
    CHECK_FALSE(fs::exists(fs::path(path + ".tmp")));
}

TEST_CASE("file io: missing parent folders are created") {
    TempDir dir;
    const std::string path = dir.file("sub/deeper/a.txt");
    std::string error;
    CHECK(mykam::writeTextFileAtomic(path, "x", error));
    CHECK(mykam::readTextFile(path, 100).status == mykam::FileStatus::Ok);
}

TEST_CASE("file io: a failed write reports an error and keeps the old file") {
    TempDir dir;
    const std::string path = dir.file("keep.txt");
    std::string error;
    REQUIRE(mykam::writeTextFileAtomic(path, "original", error));

    // Make the temp file name unusable by putting a folder where it would go.
    REQUIRE(fs::create_directories(fs::path(path + ".tmp")));
    error.clear();
    CHECK_FALSE(mykam::writeTextFileAtomic(path, "new content", error));
    CHECK_FALSE(error.empty());
    CHECK(mykam::readTextFile(path, 100).text == "original");
}

TEST_CASE("file io: a path under a regular file cannot be created") {
    TempDir dir;
    std::string error;
    REQUIRE(mykam::writeTextFileAtomic(dir.file("plain.txt"), "x", error));
    CHECK_FALSE(mykam::writeTextFileAtomic(dir.file("plain.txt/child.txt"), "y", error));
    CHECK_FALSE(error.empty());
}

TEST_CASE("file io: missing, oversized and non-file paths are reported, not thrown") {
    TempDir dir;
    CHECK(mykam::readTextFile(dir.file("nope.txt"), 100).status == mykam::FileStatus::Missing);

    std::string error;
    REQUIRE(mykam::writeTextFileAtomic(dir.file("big.txt"), std::string(101, 'x'), error));
    const mykam::FileReadResult big = mykam::readTextFile(dir.file("big.txt"), 100);
    CHECK(big.status == mykam::FileStatus::TooLarge);
    CHECK(big.text.empty());
    CHECK(mykam::readTextFile(dir.file("big.txt"), 101).status == mykam::FileStatus::Ok);

    CHECK(mykam::readTextFile(dir.str(), 100).status == mykam::FileStatus::Error);  // a folder
}

TEST_CASE("file io: a saved slot file can be read back through the slot store") {
    TempDir dir;
    mykam::ViewStore a;
    mykam::Pose p;
    p.x = 3.0;
    p.heading = 90.0;
    REQUIRE(a.add(p, mykam::ViewKind::External, "", 4) != 0);
    const std::string path = dir.file(mykam::slotFileName("Cessna_172SP.acf"));
    std::string error;
    REQUIRE(mykam::writeTextFileAtomic(path, a.serialize(), error));

    mykam::ViewStore b;
    const mykam::FileReadResult r = mykam::readTextFile(path, mykam::kMaxSlotFileBytes);
    REQUIRE(r.status == mykam::FileStatus::Ok);
    CHECK(b.parse(r.text).loaded == 1);
    CHECK(b.get(4)->heading == 90.0);
}

TEST_CASE("paths: the plugin folder is found from the binary path") {
    CHECK(mykam::pluginFolderFromBinaryPath("C:\\X-Plane 12\\Resources\\plugins\\mykam\\win_x64\\mykam.xpl") ==
          "C:\\X-Plane 12\\Resources\\plugins\\mykam");
    CHECK(mykam::pluginFolderFromBinaryPath("/a/plugins/mykam/win_x64/mykam.xpl") == "/a/plugins/mykam");
    CHECK(mykam::pluginFolderFromBinaryPath("/a/plugins/mykam/64/win.xpl") == "/a/plugins/mykam");
    CHECK(mykam::pluginFolderFromBinaryPath("/a/plugins/mykam/mykam.xpl") == "/a/plugins/mykam");
    CHECK(mykam::pluginFolderFromBinaryPath("mykam.xpl").empty());
    CHECK(mykam::joinPath("/a/b", "c.txt") == "/a/b/c.txt");
    CHECK(mykam::joinPath("/a/b/", "c.txt") == "/a/b/c.txt");
    CHECK(mykam::joinPath("", "c.txt") == "c.txt");
}
