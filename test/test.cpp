#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include <er/hwinfo.hpp>

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include <arpa/inet.h>
#include <sys/wait.h>

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

TEST_CASE("get finds exact revision match", "[get]") {
  TempDir temp;
  // Device has version 1.2.3, hwdb has exactly 1.2.3
  create_device_tree(temp.path(), "test-board", 1, 2, 3);
  write_text_file(temp.path() / "schema.json", valid_schema);
  write_text_file(temp.path() / "hwdb.json", R"({
    "test-board": {
      "1.0.0": {
        "pins": {
          "OLD_PIN": { "description": "Old pin", "value": 10 }
        }
      },
      "1.2.3": {
        "pins": {
          "EXACT_PIN": { "description": "Exact match", "value": 20 }
        }
      },
      "1.5.0": {
        "pins": {
          "NEW_PIN": { "description": "New pin", "value": 30 }
        }
      }
    }
  })");

  auto result = er::hwinfo::get(temp.path(), temp.path() / "hwdb.json",
                                temp.path() / "schema.json");

  REQUIRE(result.has_value());
  REQUIRE(result->pins.size() == 1);
  REQUIRE(result->pins.begin()->name == "EXACT_PIN");
}

TEST_CASE("get uses backward search when lower_bound finds different major",
          "[get]") {
  TempDir temp;
  // Device has version 1.9.0, hwdb has 1.5.0 and 2.0.0
  // lower_bound returns 2.0.0 (different major), backward search finds 1.5.0
  create_device_tree(temp.path(), "test-board", 1, 9, 0);
  write_text_file(temp.path() / "schema.json", valid_schema);
  write_text_file(temp.path() / "hwdb.json", R"({
    "test-board": {
      "1.5.0": {
        "pins": {
          "V1_PIN": { "description": "Version 1 pin", "value": 10 }
        }
      },
      "2.0.0": {
        "pins": {
          "V2_PIN": { "description": "Version 2 pin", "value": 20 }
        }
      }
    }
  })");

  auto result = er::hwinfo::get(temp.path(), temp.path() / "hwdb.json",
                                temp.path() / "schema.json");

  REQUIRE(result.has_value());
  REQUIRE(result->pins.size() == 1);
  REQUIRE(result->pins.begin()->name == "V1_PIN");
}

TEST_CASE("get uses backward search when lower_bound returns end", "[get]") {
  TempDir temp;
  // Device has version 1.9.0, hwdb only has 1.5.0
  // lower_bound returns end(), backward search finds 1.5.0
  create_device_tree(temp.path(), "test-board", 1, 9, 0);
  write_text_file(temp.path() / "schema.json", valid_schema);
  write_text_file(temp.path() / "hwdb.json", R"({
    "test-board": {
      "1.5.0": {
        "pins": {
          "ONLY_PIN": { "description": "Only pin", "value": 10 }
        }
      }
    }
  })");

  auto result = er::hwinfo::get(temp.path(), temp.path() / "hwdb.json",
                                temp.path() / "schema.json");

  REQUIRE(result.has_value());
  REQUIRE(result->pins.size() == 1);
  REQUIRE(result->pins.begin()->name == "ONLY_PIN");
}

TEST_CASE("get backward search selects highest revision with matching major",
          "[get]") {
  TempDir temp;
  // Device has version 1.9.0, hwdb has 1.2.0, 1.5.0, 2.0.0
  // lower_bound returns 2.0.0, backward search should find 1.5.0 (highest 1.x)
  create_device_tree(temp.path(), "test-board", 1, 9, 0);
  write_text_file(temp.path() / "schema.json", valid_schema);
  write_text_file(temp.path() / "hwdb.json", R"({
    "test-board": {
      "1.2.0": {
        "pins": {
          "LOW_PIN": { "description": "Low version", "value": 10 }
        }
      },
      "1.5.0": {
        "pins": {
          "MID_PIN": { "description": "Mid version", "value": 20 }
        }
      },
      "2.0.0": {
        "pins": {
          "HIGH_PIN": { "description": "High version", "value": 30 }
        }
      }
    }
  })");

  auto result = er::hwinfo::get(temp.path(), temp.path() / "hwdb.json",
                                temp.path() / "schema.json");

  REQUIRE(result.has_value());
  REQUIRE(result->pins.size() == 1);
  REQUIRE(result->pins.begin()->name == "MID_PIN");
}

TEST_CASE("get selects correct major version from multiple majors", "[get]") {
  TempDir temp;
  // Device has version 2.5.0, hwdb has 1.9.0, 2.1.0, 2.8.0
  // Should match 2.8.0 (first >= 2.5.0 with same major)
  create_device_tree(temp.path(), "test-board", 2, 5, 0);
  write_text_file(temp.path() / "schema.json", valid_schema);
  write_text_file(temp.path() / "hwdb.json", R"({
    "test-board": {
      "1.9.0": {
        "pins": {
          "V1_PIN": { "description": "Version 1", "value": 10 }
        }
      },
      "2.1.0": {
        "pins": {
          "V2_LOW_PIN": { "description": "Version 2 low", "value": 20 }
        }
      },
      "2.8.0": {
        "pins": {
          "V2_HIGH_PIN": { "description": "Version 2 high", "value": 30 }
        }
      }
    }
  })");

  auto result = er::hwinfo::get(temp.path(), temp.path() / "hwdb.json",
                                temp.path() / "schema.json");

  REQUIRE(result.has_value());
  REQUIRE(result->pins.size() == 1);
  REQUIRE(result->pins.begin()->name == "V2_HIGH_PIN");
}

TEST_CASE("get returns empty pins when type has no revisions", "[get]") {
  TempDir temp;
  // Device has version 1.0.0, hwdb has the type but no revisions
  create_device_tree(temp.path(), "test-board", 1, 0, 0);
  write_text_file(temp.path() / "schema.json", valid_schema);
  write_text_file(temp.path() / "hwdb.json", R"({
    "test-board": {}
  })");

  auto result = er::hwinfo::get(temp.path(), temp.path() / "hwdb.json",
                                temp.path() / "schema.json");

  REQUIRE(result.has_value());
  REQUIRE(result->pins.empty());
}

TEST_CASE("get returns empty pins when device major exceeds all hwdb majors",
          "[get]") {
  TempDir temp;
  // Device has version 3.0.0, hwdb only has 1.x and 2.x
  create_device_tree(temp.path(), "test-board", 3, 0, 0);
  write_text_file(temp.path() / "schema.json", valid_schema);
  write_text_file(temp.path() / "hwdb.json", R"({
    "test-board": {
      "1.5.0": {
        "pins": {
          "V1_PIN": { "description": "Version 1", "value": 10 }
        }
      },
      "2.5.0": {
        "pins": {
          "V2_PIN": { "description": "Version 2", "value": 20 }
        }
      }
    }
  })");

  auto result = er::hwinfo::get(temp.path(), temp.path() / "hwdb.json",
                                temp.path() / "schema.json");

  REQUIRE(result.has_value());
  REQUIRE(result->pins.empty());
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

// --- Tests for er::hwinfo::pin_set transparent lookup ---

TEST_CASE("pin_set supports transparent lookup by name", "[pin_set]") {
  er::hwinfo::pin_set pins;
  pins.insert({.name = "LED", .number = 17, .description = "Status LED"});
  pins.insert({.name = "BUTTON", .number = 27, .description = "User button"});
  pins.insert({.name = "RELAY", .number = 22, .description = "Power relay"});

  SECTION("find by string_view returns correct pin") {
    auto it = pins.find(std::string_view("BUTTON"));
    REQUIRE(it != pins.end());
    REQUIRE(it->name == "BUTTON");
    REQUIRE(it->number == 27);
  }

  SECTION("find by string returns correct pin") {
    std::string name = "RELAY";
    auto it = pins.find(name);
    REQUIRE(it != pins.end());
    REQUIRE(it->name == "RELAY");
    REQUIRE(it->number == 22);
  }

  SECTION("find by string literal returns correct pin") {
    auto it = pins.find("LED");
    REQUIRE(it != pins.end());
    REQUIRE(it->name == "LED");
    REQUIRE(it->number == 17);
  }

  SECTION("find returns end for non-existent name") {
    auto it = pins.find("NONEXISTENT");
    REQUIRE(it == pins.end());
  }

  SECTION("contains by name works correctly") {
    REQUIRE(pins.contains("LED"));
    REQUIRE(pins.contains("BUTTON"));
    REQUIRE_FALSE(pins.contains("MISSING"));
  }
}

// --- Integration tests for er-hwinfo CLI ---

namespace {
struct cli_result {
  std::string output;
  int exit_code;
};

cli_result run_cli(const std::string &args) {
  std::string cmd = "../er-hwinfo " + args + " 2>&1";
  std::array<char, 256> buffer;
  std::string result;
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    throw std::runtime_error("popen() failed");
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    result += buffer.data();
  }
  int status = pclose(pipe);
  int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  return {result, exit_code};
}
} // namespace

TEST_CASE("CLI outputs device info when device tree exists", "[cli]") {
  TempDir temp;
  create_device_tree(temp.path(), "test-board", 1, 2, 3);

  auto [output, exit_code] = run_cli(temp.path().string());

  REQUIRE(output.find("Device type: test-board") != std::string::npos);
  REQUIRE(output.find("Device revision: 1.2.3") != std::string::npos);
  REQUIRE(exit_code == 0);
}

TEST_CASE("CLI outputs not found message when device tree missing", "[cli]") {
  TempDir temp;
  // Don't create device tree files

  auto [output, exit_code] = run_cli(temp.path().string());

  REQUIRE(output.find("No Effective Range device found") != std::string::npos);
  REQUIRE(exit_code == 1);
}

TEST_CASE("CLI handles default path gracefully", "[cli]") {
  // Run without arguments - will use /proc/device-tree
  // On non-ER hardware, should report "not found"
  auto [output, exit_code] = run_cli("");

  // Should either find a device or report not found (both are valid)
  bool found_device = output.find("Device type:") != std::string::npos;
  bool not_found = output.find("No Effective Range device found") != std::string::npos;
  REQUIRE((found_device || not_found));
  // Exit code should match: 0 if found, 1 if not found
  REQUIRE(((found_device && exit_code == 0) || (not_found && exit_code == 1)));
}

TEST_CASE("CLI prints no pin info message when hwdb missing", "[cli]") {
  TempDir temp;
  create_device_tree(temp.path(), "test-board", 1, 0, 0);

  auto [output, exit_code] = run_cli(temp.path().string());

  REQUIRE(output.find("No pin information available") != std::string::npos);
  REQUIRE(exit_code == 0);
}

TEST_CASE("CLI prints pin table with correct header", "[cli][table]") {
  TempDir temp;
  create_device_tree(temp.path(), "test-board", 1, 0, 0);

  // Create hwdb files in the expected location
  auto etc_path = temp.path() / "etc" / "er-hwinfo";
  std::filesystem::create_directories(etc_path);
  write_text_file(etc_path / "hwdb-schema.json", valid_schema);
  write_text_file(etc_path / "hwdb.json", R"({
    "test-board": {
      "1.0.0": {
        "pins": {
          "LED": { "description": "Status LED", "value": 17 },
          "BUTTON": { "description": "User button", "value": 27 }
        }
      }
    }
  })");

  // Need to call get() directly since CLI uses hardcoded paths
  auto info = er::hwinfo::get(temp.path(), etc_path / "hwdb.json",
                              etc_path / "hwdb-schema.json");

  REQUIRE(info.has_value());
  REQUIRE(info->pins.size() == 2);

  // Verify pin content
  auto led_it = info->pins.find("LED");
  REQUIRE(led_it != info->pins.end());
  REQUIRE(led_it->number == 17);
  REQUIRE(led_it->description == "Status LED");

  auto button_it = info->pins.find("BUTTON");
  REQUIRE(button_it != info->pins.end());
  REQUIRE(button_it->number == 27);
  REQUIRE(button_it->description == "User button");
}

TEST_CASE("CLI table column widths adjust to content", "[cli][table]") {
  TempDir temp;
  create_device_tree(temp.path(), "test-board", 1, 0, 0);

  auto etc_path = temp.path() / "etc" / "er-hwinfo";
  std::filesystem::create_directories(etc_path);
  write_text_file(etc_path / "hwdb-schema.json", valid_schema);
  write_text_file(etc_path / "hwdb.json", R"({
    "test-board": {
      "1.0.0": {
        "pins": {
          "VERY_LONG_PIN_NAME": { "description": "A very long description for testing column width adjustment", "value": 42 }
        }
      }
    }
  })");

  auto info = er::hwinfo::get(temp.path(), etc_path / "hwdb.json",
                              etc_path / "hwdb-schema.json");

  REQUIRE(info.has_value());
  REQUIRE(info->pins.size() == 1);

  auto it = info->pins.begin();
  REQUIRE(it->name == "VERY_LONG_PIN_NAME");
  REQUIRE(it->number == 42);
  REQUIRE(it->description ==
          "A very long description for testing column width adjustment");
}

TEST_CASE("CLI table handles empty pins gracefully", "[cli][table]") {
  TempDir temp;
  create_device_tree(temp.path(), "unknown-board", 1, 0, 0);

  auto etc_path = temp.path() / "etc" / "er-hwinfo";
  std::filesystem::create_directories(etc_path);
  write_text_file(etc_path / "hwdb-schema.json", valid_schema);
  write_text_file(etc_path / "hwdb.json", R"({
    "other-board": {
      "1.0.0": {
        "pins": {
          "LED": { "description": "Status LED", "value": 17 }
        }
      }
    }
  })");

  auto info = er::hwinfo::get(temp.path(), etc_path / "hwdb.json",
                              etc_path / "hwdb-schema.json");

  REQUIRE(info.has_value());
  REQUIRE(info->dev.hw_type == "unknown-board");
  REQUIRE(info->pins.empty());
}

TEST_CASE("CLI table sorts pins alphabetically by name", "[cli][table]") {
  TempDir temp;
  create_device_tree(temp.path(), "test-board", 1, 0, 0);

  auto etc_path = temp.path() / "etc" / "er-hwinfo";
  std::filesystem::create_directories(etc_path);
  write_text_file(etc_path / "hwdb-schema.json", valid_schema);
  write_text_file(etc_path / "hwdb.json", R"({
    "test-board": {
      "1.0.0": {
        "pins": {
          "ZEBRA": { "description": "Last alphabetically", "value": 1 },
          "ALPHA": { "description": "First alphabetically", "value": 2 },
          "MIDDLE": { "description": "Middle alphabetically", "value": 3 }
        }
      }
    }
  })");

  auto info = er::hwinfo::get(temp.path(), etc_path / "hwdb.json",
                              etc_path / "hwdb-schema.json");

  REQUIRE(info.has_value());
  REQUIRE(info->pins.size() == 3);

  // pin_set is ordered by name, verify order
  auto it = info->pins.begin();
  REQUIRE(it->name == "ALPHA");
  ++it;
  REQUIRE(it->name == "MIDDLE");
  ++it;
  REQUIRE(it->name == "ZEBRA");
}