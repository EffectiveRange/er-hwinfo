#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

#include <arpa/inet.h>

namespace er {
namespace hwinfo {
struct revision {
  std::size_t major = 0;
  std::size_t minor = 0;
  std::size_t patch = 0;
};

struct pin {
  std::string name;
  std::size_t number;
  std::string description;
};

struct device {
  std::string hw_type;
  revision hw_revision;
};

struct info {
  device dev;
  std::vector<pin> pins;
};

namespace impl {
inline std::optional<device>
get_device(std::filesystem::path const &dt_base_path) {
  using std::filesystem::exists;
  const auto type_path = dt_base_path / "effective-range,type";
  const auto rev_major_path = dt_base_path / "effective-range,revision-major";
  const auto rev_minor_path = dt_base_path / "effective-range,revision-minor";
  const auto rev_patch_path = dt_base_path / "effective-range,revision-patch";
  if (!exists(type_path) || !exists(rev_major_path) ||
      !exists(rev_minor_path) || !exists(rev_patch_path)) {
    return std::nullopt;
  }

  std::ifstream type_file(type_path);
  std::istream_iterator<char> type_begin(type_file), type_end;
  std::string hw_type(type_begin, type_end);
  if (!hw_type.empty() && hw_type.back() == '\0') {
    hw_type.pop_back();
  }
  if (hw_type.empty()) {
    return std::nullopt;
  }
  const auto read_u32 =
      [](std::filesystem::path const &p) -> std::optional<std::uint32_t> {
    std::ifstream file(p, std::ios::binary);
    if (!file) {
      return std::nullopt;
    }
    std::uint32_t value = 0;
    file.read(reinterpret_cast<char *>(&value), sizeof(value));
    if (!file) {
      return std::nullopt;
    }
    value = ntohl(value);
    return value;
  };

  const auto rev_major = read_u32(rev_major_path);
  const auto rev_minor = read_u32(rev_minor_path);
  const auto rev_patch = read_u32(rev_patch_path);
  if (!rev_major || !rev_minor || !rev_patch) {
    return std::nullopt;
  }

  return device{
      .hw_type = hw_type,
      .hw_revision =
          revision{
              .major = *rev_major,
              .minor = *rev_minor,
              .patch = *rev_patch,
          },
  };
}
} // namespace impl

inline std::optional<info>
get(std::filesystem::path const &dt_base_path = "/proc/device-tree",
    std::filesystem::path const &hwdb_path = "/etc/er-hwinfo/db.json") {
  auto device_opt = impl::get_device(dt_base_path);
  if (!device_opt) {
    return std::nullopt;
  }
  rapidjson::Document hwdb;
  std::ifstream hwdb_file(hwdb_path);
  if (!hwdb_file) {
    return std::nullopt;
  }
  rapidjson::IStreamWrapper rstream(hwdb_file);
  hwdb.ParseStream<rapidjson::kParseCommentsFlag |
                   rapidjson::kParseTrailingCommasFlag>(rstream);
  return {};
}

} // namespace hwinfo

} // namespace er