#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace koto {

inline constexpr std::uint32_t kSettingsMagic = 0x13371337u;
inline constexpr std::uint8_t kSettingsVersion = 1;
inline constexpr std::size_t kSettingsBlobSize = 16;

inline constexpr std::uint8_t kFlagMatrix = 1 << 0;
inline constexpr std::uint8_t kFlagLed = 1 << 1;
inline constexpr std::uint8_t kFlagBlink = 1 << 2;
inline constexpr std::uint8_t kFlagBoop = 1 << 3;
inline constexpr std::uint8_t kFlagMouth = 1 << 4;

struct SettingsBlob {
  std::uint32_t magic = kSettingsMagic;
  std::uint8_t version = kSettingsVersion;
  std::uint8_t brightness = 8;
  std::uint8_t flags = kFlagMatrix | kFlagLed | kFlagBlink | kFlagBoop | kFlagMouth;
  std::uint8_t boop_sensitivity = 127;
  std::uint8_t rare_chance = 25;
  std::uint8_t fan_speed = 255;
  std::uint8_t reserved[2] = {0, 0};
  std::uint32_t checksum = 0;
};

inline std::uint32_t settings_checksum(const SettingsBlob& blob) {
  std::uint32_t sum = kSettingsMagic;
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(&blob);
  for (std::size_t i = 0; i < offsetof(SettingsBlob, checksum); ++i) {
    sum ^= static_cast<std::uint32_t>(bytes[i]) * static_cast<std::uint32_t>(i + 1u);
  }
  return sum;
}

inline void settings_seal(SettingsBlob& blob) {
  blob.magic = kSettingsMagic;
  blob.version = kSettingsVersion;
  blob.checksum = settings_checksum(blob);
}

inline bool settings_valid(const SettingsBlob& blob) {
  if (blob.magic != kSettingsMagic || blob.version != kSettingsVersion) {
    return false;
  }
  return blob.checksum == settings_checksum(blob);
}

static_assert(sizeof(SettingsBlob) == kSettingsBlobSize, "settings blob must stay 16 bytes");

}  // namespace koto
