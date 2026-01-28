#include <er/hwinfo.hpp>

#include <fmt/format.h>
#include <iostream>

int main() {
  auto const dev = er::hwinfo::impl::get_device("/proc/device-tree");
  if (dev) {
    std::cout << fmt::format("Device type: {}\n", dev->hw_type);
    std::cout << fmt::format("Device revision: {}.{}.{}\n",
                             dev->hw_revision.major, dev->hw_revision.minor,
                             dev->hw_revision.patch);
  } else {
    std::cout << "No Effective Range device found.\n";
  }
  return 0;
}