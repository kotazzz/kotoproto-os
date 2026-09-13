#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace koto {
namespace hal {

// HID host for Mocute GAME/KEY reports. Simulator injects the same 6-byte GAME
// frames (and 8-byte KEY) via HTTP. ESP32 start() only logs; connected() is
// stubbed true so App does not enter safe-mode on boot.
class IHidHost {
 public:
  using ReportHandler = std::function<void(const std::uint8_t* data, std::size_t len)>;

  virtual ~IHidHost() = default;
  virtual void start() = 0;
  virtual void set_report_handler(ReportHandler handler) = 0;
  virtual bool connected() const = 0;
};

}  // namespace hal
}  // namespace koto
