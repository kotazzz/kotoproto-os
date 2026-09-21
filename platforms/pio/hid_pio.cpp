#include "hal_pio.hpp"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <cstddef>
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string>
#include <utility>

#include "koto/protocol/mocute.hpp"

namespace koto {
namespace pio {
namespace {

constexpr uint16_t kTEsc = 0x01;
constexpr uint16_t kTMenu = 0x02;
constexpr uint16_t kTX = 0x04;
constexpr uint16_t kTY = 0x08;
constexpr uint16_t kTA = 0x10;
constexpr uint16_t kTB = 0x20;
constexpr uint16_t kTOk = 0x40;

const NimBLEUUID kHidService((uint16_t)0x1812);
const NimBLEUUID kHidReport((uint16_t)0x2A4D);
const NimBLEUUID kHidReportMap((uint16_t)0x2A4B);
const NimBLEUUID kHidProto((uint16_t)0x2A4E);
const NimBLEUUID kHidControl((uint16_t)0x2A4C);

SemaphoreHandle_t gMux = nullptr;
NimBLEAddress gTargetAddr;
bool gHaveTarget = false;
bool gConnecting = false;
volatile bool gConnected = false;
NimBLEClient* gClient = nullptr;
uint32_t gLastScanKickMs = 0;

uint8_t gQueued[8]{};
size_t gQueuedLen = 0;
bool gQueuedDirty = false;

uint8_t toKotoButtons(uint16_t raw) {
  uint8_t buttons = 0;
  if (raw & kTA) {
    buttons |= kBtnA;
  }
  if (raw & kTB) {
    buttons |= kBtnB;
  }
  if (raw & kTX) {
    buttons |= kBtnX;
  }
  if (raw & kTY) {
    buttons |= kBtnY;
  }
  if (raw & kTOk) {
    buttons |= kBtnOk;
  }
  if (raw & kTEsc) {
    buttons |= kBtnEsc;
  }
  if (raw & kTMenu) {
    buttons |= kBtnSelect;
  }
  return buttons;
}

uint16_t mapMocute052(uint16_t raw, uint8_t rt) {
  uint16_t buttons = 0;
  if (raw & 0x0002) {
    buttons |= kTEsc;
  }
  if (raw & 0x0400) {
    buttons |= kTMenu;
  }
  if (raw & 0x0008) {
    buttons |= kTX;
  }
  if (raw & 0x0010) {
    buttons |= kTY;
  }
  if (raw & 0x0800) {
    buttons |= kTA;
  }
  if (raw & 0x0080) {
    buttons |= kTB;
  }
  if (rt > 32 || (raw & 0x0001) || (raw & 0x0200)) {
    buttons |= kTOk;
  }
  return buttons;
}

void queueBytes(const uint8_t* data, size_t len) {
  if (gMux == nullptr || data == nullptr || len == 0 || len > sizeof(gQueued)) {
    return;
  }
  xSemaphoreTake(gMux, portMAX_DELAY);
  for (size_t i = 0; i < len; ++i) {
    gQueued[i] = data[i];
  }
  gQueuedLen = len;
  gQueuedDirty = true;
  xSemaphoreGive(gMux);
}

void queueGamepad(const uint8_t* data) {
  PadState pad;
  pad.mode = PadMode::Game;
  pad.x = data[0];
  pad.y = data[1];
  pad.hat = kHatCenter;
  const uint16_t raw = static_cast<uint16_t>(data[7]) | (static_cast<uint16_t>(data[8]) << 8);
  pad.buttons = toKotoButtons(mapMocute052(raw, data[5]));
  const auto packed = encode_mocute_report(pad);
  queueBytes(packed.data(), packed.size());
}

void notifyCb(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
  if (data == nullptr || len == 0 || len == 4) {
    return;
  }
  if (len >= 9) {
    queueGamepad(data);
  } else if (len == 8 && data[1] == 0) {
    queueBytes(data, 8);
  }
}

bool nameLooksMocute(const std::string& name) {
  std::string lower = name;
  for (char& c : lower) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return lower.find("mocute") != std::string::npos;
}

bool deviceLooksMocute(NimBLEAdvertisedDevice* dev) {
  if (dev == nullptr) {
    return false;
  }
  if (nameLooksMocute(dev->getName())) {
    return true;
  }
  if (dev->isAdvertisingService(kHidService)) {
    const uint16_t appearance = dev->getAppearance();
    if (appearance == 0x03C3 || appearance == 0x03C4) {
      return true;
    }
  }
  return false;
}

class ClientCb : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient*) override {
    gConnected = true;
    Serial.println("BLE connected");
  }

  void onDisconnect(NimBLEClient*) override {
    gConnected = false;
    gConnecting = false;
    gHaveTarget = false;
    Serial.println("BLE dropped, scan again");
    NimBLEDevice::getScan()->start(0, nullptr, false);
  }
};

class ScanCb : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* dev) override {
    if (dev == nullptr || gHaveTarget || gConnecting || gConnected) {
      return;
    }
    if (!deviceLooksMocute(dev)) {
      return;
    }
    gTargetAddr = dev->getAddress();
    gHaveTarget = true;
    Serial.print("MOCUTE ");
    Serial.println(dev->getAddress().toString().c_str());
    NimBLEDevice::getScan()->stop();
  }
};

ScanCb gScanCb;
ClientCb gClientCb;

bool subscribeHid(NimBLEClient* client) {
  NimBLERemoteService* hid = client->getService(kHidService);
  if (hid == nullptr) {
    auto* services = client->getServices(true);
    if (services == nullptr) {
      return false;
    }
    int n = 0;
    for (auto* svc : *services) {
      auto* chars = svc->getCharacteristics(true);
      if (chars == nullptr) {
        continue;
      }
      for (auto* ch : *chars) {
        if ((ch->canNotify() || ch->canIndicate()) && ch->subscribe(ch->canNotify(), notifyCb, true)) {
          ++n;
        }
      }
    }
    return n > 0;
  }

  NimBLERemoteCharacteristic* proto = hid->getCharacteristic(kHidProto);
  if (proto && proto->canWrite()) {
    uint8_t report_mode = 1;
    proto->writeValue(&report_mode, 1, true);
  }
  NimBLERemoteCharacteristic* ctrl = hid->getCharacteristic(kHidControl);
  if (ctrl && ctrl->canWrite()) {
    uint8_t wake = 0;
    ctrl->writeValue(&wake, 1, false);
  }

  auto* chars = hid->getCharacteristics(true);
  if (chars == nullptr) {
    return false;
  }
  int n = 0;
  for (auto* ch : *chars) {
    const bool report = ch->getUUID().equals(kHidReport);
    if ((report || ch->canNotify() || ch->canIndicate()) && (ch->canNotify() || ch->canIndicate())) {
      if (ch->subscribe(ch->canNotify(), notifyCb, true)) {
        ++n;
      }
    }
  }
  return n > 0;
}

bool connectTarget() {
  if (!gHaveTarget || gConnecting || gConnected) {
    return false;
  }
  gConnecting = true;
  Serial.println("BLE connect");
  if (gClient == nullptr) {
    gClient = NimBLEDevice::createClient();
    gClient->setClientCallbacks(&gClientCb, false);
    gClient->setConnectTimeout(15);
  }
  if (!gClient->connect(gTargetAddr)) {
    Serial.println("BLE connect failed");
    gConnecting = false;
    gHaveTarget = false;
    NimBLEDevice::getScan()->start(0, nullptr, false);
    return false;
  }
  gClient->getMTU();
  delay(150);
  if (!subscribeHid(gClient)) {
    Serial.println("HID subscribe failed");
    gClient->disconnect();
    gConnecting = false;
    return false;
  }
  gConnecting = false;
  gConnected = true;
  Serial.println("HID ready");
  return true;
}

}  // namespace

void HidHost::start() {
  gMux = xSemaphoreCreateMutex();
  NimBLEDevice::init("Kotaz");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(&gScanCb, false);
  scan->setActiveScan(true);
  scan->setInterval(160);
  scan->setWindow(80);
  scan->setDuplicateFilter(true);
  scan->start(0, nullptr, false);
  Serial.println("BLE scan MOCUTE");
}

void HidHost::set_report_handler(ReportHandler handler) {
  handler_ = std::move(handler);
}

bool HidHost::connected() const { return gConnected; }

void HidHost::pump() {
  if (gHaveTarget && !gConnected && !gConnecting) {
    connectTarget();
  }
  if (!gConnected && !gConnecting && (millis() - gLastScanKickMs) > 15000) {
    gLastScanKickMs = millis();
    NimBLEScan* scan = NimBLEDevice::getScan();
    if (scan != nullptr && !scan->isScanning()) {
      scan->start(0, nullptr, false);
    }
  }
  if (handler_ == nullptr || gMux == nullptr) {
    return;
  }
  uint8_t local[8];
  size_t len = 0;
  xSemaphoreTake(gMux, portMAX_DELAY);
  if (gQueuedDirty) {
    len = gQueuedLen;
    for (size_t i = 0; i < len; ++i) {
      local[i] = gQueued[i];
    }
    gQueuedDirty = false;
  }
  xSemaphoreGive(gMux);
  if (len > 0) {
    handler_(local, len);
  }
}

}  // namespace pio
}  // namespace koto
