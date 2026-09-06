#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace koto {
namespace hal {

// BLE HID host: ESP32 central подключается к Mocute, sim подставляет те же репорты.
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
