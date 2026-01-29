#pragma once

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>

#include <arpa/inet.h>

#include <fmt/format.h>

/**
 * @namespace er::hwinfo
 * @brief Hardware information library for Effective Range devices.
 *
 * Provides functionality to query GPIO pin information from Raspberry Pi
 * devices running Effective Range hardware by reading the Linux device tree
 * and looking up pin definitions in a JSON hardware database.
 */
namespace er {
namespace hwinfo {

/**
 * @brief Semantic version representation.
 *
 * Represents a version number in major.minor.patch format.
 * Supports comparison operators via the spaceship operator.
 */
struct revision {
  std::size_t major = 0; ///< Major version number
  std::size_t minor = 0; ///< Minor version number
  std::size_t patch = 0; ///< Patch version number

  /// Default three-way comparison operator
  constexpr auto operator<=>(const revision &) const = default;

  /// @brief Converts the revision to a string in "major.minor.patch" format.
  /// @return String representation of the revision
  std::string as_string() const {
    return fmt::format("{}.{}.{}", major, minor, patch);
  }
};

/**
 * @brief GPIO pin definition.
 *
 * Contains information about a single GPIO pin including its name,
 * GPIO number, and human-readable description.
 */
struct pin {
  std::string name;        ///< Pin identifier (e.g., "LED", "BUTTON")
  std::size_t number;      ///< GPIO pin number (0-255)
  std::string description; ///< Human-readable description of the pin's purpose
};

/**
 * @brief Device identification.
 *
 * Contains the hardware type name and revision as read from the device tree.
 */
struct device {
  std::string hw_type;  ///< Hardware type identifier (e.g., "mrcm")
  revision hw_revision; ///< Hardware revision
};

/// @brief Comparator for ordering pins by name, with transparent lookup support
struct pin_compare {
  using is_transparent = void;
  constexpr bool operator()(const pin &a, const pin &b) const noexcept {
    return a.name < b.name;
  }
  bool operator()(const pin &a, std::string_view b) const noexcept {
    return a.name < b;
  }
  bool operator()(std::string_view a, const pin &b) const noexcept {
    return a < b.name;
  }
};

/// @brief Set of pins ordered by name
using pin_set = std::set<pin, pin_compare>;

/**
 * @brief Complete hardware information result.
 *
 * Contains device identification and all GPIO pin definitions
 * for the resolved hardware revision.
 */
struct info {
  device dev;   ///< Device identification
  pin_set pins; ///< GPIO pin definitions (may be empty if revision not found)
};

namespace impl {
namespace rg = std::ranges;
namespace rgv = std::ranges::views;

inline std::optional<device>
get_device(std::filesystem::path const &dt_base_path) {
  using std::filesystem::exists;
  const auto er_base_path = dt_base_path / "effective-range,hardware";
  const auto type_path = er_base_path / "effective-range,type";
  const auto rev_major_path = er_base_path / "effective-range,revision-major";
  const auto rev_minor_path = er_base_path / "effective-range,revision-minor";
  const auto rev_patch_path = er_base_path / "effective-range,revision-patch";
  if (!exists(type_path) || !exists(rev_major_path) ||
      !exists(rev_minor_path) || !exists(rev_patch_path)) {
    return std::nullopt;
  }

  std::ifstream type_file(type_path);
  std::string hw_type;
  type_file >> hw_type;
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

inline std::pair<std::size_t, const char *>
extract_revision_component(const char *first, const char *last, bool last_rev) {
  std::size_t value = 0;
  const auto res = std::from_chars(first, last, value);
  if (res.ec != std::errc() || (last_rev && res.ptr != last) ||
      (!last_rev && *res.ptr != '.')) {
    throw std::runtime_error(
        fmt::format("Invalid revision string component: {}",
                    std::string_view(first, last)));
  }
  return {value, res.ptr + (last_rev ? 0 : 1)};
}

inline revision extract_revision(std::string_view rev_str) {
  revision rev{0, 0, 0};
  const char *first = rev_str.begin();
  const char *const last = rev_str.end();
  std::tie(rev.major, first) = extract_revision_component(first, last, false);
  std::tie(rev.minor, first) = extract_revision_component(first, last, false);
  std::tie(rev.patch, first) = extract_revision_component(first, last, true);
  return rev;
}

template <auto Flags>
rapidjson::Document read_document(std::filesystem::path const &path) {
  rapidjson::Document doc;
  std::ifstream file(path);
  if (!file) {
    throw std::runtime_error(
        fmt::format("Failed to open json file: {}", path.string()));
  }
  rapidjson::IStreamWrapper sstream(file);
  if (auto &result = doc.ParseStream<Flags>(sstream); result.HasParseError()) {
    throw std::runtime_error(
        fmt::format("Failed to parse JSON file: {} ({})",
                    rapidjson::GetParseError_En(result.GetParseError()),
                    result.GetErrorOffset()));
  }
  return doc;
}

inline void validate_json(rapidjson::Document const &doc,
                          rapidjson::Document &schema_doc) {
  rapidjson::SchemaDocument schema(schema_doc);
  rapidjson::SchemaValidator validator(schema);
  if (!doc.Accept(validator)) {
    rapidjson::StringBuffer sb;
    validator.GetInvalidSchemaPointer().StringifyUriFragment(sb);
    throw std::runtime_error(
        fmt::format("JSON does not conform to schema: {}", sb.GetString()));
  }
}

template <auto Flags>
inline rapidjson::Document
read_and_validate_json(std::filesystem::path const &json_path,
                       std::filesystem::path const &schema_path) {
  rapidjson::Document schema = read_document<Flags>(schema_path);
  rapidjson::Document doc = read_document<Flags>(json_path);
  validate_json(doc, schema);
  return doc;
}

inline auto resolve_revision(revision requested, auto const &type_entry) {

  auto &&revs = rg::subrange(type_entry.GetObject().MemberBegin(),
                             type_entry.GetObject().MemberEnd()) |
                rgv::transform([](auto const &m) {
                  return impl::extract_revision(m.name.GetString());
                });
  std::set<revision> revisions(rg::begin(revs), rg::end(revs));
  auto revision_iter = revisions.lower_bound(requested);

  if (revision_iter == revisions.end() ||
      (*revision_iter != requested &&
       revision_iter->major != requested.major)) {
    auto found_iter = std::find_if(
        std::make_reverse_iterator(revision_iter), revisions.rend(),
        [&](revision const &rev) { return rev.major == requested.major; });
    if (found_iter == revisions.rend()) {
      return type_entry.GetObject().MemberEnd();
    }
    revision_iter = --found_iter.base();
  }
  const auto hwrevision_iter =
      type_entry.GetObject().FindMember(revision_iter->as_string().c_str());
  if (hwrevision_iter == type_entry.GetObject().MemberEnd()) {
    throw std::logic_error(
        fmt::format("Inconsistent hardware database: computed revision "
                    "{} not found",
                    revision_iter->as_string()));
  }
  return hwrevision_iter;
}

} // namespace impl

/**
 * @brief Query hardware information for the current device.
 *
 * Reads device type and revision from the Linux device tree, then looks up
 * GPIO pin definitions from the hardware database. Uses intelligent revision
 * matching to find compatible pin definitions.
 *
 * @param dt_base_path Path to the device tree base directory
 * @param hwdb_path Path to the hardware database JSON file
 * @param hwdb_schema_path Path to the JSON schema for validation
 *
 * @return std::optional<info> containing device info and pins, or std::nullopt
 *         if the device tree is missing or invalid
 *
 * @throws std::runtime_error if JSON files cannot be opened or parsed
 * @throws std::runtime_error if JSON fails schema validation
 *
 * @note Returns info with empty pins if device type is not in the database
 *       or no compatible revision is found (different major version)
 *
 * @par Revision Matching Algorithm:
 * 1. Exact match: use if device revision matches a database entry exactly
 * 2. Forward match: use first database revision >= device with same major
 * 3. Backward search: use highest database revision with same major
 * 4. No match: return empty pins if no same-major revision exists
 *
 * @par Example:
 * @code
 * auto info = er::hwinfo::get();
 * if (info) {
 *     std::cout << "Type: " << info->dev.hw_type << "\n";
 *     for (const auto& pin : info->pins) {
 *         std::cout << pin.name << ": GPIO " << pin.number << "\n";
 *     }
 * }
 * @endcode
 */
inline std::optional<info>
get(std::filesystem::path const &dt_base_path = "/proc/device-tree",
    std::filesystem::path const &hwdb_path = "/etc/er-hwinfo/hwdb.json",
    std::filesystem::path const &hwdb_schema_path =
        "/etc/er-hwinfo/hwdb-schema.json") {
  auto device_opt = impl::get_device(dt_base_path);
  if (!device_opt) {
    return std::nullopt;
  }
  constexpr auto flags =
      rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag;
  rapidjson::Document hwdb =
      impl::read_and_validate_json<flags>(hwdb_path, hwdb_schema_path);

  auto const type_iter = hwdb.FindMember(device_opt->hw_type.c_str());
  if (type_iter == hwdb.MemberEnd()) {
    return info{.dev = *device_opt, .pins = {}};
  }
  const auto &type_entry = type_iter->value;
  const auto hwrevision_iter =
      impl::resolve_revision(device_opt->hw_revision, type_entry);
  if (hwrevision_iter == type_entry.GetObject().MemberEnd()) {
    return info{.dev = *device_opt, .pins = {}};
  }
  auto &hwrev_entry = hwrevision_iter->value;
  auto const &pins = hwrev_entry["pins"].GetObject();
  auto &&pinrange =
      impl::rg::subrange(pins.MemberBegin(), pins.MemberEnd()) |
      impl::rgv::transform([&](auto const &m) {
        return pin{.name = m.name.GetString(),
                   .number = m.value["value"].GetUint(),
                   .description = m.value["description"].GetString()};
      });

  return info{.dev = *device_opt,
              .pins = {impl::rg::begin(pinrange), impl::rg::end(pinrange)}};
}

} // namespace hwinfo
} // namespace er