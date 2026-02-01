#include <er/hwinfo.hpp>

#include <algorithm>
#include <fmt/format.h>
#include <iostream>

int main(int argc, char *argv[]) {
  const char *dt_path = argc > 1 ? argv[1] : "/proc/device-tree";

  // First check if device exists
  auto const dev = er::hwinfo::impl::get_device(dt_path);
  if (!dev) {
    std::cout << "No Effective Range device found.\n";
    return 1;
  }

  std::cout << fmt::format("Device type: {}\n", dev->hw_type);
  std::cout << fmt::format("Device revision: {}\n",
                           dev->hw_revision.as_string());

  // Try to get pin information (may fail if hwdb files are missing)
  er::hwinfo::pin_set pins;
  try {
    auto const info = er::hwinfo::get(dt_path);
    if (info) {
      pins = info->pins;
    }
  } catch (const std::runtime_error &) {
    // hwdb files not available, continue without pin info
  }

  if (pins.empty()) {
    std::cout << "\nNo pin information available for this device.\n";
    return 0;
  }

  // Calculate column widths
  std::size_t name_width = 4; // "Name"
  std::size_t desc_width = 11; // "Description"
  for (const auto &pin : pins) {
    name_width = std::max(name_width, pin.name.size());
    desc_width = std::max(desc_width, pin.description.size());
  }

  // Print table header
  std::cout << fmt::format("\n{:<{}}  {:>5}  {:<{}}\n", "Name", name_width,
                           "GPIO#", "Description", desc_width);
  std::cout << fmt::format("{:-<{}}  {:->5}  {:-<{}}\n", "", name_width, "",
                           "", desc_width);

  // Print pins
  for (const auto &pin : pins) {
    std::cout << fmt::format("{:<{}}  {:>5}  {:<{}}\n", pin.name, name_width,
                             pin.number, pin.description, desc_width);
  }

  return 0;
}