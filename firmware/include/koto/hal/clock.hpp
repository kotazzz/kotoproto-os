#pragma once

#include <cstdint>

namespace koto {
namespace hal {

class IClock {
 public:
  virtual ~IClock() = default;
  virtual std::uint32_t millis() const = 0;
};

}  // namespace hal
}  // namespace koto
