#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "koto/app.hpp"
#include "koto/config.hpp"
#include "koto/gfx/framebuffer.hpp"
#include "koto/sim/hal_sim.hpp"
#include "koto/sim/http_server.hpp"
#include "koto/version.hpp"

namespace {

constexpr std::uint32_t kTickMs = koto::kTickMs;

std::string query_value(const std::string& query, const std::string& key, const std::string& fallback) {
  std::string prefix = key + "=";
  std::size_t pos = 0;
  while (pos < query.size()) {
    const std::size_t amp = query.find('&', pos);
    const std::string part = query.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
    if (part.compare(0, prefix.size(), prefix) == 0) {
      return part.substr(prefix.size());
    }
    if (amp == std::string::npos) {
      break;
    }
    pos = amp + 1;
  }
  return fallback;
}

std::string find_www_dir(const char* argv0) {
  namespace fs = std::filesystem;
  std::vector<fs::path> candidates;
#ifdef KOTO_WWW_DIR
  candidates.emplace_back(KOTO_WWW_DIR);
#endif
  if (argv0 != nullptr) {
    const fs::path exe_dir = fs::absolute(fs::path(argv0)).parent_path();
    candidates.push_back(exe_dir / "www");
    candidates.push_back(exe_dir.parent_path() / "platforms" / "sim" / "www");
    candidates.push_back(exe_dir.parent_path().parent_path() / "platforms" / "sim" / "www");
  }
  candidates.push_back(fs::current_path() / "platforms" / "sim" / "www");
  candidates.push_back(fs::current_path() / "www");

  for (const fs::path& dir : candidates) {
    std::error_code ec;
    if (fs::exists(dir / "index.html", ec)) {
      return dir.string();
    }
  }
  return {};
}

int hex_nibble(char ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'a' && ch <= 'f') {
    return ch - 'a' + 10;
  }
  if (ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  }
  return -1;
}

std::vector<std::uint8_t> parse_hex_bytes(const std::string& raw) {
  std::vector<std::uint8_t> out;
  int hi = -1;
  for (char ch : raw) {
    const int nibble = hex_nibble(ch);
    if (nibble < 0) {
      continue;
    }
    if (hi < 0) {
      hi = nibble;
    } else {
      out.push_back(static_cast<std::uint8_t>((hi << 4) | nibble));
      hi = -1;
    }
  }
  return out;
}

float parse_form_float(const std::string& body, const std::string& key, float fallback) {
  return std::stof(query_value(body, key, std::to_string(fallback)));
}

}  // namespace

int main(int argc, char** argv) {
  std::uint16_t port = 8080;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
      port = static_cast<std::uint16_t>(std::stoi(argv[++i]));
    }
  }

  koto::sim::Matrix matrix(koto::kMatrixW, koto::kMatrixH);
  koto::sim::Oled oled(koto::kOledW, koto::kOledH);
  koto::sim::LedRing ring(koto::hal::kLedRingCount);
  koto::sim::HidHost hid;
  koto::sim::Clock clock;
  koto::sim::Sensors sensors;
  koto::sim::Fan fan;
  koto::sim::Store store;
  koto::App app(matrix, oled, ring, hid, clock, sensors, fan, store);

  std::mutex mu;
  std::atomic<bool> playing{true};

  {
    std::lock_guard<std::mutex> lock(mu);
    app.init();
  }

  std::thread ticker([&]() {
    while (true) {
      {
        std::lock_guard<std::mutex> lock(mu);
        if (playing.load()) {
          clock.advance(kTickMs);
          app.tick();
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(kTickMs));
    }
  });
  ticker.detach();

  koto::sim::HttpServer server;
  const std::string www_dir = find_www_dir(argc > 0 ? argv[0] : nullptr);
  if (www_dir.empty()) {
    std::cerr << "не найден каталог www с index.html\n";
    return 1;
  }
  server.set_static_dir(www_dir);

  server.get("/api/state", [&](const koto::sim::HttpRequest&) {
    std::vector<std::uint8_t> rgb;
    std::vector<std::uint8_t> bits;
    std::vector<std::uint8_t> ring_rgb;
    std::vector<std::string> log;
    koto::DeviceState state;
    std::uint32_t ticks = 0;
    std::uint32_t uptime = 0;
    bool is_playing = playing.load();

    {
      std::lock_guard<std::mutex> lock(mu);
      matrix.copy_rgb(rgb);
      oled.copy_bits(bits);
      ring.copy_rgb(ring_rgb);
      log = hid.log_copy();
      state = app.state();
      ticks = app.tick_count();
      uptime = clock.millis();
    }

    const koto::Color color = koto::gfx::hue(state.hue_deg);
    const koto::PadState& pad = state.pad;
    std::ostringstream json;
    json << "{"
         << "\"ok\":true,"
         << "\"version\":\"" << koto::kVersion << "\","
         << "\"playing\":" << (is_playing ? "true" : "false") << ","
         << "\"tick\":" << ticks << ","
         << "\"uptime_ms\":" << uptime << ","
         << "\"scene\":\"" << koto::sim::json_escape(state.scene) << "\","
         << "\"text\":\"" << koto::sim::json_escape(state.text) << "\","
         << "\"face\":\"" << koto::sim::json_escape(state.face) << "\","
         << "\"brightness\":" << static_cast<int>(state.brightness) << ","
         << "\"auto_scroll\":" << (state.auto_scroll ? "true" : "false") << ","
         << "\"faceset\":" << state.faceset << ","
         << "\"octant\":" << state.octant << ","
         << "\"blinking\":" << (state.blinking ? "true" : "false") << ","
         << "\"boop\":" << (state.boop ? "true" : "false") << ","
         << "\"dizzy\":" << (state.dizzy ? "true" : "false") << ","
         << "\"auto_blink\":" << (state.auto_blink ? "true" : "false") << ","
         << "\"mouth\":" << (state.mouth_enabled ? "true" : "false") << ","
         << "\"boop_enabled\":" << (state.boop_enabled ? "true" : "false") << ","
         << "\"fan\":" << static_cast<int>(state.fan_speed) << ","
         << "\"rare_chance\":" << static_cast<int>(state.rare_chance) << ","
         << "\"snake_score\":" << state.snake_score << ","
         << "\"transition\":\"" << koto::sim::json_escape(state.transition) << "\","
         << "\"sensors\":{"
         << "\"mic\":" << state.mic << ","
         << "\"proximity\":" << state.proximity << ","
         << "\"pitch\":" << state.pitch << ","
         << "\"roll\":" << state.roll << ","
         << "\"yaw\":" << state.yaw
         << "},"
         << "\"color\":[" << static_cast<int>(color.r) << ","
         << static_cast<int>(color.g) << ","
         << static_cast<int>(color.b) << "],"
         << "\"pad\":{"
         << "\"mode\":\"" << (pad.mode == koto::PadMode::Game ? "game" : "key") << "\","
         << "\"x\":" << static_cast<int>(pad.x) << ","
         << "\"y\":" << static_cast<int>(pad.y) << ","
         << "\"hat\":" << static_cast<int>(pad.hat) << ","
         << "\"a\":" << (pad.a() ? "true" : "false") << ","
         << "\"b\":" << (pad.b() ? "true" : "false") << ","
         << "\"x_btn\":" << (pad.x_btn() ? "true" : "false") << ","
         << "\"y_btn\":" << (pad.y_btn() ? "true" : "false") << ","
         << "\"ok\":" << (pad.ok() ? "true" : "false") << ","
         << "\"esc\":" << (pad.esc() ? "true" : "false") << ","
         << "\"select\":" << (pad.select() ? "true" : "false")
         << "},"
         << "\"matrix\":{\"w\":" << koto::kMatrixW << ",\"h\":" << koto::kMatrixH
         << ",\"rgb\":\"" << koto::sim::base64_encode(rgb.data(), rgb.size()) << "\"},"
         << "\"oled\":{\"w\":" << koto::kOledW << ",\"h\":" << koto::kOledH
         << ",\"bits\":\"" << koto::sim::base64_encode(bits.data(), bits.size()) << "\"},"
         << "\"ring\":{\"n\":" << koto::hal::kLedRingCount
         << ",\"copies\":" << koto::hal::kLedRingCopies
         << ",\"rgb\":\"" << koto::sim::base64_encode(ring_rgb.data(), ring_rgb.size()) << "\"},"
         << "\"hid_log\":[";
    for (std::size_t i = 0; i < log.size(); ++i) {
      if (i != 0) {
        json << ",";
      }
      json << "\"" << koto::sim::json_escape(log[i]) << "\"";
    }
    json << "]}";
    return koto::sim::HttpResponse{200, "application/json; charset=utf-8", json.str()};
  });

  server.post("/api/hid", [&](const koto::sim::HttpRequest& req) {
    const std::vector<std::uint8_t> report = parse_hex_bytes(req.body);
    std::string reply;
    if (report.empty()) {
      return koto::sim::HttpResponse{400, "application/json; charset=utf-8", "{\"ok\":false}"};
    }
    {
      std::lock_guard<std::mutex> lock(mu);
      hid.inject_report(report.data(), report.size());
      const auto log = hid.log_copy();
      if (!log.empty()) {
        reply = log.back();
      }
    }
    return koto::sim::HttpResponse{
        200,
        "application/json; charset=utf-8",
        std::string("{\"ok\":true,\"reply\":\"") + koto::sim::json_escape(reply) + "\"}"};
  });

  server.post("/api/sensors", [&](const koto::sim::HttpRequest& req) {
    const std::string src = req.body.empty() ? req.query : req.body;
    try {
      const float mic = std::clamp(parse_form_float(src, "mic", 0.0f), 0.0f, 1.0f);
      const float prox = std::clamp(parse_form_float(src, "prox", 0.0f), 0.0f, 1.0f);
      const float pitch = parse_form_float(src, "pitch", 0.0f);
      const float roll = parse_form_float(src, "roll", 0.0f);
      const float yaw = parse_form_float(src, "yaw", 0.0f);
      const bool calibrate = query_value(src, "calibrate", "0") == "1";
      {
        std::lock_guard<std::mutex> lock(mu);
        sensors.set_microphone(mic);
        sensors.set_proximity(prox);
        sensors.set_gyro(pitch, roll, yaw);
        if (calibrate) {
          app.calibrate_boop();
          hid.log_line("SNS calibrate boop");
        }
        if (!playing.load()) {
          app.tick();
        }
      }
    } catch (const std::exception&) {
      return koto::sim::HttpResponse{400, "application/json; charset=utf-8", "{\"ok\":false}"};
    }
    return koto::sim::HttpResponse{200, "application/json; charset=utf-8", "{\"ok\":true}"};
  });

  server.post("/api/fan", [&](const koto::sim::HttpRequest& req) {
    const std::string src = req.body.empty() ? req.query : req.body;
    try {
      const int duty = std::clamp(static_cast<int>(parse_form_float(src, "duty", 255.0f)), 0, 255);
      {
        std::lock_guard<std::mutex> lock(mu);
        app.set_fan_speed(static_cast<std::uint8_t>(duty));
        fan.set_speed(static_cast<std::uint8_t>(duty));
        if (!playing.load()) {
          app.tick();
        }
      }
    } catch (const std::exception&) {
      return koto::sim::HttpResponse{400, "application/json; charset=utf-8", "{\"ok\":false}"};
    }
    return koto::sim::HttpResponse{200, "application/json; charset=utf-8", "{\"ok\":true}"};
  });

  server.post("/api/play", [&](const koto::sim::HttpRequest&) {
    playing = true;
    return koto::sim::HttpResponse{200, "application/json; charset=utf-8", "{\"ok\":true,\"playing\":true}"};
  });

  server.post("/api/pause", [&](const koto::sim::HttpRequest&) {
    playing = false;
    return koto::sim::HttpResponse{200, "application/json; charset=utf-8", "{\"ok\":true,\"playing\":false}"};
  });

  server.post("/api/step", [&](const koto::sim::HttpRequest& req) {
    const int dt = std::max(1, std::stoi(query_value(req.query, "dt", "33")));
    {
      std::lock_guard<std::mutex> lock(mu);
      clock.advance(static_cast<std::uint32_t>(dt));
      app.tick();
    }
    return koto::sim::HttpResponse{200, "application/json; charset=utf-8", "{\"ok\":true}"};
  });

  server.post("/api/restart", [&](const koto::sim::HttpRequest&) {
    {
      std::lock_guard<std::mutex> lock(mu);
      clock.reset();
      app.restart();
      hid.log_line("SYS restart");
    }
    return koto::sim::HttpResponse{200, "application/json; charset=utf-8", "{\"ok\":true,\"playing\":true}"};
  });

  std::cout << "kotoproto-os " << koto::kVersion << " simulator\n"
            << "P3 " << koto::kMatrixW << "x" << koto::kMatrixH << " + OLED " << koto::kOledW << "x"
            << koto::kOledH
            << " + WS2812 x" << koto::hal::kLedRingCount
            << " + Mocute HID + mic/gyro/boop + fan\n"
            << "Open http://127.0.0.1:" << port << "/\n"
            << "www: " << www_dir << "\n"
            << std::flush;

  return server.listen("127.0.0.1", port);
}
