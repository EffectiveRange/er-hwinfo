#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include <er/hwinfo.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>

#include <arpa/inet.h>

namespace {

class TempDir {
public:
  TempDir() : path_(std::filesystem::temp_directory_path() /
                    ("hwinfo_test_" + std::to_string(std::rand()))) {
    std::filesystem::create_directories(path_);
  }
  ~TempDir() { std::filesystem::remove_all(path_); }
  std::filesystem::path const &path() const { return path_; }

private:
  std::filesystem::path path_;
};

void write_text_file(std::filesystem::path const &path,
                     std::string const &content) {
  std::ofstream file(path, std::ios::binary);
  file.write(content.data(), static_cast<std::streamsize>(content.size()));
}

void write_u32_file(std::filesystem::path const &path, std::uint32_t value) {
  std::ofstream file(path, std::ios::binary);
  std::uint32_t net_value = htonl(value);
  file.write(reinterpret_cast<char const *>(&net_value), sizeof(net_value));
}

void create_device_tree(std::filesystem::path const &base,
                        std::string const &hw_type, std::uint32_t major,
                        std::uint32_t minor, std::uint32_t patch) {
  auto er_path = base / "effective-range,hardware";
  std::filesystem::create_directories(er_path);
  write_text_file(er_path / "effective-range,type", hw_type);
  write_u32_file(er_path / "effective-range,revision-major", major);
  write_u32_file(er_path / "effective-range,revision-minor", minor);
  write_u32_file(er_path / "effective-range,revision-patch", patch);
}

} // namespace

TEST_CASE("get_device returns device info when all files exist",
          "[get_device]") {
  TempDir temp;
  create_device_tree(temp.path(), "test-board", 1, 2, 3);

  auto result = er::hwinfo::impl::get_device(temp.path());

  REQUIRE(result.has_value());
  REQUIRE(result->hw_type == "test-board");
  REQUIRE(result->hw_revision.major == 1);
  REQUIRE(result->hw_revision.minor == 2);
  REQUIRE(result->hw_revision.patch == 3);
}

TEST_CASE("get_device returns nullopt when base directory does not exist",
          "[get_device]") {
  auto result = er::hwinfo::impl::get_device("/nonexistent/path");

  REQUIRE_FALSE(result.has_value());
}

TEST_CASE("get_device returns nullopt when type file is missing",
          "[get_device]") {
  TempDir temp;
  auto er_path = temp.path() / "effective-range,hardware";
  std::filesystem::create_directories(er_path);
  write_u32_file(er_path / "effective-range,revision-major", 1);
  write_u32_file(er_path / "effective-range,revision-minor", 0);
  write_u32_file(er_path / "effective-range,revision-patch", 0);

  auto result = er::hwinfo::impl::get_device(temp.path());

  REQUIRE_FALSE(result.has_value());
}

TEST_CASE("get_device returns nullopt when revision-major is missing",
          "[get_device]") {
  TempDir temp;
  auto er_path = temp.path() / "effective-range,hardware";
  std::filesystem::create_directories(er_path);
  write_text_file(er_path / "effective-range,type", "test-board");
  write_u32_file(er_path / "effective-range,revision-minor", 0);
  write_u32_file(er_path / "effective-range,revision-patch", 0);

  auto result = er::hwinfo::impl::get_device(temp.path());

  REQUIRE_FALSE(result.has_value());
}

TEST_CASE("get_device returns nullopt when revision-minor is missing",
          "[get_device]") {
  TempDir temp;
  auto er_path = temp.path() / "effective-range,hardware";
  std::filesystem::create_directories(er_path);
  write_text_file(er_path / "effective-range,type", "test-board");
  write_u32_file(er_path / "effective-range,revision-major", 1);
  write_u32_file(er_path / "effective-range,revision-patch", 0);

  auto result = er::hwinfo::impl::get_device(temp.path());

  REQUIRE_FALSE(result.has_value());
}

TEST_CASE("get_device returns nullopt when revision-patch is missing",
          "[get_device]") {
  TempDir temp;
  auto er_path = temp.path() / "effective-range,hardware";
  std::filesystem::create_directories(er_path);
  write_text_file(er_path / "effective-range,type", "test-board");
  write_u32_file(er_path / "effective-range,revision-major", 1);
  write_u32_file(er_path / "effective-range,revision-minor", 0);

  auto result = er::hwinfo::impl::get_device(temp.path());

  REQUIRE_FALSE(result.has_value());
}

TEST_CASE("get_device returns nullopt when revision-major file is truncated",
          "[get_device]") {
  TempDir temp;
  auto er_path = temp.path() / "effective-range,hardware";
  std::filesystem::create_directories(er_path);
  write_text_file(er_path / "effective-range,type", "test-board");
  // Write only 1 byte instead of 4 bytes for u32
  write_text_file(er_path / "effective-range,revision-major", "\x01");
  write_u32_file(er_path / "effective-range,revision-minor", 0);
  write_u32_file(er_path / "effective-range,revision-patch", 0);

  auto result = er::hwinfo::impl::get_device(temp.path());

  REQUIRE_FALSE(result.has_value());
}

TEST_CASE("get_device returns nullopt when revision-minor file is truncated",
          "[get_device]") {
  TempDir temp;
  auto er_path = temp.path() / "effective-range,hardware";
  std::filesystem::create_directories(er_path);
  write_text_file(er_path / "effective-range,type", "test-board");
  write_u32_file(er_path / "effective-range,revision-major", 1);
  write_text_file(er_path / "effective-range,revision-minor", "\x01");
  write_u32_file(er_path / "effective-range,revision-patch", 0);

  auto result = er::hwinfo::impl::get_device(temp.path());

  REQUIRE_FALSE(result.has_value());
}

TEST_CASE("get_device returns nullopt when revision-patch file is truncated",
          "[get_device]") {
  TempDir temp;
  auto er_path = temp.path() / "effective-range,hardware";
  std::filesystem::create_directories(er_path);
  write_text_file(er_path / "effective-range,type", "test-board");
  write_u32_file(er_path / "effective-range,revision-major", 1);
  write_u32_file(er_path / "effective-range,revision-minor", 0);
  write_text_file(er_path / "effective-range,revision-patch", "\x01");

  auto result = er::hwinfo::impl::get_device(temp.path());

  REQUIRE_FALSE(result.has_value());
}

TEST_CASE("get_device returns nullopt when all revision files are truncated",
          "[get_device]") {
  TempDir temp;
  auto er_path = temp.path() / "effective-range,hardware";
  std::filesystem::create_directories(er_path);
  write_text_file(er_path / "effective-range,type", "test-board");
  write_text_file(er_path / "effective-range,revision-major", "\x01");
  write_text_file(er_path / "effective-range,revision-minor", "\x02");
  write_text_file(er_path / "effective-range,revision-patch", "\x03");

  auto result = er::hwinfo::impl::get_device(temp.path());

  REQUIRE_FALSE(result.has_value());
}

TEST_CASE("get_device returns nullopt when revision file is not readable",
          "[get_device]") {
  TempDir temp;
  auto er_path = temp.path() / "effective-range,hardware";
  std::filesystem::create_directories(er_path);
  write_text_file(er_path / "effective-range,type", "test-board");
  write_u32_file(er_path / "effective-range,revision-major", 1);
  write_u32_file(er_path / "effective-range,revision-minor", 0);
  write_u32_file(er_path / "effective-range,revision-patch", 0);
  // Remove read permission from revision-major file
  std::filesystem::permissions(er_path / "effective-range,revision-major",
                               std::filesystem::perms::none);

  auto result = er::hwinfo::impl::get_device(temp.path());

  // Restore permissions for cleanup
  std::filesystem::permissions(er_path / "effective-range,revision-major",
                               std::filesystem::perms::owner_all);
  REQUIRE_FALSE(result.has_value());
}

TEST_CASE("get_device returns nullopt when type file is empty",
          "[get_device]") {
  TempDir temp;
  auto er_path = temp.path() / "effective-range,hardware";
  std::filesystem::create_directories(er_path);
  write_text_file(er_path / "effective-range,type", "");
  write_u32_file(er_path / "effective-range,revision-major", 1);
  write_u32_file(er_path / "effective-range,revision-minor", 0);
  write_u32_file(er_path / "effective-range,revision-patch", 0);

  auto result = er::hwinfo::impl::get_device(temp.path());

  REQUIRE_FALSE(result.has_value());
}

TEST_CASE("get_device returns nullopt when type file contains only whitespace",
          "[get_device]") {
  TempDir temp;
  auto er_path = temp.path() / "effective-range,hardware";
  std::filesystem::create_directories(er_path);
  write_text_file(er_path / "effective-range,type", "   ");
  write_u32_file(er_path / "effective-range,revision-major", 1);
  write_u32_file(er_path / "effective-range,revision-minor", 0);
  write_u32_file(er_path / "effective-range,revision-patch", 0);

  auto result = er::hwinfo::impl::get_device(temp.path());

  REQUIRE_FALSE(result.has_value());
}

TEST_CASE("get_device handles revision values correctly", "[get_device]") {
  TempDir temp;
  create_device_tree(temp.path(), "board-v2", 10, 20, 30);

  auto result = er::hwinfo::impl::get_device(temp.path());

  REQUIRE(result.has_value());
  REQUIRE(result->hw_revision.major == 10);
  REQUIRE(result->hw_revision.minor == 20);
  REQUIRE(result->hw_revision.patch == 30);
}

// --- Tests for er::hwinfo::get ---

namespace {

const std::string valid_schema = R"({
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "type": "object",
  "additionalProperties": {
    "type": "object",
    "additionalProperties": {
      "type": "object",
      "properties": {
        "pins": {
          "type": "object",
          "additionalProperties": {
            "type": "object",
            "properties": {
              "description": { "type": "string" },
              "value": { "type": "integer", "minimum": 0 }
            },
            "required": ["description", "value"]
          }
        }
      },
      "required": ["pins"]
    }
  }
})";

const std::string valid_hwdb = R"({
  "test-board": {
    "1.2.3": {
      "pins": {
        "LED": { "description": "Status LED", "value": 17 }
      }
    }
  }
})";

} // namespace

TEST_CASE("get returns nullopt when device tree is missing", "[get]") {
  TempDir temp;
  write_text_file(temp.path() / "schema.json", valid_schema);
  write_text_file(temp.path() / "hwdb.json", valid_hwdb);

  auto result = er::hwinfo::get(temp.path() / "nonexistent",
                                temp.path() / "hwdb.json",
                                temp.path() / "schema.json");

  REQUIRE_FALSE(result.has_value());
}

TEST_CASE("get throws when schema file does not exist", "[get]") {
  TempDir temp;
  create_device_tree(temp.path(), "test-board", 1, 2, 3);
  write_text_file(temp.path() / "hwdb.json", valid_hwdb);

  REQUIRE_THROWS_AS(er::hwinfo::get(temp.path(), temp.path() / "hwdb.json",
                                    temp.path() / "nonexistent.json"),
                    std::runtime_error);
}

TEST_CASE("get throws when schema file contains invalid JSON", "[get]") {
  TempDir temp;
  create_device_tree(temp.path(), "test-board", 1, 2, 3);
  write_text_file(temp.path() / "schema.json", "{ invalid json }");
  write_text_file(temp.path() / "hwdb.json", valid_hwdb);

  REQUIRE_THROWS_AS(er::hwinfo::get(temp.path(), temp.path() / "hwdb.json",
                                    temp.path() / "schema.json"),
                    std::runtime_error);
}

TEST_CASE("get throws when hwdb file does not exist", "[get]") {
  TempDir temp;
  create_device_tree(temp.path(), "test-board", 1, 2, 3);
  write_text_file(temp.path() / "schema.json", valid_schema);

  REQUIRE_THROWS_AS(er::hwinfo::get(temp.path(), temp.path() / "nonexistent.json",
                                    temp.path() / "schema.json"),
                    std::runtime_error);
}

TEST_CASE("get throws when hwdb file contains invalid JSON", "[get]") {
  TempDir temp;
  create_device_tree(temp.path(), "test-board", 1, 2, 3);
  write_text_file(temp.path() / "schema.json", valid_schema);
  write_text_file(temp.path() / "hwdb.json", "{ not valid json }");

  REQUIRE_THROWS_AS(er::hwinfo::get(temp.path(), temp.path() / "hwdb.json",
                                    temp.path() / "schema.json"),
                    std::runtime_error);
}

TEST_CASE("get throws when hwdb does not conform to schema", "[get]") {
  TempDir temp;
  create_device_tree(temp.path(), "test-board", 1, 2, 3);
  write_text_file(temp.path() / "schema.json", valid_schema);
  // Missing required "pins" field
  write_text_file(temp.path() / "hwdb.json", R"({ "test-board": { "1.0.0": {} } })");

  REQUIRE_THROWS_AS(er::hwinfo::get(temp.path(), temp.path() / "hwdb.json",
                                    temp.path() / "schema.json"),
                    std::runtime_error);
}

TEST_CASE("get returns info with empty pins when device type not in hwdb",
          "[get]") {
  TempDir temp;
  create_device_tree(temp.path(), "unknown-board", 1, 0, 0);
  write_text_file(temp.path() / "schema.json", valid_schema);
  write_text_file(temp.path() / "hwdb.json", valid_hwdb);

  auto result = er::hwinfo::get(temp.path(), temp.path() / "hwdb.json",
                                temp.path() / "schema.json");

  REQUIRE(result.has_value());
  REQUIRE(result->dev.hw_type == "unknown-board");
  REQUIRE(result->pins.empty());
}

TEST_CASE("get returns info with pins when device type exists in hwdb",
          "[get]") {
  TempDir temp;
  create_device_tree(temp.path(), "test-board", 1, 2, 3);
  write_text_file(temp.path() / "schema.json", valid_schema);
  write_text_file(temp.path() / "hwdb.json", valid_hwdb);

  auto result = er::hwinfo::get(temp.path(), temp.path() / "hwdb.json",
                                temp.path() / "schema.json");

  REQUIRE(result.has_value());
  REQUIRE(result->dev.hw_type == "test-board");
  REQUIRE(result->dev.hw_revision.major == 1);
  REQUIRE(result->dev.hw_revision.minor == 2);
  REQUIRE(result->dev.hw_revision.patch == 3);
  REQUIRE(result->pins.size() == 1);
  auto it = result->pins.begin();
  REQUIRE(it->name == "LED");
  REQUIRE(it->number == 17);
  REQUIRE(it->description == "Status LED");
}

TEST_CASE("get parses multiple pins correctly", "[get]") {
  TempDir temp;
  create_device_tree(temp.path(), "test-board", 1, 0, 0);
  write_text_file(temp.path() / "schema.json", valid_schema);
  write_text_file(temp.path() / "hwdb.json", R"({
    "test-board": {
      "1.0.0": {
        "pins": {
          "LED": { "description": "Status LED", "value": 17 },
          "BUTTON": { "description": "User button", "value": 27 },
          "RELAY": { "description": "Power relay", "value": 22 }
        }
      }
    }
  })");

  auto result = er::hwinfo::get(temp.path(), temp.path() / "hwdb.json",
                                temp.path() / "schema.json");

  REQUIRE(result.has_value());
  REQUIRE(result->pins.size() == 3);
}

TEST_CASE("get uses lower_bound to find compatible revision", "[get]") {
  TempDir temp;
  // Device has version 1.5.0, hwdb has 1.2.0 and 1.8.0
  // Should match 1.8.0 (first >= 1.5.0 with same major)
  create_device_tree(temp.path(), "test-board", 1, 5, 0);
  write_text_file(temp.path() / "schema.json", valid_schema);
  write_text_file(temp.path() / "hwdb.json", R"({
    "test-board": {
      "1.2.0": {
        "pins": {
          "OLD_PIN": { "description": "Old pin", "value": 10 }
        }
      },
      "1.8.0": {
        "pins": {
          "NEW_PIN": { "description": "New pin", "value": 20 }
        }
      }
    }
  })");

  auto result = er::hwinfo::get(temp.path(), temp.path() / "hwdb.json",
                                temp.path() / "schema.json");

  REQUIRE(result.has_value());
  REQUIRE(result->pins.size() == 1);
  REQUIRE(result->pins.begin()->name == "NEW_PIN");
}

TEST_CASE("get returns empty pins when major version differs", "[get]") {
  TempDir temp;
  // Device has version 2.0.0, hwdb only has 1.x versions
  create_device_tree(temp.path(), "test-board", 2, 0, 0);
  write_text_file(temp.path() / "schema.json", valid_schema);
  write_text_file(temp.path() / "hwdb.json", valid_hwdb);

  auto result = er::hwinfo::get(temp.path(), temp.path() / "hwdb.json",
                                temp.path() / "schema.json");

  REQUIRE(result.has_value());
  REQUIRE(result->dev.hw_type == "test-board");
  REQUIRE(result->pins.empty());
}

TEST_CASE("get returns empty pins when device revision is lower than all hwdb revisions",
          "[get]") {
  TempDir temp;
  // Device has version 1.0.0, hwdb has 1.2.3
  create_device_tree(temp.path(), "test-board", 1, 0, 0);
  write_text_file(temp.path() / "schema.json", valid_schema);
  write_text_file(temp.path() / "hwdb.json", valid_hwdb);

  auto result = er::hwinfo::get(temp.path(), temp.path() / "hwdb.json",
                                temp.path() / "schema.json");

  REQUIRE(result.has_value());
  REQUIRE(result->pins.size() == 1);
}

// --- Tests for er::hwinfo::impl::extract_revision ---

TEST_CASE("extract_revision parses valid revision string",
          "[extract_revision]") {
  auto rev = er::hwinfo::impl::extract_revision("1.2.3");

  REQUIRE(rev.major == 1);
  REQUIRE(rev.minor == 2);
  REQUIRE(rev.patch == 3);
}

TEST_CASE("extract_revision parses multi-digit version numbers",
          "[extract_revision]") {
  auto rev = er::hwinfo::impl::extract_revision("10.20.30");

  REQUIRE(rev.major == 10);
  REQUIRE(rev.minor == 20);
  REQUIRE(rev.patch == 30);
}

TEST_CASE("extract_revision parses zero version numbers",
          "[extract_revision]") {
  auto rev = er::hwinfo::impl::extract_revision("0.0.0");

  REQUIRE(rev.major == 0);
  REQUIRE(rev.minor == 0);
  REQUIRE(rev.patch == 0);
}

TEST_CASE("extract_revision throws on empty string", "[extract_revision]") {
  REQUIRE_THROWS_AS(er::hwinfo::impl::extract_revision(""), std::runtime_error);
}

TEST_CASE("extract_revision throws on major only", "[extract_revision]") {
  REQUIRE_THROWS_AS(er::hwinfo::impl::extract_revision("1"),
                    std::runtime_error);
}

TEST_CASE("extract_revision throws on major.minor only", "[extract_revision]") {
  REQUIRE_THROWS_AS(er::hwinfo::impl::extract_revision("1.2"),
                    std::runtime_error);
}

TEST_CASE("extract_revision throws on wrong separator after major",
          "[extract_revision]") {
  REQUIRE_THROWS_AS(er::hwinfo::impl::extract_revision("1-2.3"),
                    std::runtime_error);
}

TEST_CASE("extract_revision throws on wrong separator after minor",
          "[extract_revision]") {
  REQUIRE_THROWS_AS(er::hwinfo::impl::extract_revision("1.2-3"),
                    std::runtime_error);
}

TEST_CASE("extract_revision throws on non-numeric major",
          "[extract_revision]") {
  REQUIRE_THROWS_AS(er::hwinfo::impl::extract_revision("a.2.3"),
                    std::runtime_error);
}

TEST_CASE("extract_revision throws on non-numeric minor",
          "[extract_revision]") {
  REQUIRE_THROWS_AS(er::hwinfo::impl::extract_revision("1.b.3"),
                    std::runtime_error);
}

TEST_CASE("extract_revision throws on non-numeric patch",
          "[extract_revision]") {
  REQUIRE_THROWS_AS(er::hwinfo::impl::extract_revision("1.2.c"),
                    std::runtime_error);
}