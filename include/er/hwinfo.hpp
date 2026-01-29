#pragma once

#include <charconv>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
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

namespace er {
namespace hwinfo {
struct revision {
  std::size_t major = 0;
  std::size_t minor = 0;
  std::size_t patch = 0;

  constexpr auto operator<=>(const revision &) const = default;
  std::string as_string() const {
    return fmt::format("{}.{}.{}", major, minor, patch);
  }
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

struct pin_compare {
  constexpr bool operator()(const pin &a, const pin &b) const noexcept {
    return a.name < b.name;
  }
};

using pin_set = std::set<pin, pin_compare>;
struct info {
  device dev;
  pin_set pins;
};

namespace impl {
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

  std::set<revision> revisions;
  std::transform(
      type_entry.GetObject().MemberBegin(), type_entry.GetObject().MemberEnd(),
      std::inserter(revisions, revisions.begin()),
      [](auto const &m) { return impl::extract_revision(m.name.GetString()); });
  auto revision_iter = revisions.lower_bound(requested);

  if (revision_iter == revisions.end() ||
      revision_iter->major != requested.major) {
    return type_entry.GetObject().MemberEnd();
  }

  const auto hwrevision_iter =
      type_entry.GetObject().FindMember(revision_iter->as_string().c_str());
  if (hwrevision_iter == type_entry.GetObject().MemberEnd()) {
    throw std::logic_error(fmt::format(
        "Inconsistent hardware database: computed revision {} not found",
        revision_iter->as_string()));
  }
  return hwrevision_iter;
}

} // namespace impl

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
  info result{.dev = *device_opt, .pins = {}};
  auto const &pins = hwrev_entry["pins"].GetObject();
  for (auto iter = pins.MemberBegin(); iter != pins.MemberEnd(); ++iter) {
    pin p;
    p.name = iter->name.GetString();
    p.number = iter->value["value"].GetUint64();
    p.description = iter->value["description"].GetString();
    result.pins.insert(std::move(p));
  }
  return result;
}

} // namespace hwinfo
} // namespace er