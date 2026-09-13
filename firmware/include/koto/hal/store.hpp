#pragma once

#include <cstddef>
#include <cstdint>

namespace koto {
namespace hal {

// Settings blob: file on the simulator, RAM in tests.
// ESP32 HAL is a RAM stub until NVS is wired; nvs_flash init in app_main is unused.
class IStore {
 public:
  virtual ~IStore() = default;
  virtual bool load(std::uint8_t* data, std::size_t size) = 0;
  virtual bool save(const std::uint8_t* data, std::size_t size) = 0;
  virtual std::uint32_t free_heap() const { return 0; }
};

}  // namespace hal
}  // namespace koto
