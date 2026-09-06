#pragma once

#include <cstddef>
#include <cstdint>

namespace koto {
namespace hal {

// Постоянное хранилище настроек: NVS на ESP32, файл в симе, память в тестах.
class IStore {
 public:
  virtual ~IStore() = default;
  virtual bool load(std::uint8_t* data, std::size_t size) = 0;
  virtual bool save(const std::uint8_t* data, std::size_t size) = 0;
  virtual std::uint32_t free_heap() const { return 0; }
};

}  // namespace hal
}  // namespace koto
