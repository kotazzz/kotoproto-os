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
#include "koto/assets/emotions.hpp"
#include "koto/color.hpp"
#include "koto/config.hpp"
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

bool query_has(const std::string& query, const std::string& key) {
  std::string prefix = key + "=";
  std::size_t pos = 0;
  while (pos < query.size()) {
    const std::size_t amp = query.find('&', pos);
    const std::string part = query.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
    if (part.compare(0, prefix.size(), prefix) == 0) {
      return true;
    }
    if (amp == std::string::npos) {
      break;
    }
    pos = amp + 1;
  }
  return false;
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

std::vector<float> parse_pcm(const std::string& src) {
  const std::string hex = query_value(src, "pcm", "");
  if (hex.empty()) {
    return {};
  }
  const std::vector<std::uint8_t> bytes = parse_hex_bytes(hex);
  std::vector<float> out;
  out.reserve(bytes.size());
  for (std::uint8_t b : bytes) {
    const float sample = static_cast<float>(static_cast<std::int8_t>(b)) / 127.0f;
    out.push_back(std::clamp(sample, -1.0f, 1.0f));
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
    koto::Color color{90, 220, 255};

    {
      std::lock_guard<std::mutex> lock(mu);
      matrix.copy_rgb(rgb);
      oled.copy_bits(bits);
      ring.copy_rgb(ring_rgb);
      log = hid.log_copy();
      state = app.state();
      ticks = app.tick_count();
      uptime = clock.millis();
      if (app.emotion() != nullptr) {
        color = app.emotion()->accent;
      }
    }

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
         << "\"faceset\":" << state.faceset << ","
         << "\"octant\":" << state.octant << ","
         << "\"blinking\":" << (state.blinking ? "true" : "false") << ","
         << "\"boop\":" << (state.boop ? "true" : "false") << ","
         << "\"dizzy\":" << (state.dizzy ? "true" : "false") << ","
         << "\"auto_blink\":" << (state.auto_blink ? "true" : "false") << ","
         << "\"mouth\":" << (state.mouth_enabled ? "true" : "false") << ","
         << "\"boop_enabled\":" << (state.boop_enabled ? "true" : "false") << ","
         << "\"mouth_sensitivity\":" << static_cast<int>(state.mouth_sensitivity) << ","
         << "\"fan\":" << static_cast<int>(state.fan_speed) << ","
         << "\"rare_chance\":" << static_cast<int>(state.rare_chance) << ","
         << "\"snake_score\":" << state.snake_score << ","
         << "\"dino_score\":" << state.dino_score << ","
         << "\"flappy_score\":" << state.flappy_score << ","
         << "\"tetris_score\":" << state.tetris_score << ","
         << "\"dvd_hits\":" << state.dvd_hits << ","
         << "\"bsod_bars\":" << state.bsod_bars << ","
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

  server.get("/api/emotions", [&](const koto::sim::HttpRequest&) {
    std::ostringstream json;
    json << "{\"ok\":true,\"emotions\":[";
    for (int i = 0; i < koto::assets::kEmotionCount; ++i) {
      const koto::assets::Emotion* e = koto::assets::emotion_at(i);
      if (e == nullptr) {
        continue;
      }
      if (i != 0) {
        json << ",";
      }
      json << "{\"id\":\"" << koto::sim::json_escape(e->id) << "\","
           << "\"label\":\"" << koto::sim::json_escape(e->label) << "\","
           << "\"short\":\"" << koto::sim::json_escape(e->short_label) << "\","
           << "\"kind\":\"" << koto::assets::kind_name(e->kind) << "\","
           << "\"effect\":\"" << koto::assets::effect_name(e->effect) << "\","
           << "\"transition\":\"" << koto::face::transition_name(e->transition) << "\","
           << "\"accent\":[" << static_cast<int>(e->accent.r) << ","
           << static_cast<int>(e->accent.g) << "," << static_cast<int>(e->accent.b) << "],"
           << "\"loop\":" << (e->loop ? "true" : "false") << ","
           << "\"allow_blink\":" << (e->allow_blink ? "true" : "false") << ","
           << "\"allow_boop\":" << (e->allow_boop ? "true" : "false") << ","
           << "\"frames\":" << e->frame_count << "}";
    }
    json << "]}";
    return koto::sim::HttpResponse{200, "application/json; charset=utf-8", json.str()};
  });

  server.post("/api/face", [&](const koto::sim::HttpRequest& req) {
    const std::string src = req.body.empty() ? req.query : req.body;
    const std::string id = query_value(src, "id", "");
    const bool with_transition = query_value(src, "transition", "1") != "0";
    bool ok = false;
    {
      std::lock_guard<std::mutex> lock(mu);
      ok = app.set_face(id.c_str(), with_transition);
      if (!playing.load()) {
        app.tick();
      }
    }
    return koto::sim::HttpResponse{ok ? 200 : 404, "application/json; charset=utf-8",
                                   ok ? "{\"ok\":true}" : "{\"ok\":false}"};
  });

  server.post("/api/preview", [&](const koto::sim::HttpRequest& req) {
    const std::string src = req.body.empty() ? req.query : req.body;
    const bool blink = query_value(src, "blink", "0") == "1";
    {
      std::lock_guard<std::mutex> lock(mu);
      if (blink) {
        app.preview_blink();
      }
      if (!playing.load()) {
        app.tick();
      }
    }
    return koto::sim::HttpResponse{200, "application/json; charset=utf-8", "{\"ok\":true}"};
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
      const std::vector<float> pcm = parse_pcm(src);
      const float prox = std::clamp(parse_form_float(src, "prox", 0.0f), 0.0f, 1.0f);
      const float pitch = parse_form_float(src, "pitch", 0.0f);
      const float roll = parse_form_float(src, "roll", 0.0f);
      const float yaw = parse_form_float(src, "yaw", 0.0f);
      const bool calibrate = query_value(src, "calibrate", "0") == "1";
      {
        std::lock_guard<std::mutex> lock(mu);
        if (query_has(src, "pcm")) {
          if (!pcm.empty()) {
            sensors.set_microphone_pcm(pcm.data(), static_cast<int>(pcm.size()));
            if (sensors.microphone() < 0.02f && query_has(src, "mic") && mic > 0.02f) {
              sensors.set_microphone(mic);
            }
          } else if (query_has(src, "mic")) {
            sensors.set_microphone(mic);
          } else {
            sensors.set_microphone(0);
          }
        } else if (query_has(src, "mic")) {
          sensors.set_microphone(mic);
        }
        if (query_has(src, "prox")) {
          sensors.set_proximity(prox);
        }
        if (query_has(src, "pitch") || query_has(src, "roll") || query_has(src, "yaw")) {
          koto::hal::GyroSample gyro = sensors.gyro();
          if (query_has(src, "pitch")) {
            gyro.pitch_deg = pitch;
          }
          if (query_has(src, "roll")) {
            gyro.roll_deg = roll;
          }
          if (query_has(src, "yaw")) {
            gyro.yaw_deg = yaw;
          }
          sensors.set_gyro(gyro.pitch_deg, gyro.roll_deg, gyro.yaw_deg);
        }
        if (calibrate) {
          app.calibrate_boop();
          hid.log_line("SNS calibrate boop");
        }
        app.poll_sensors();
        if (!playing.load()) {
          app.tick();
        }
      }
      std::ostringstream json;
      json << "{\"ok\":true,\"mic\":" << sensors.microphone()
           << ",\"pitch\":" << sensors.gyro().pitch_deg << "}";
      return koto::sim::HttpResponse{200, "application/json; charset=utf-8", json.str()};
    } catch (const std::exception&) {
      return koto::sim::HttpResponse{400, "application/json; charset=utf-8", "{\"ok\":false}"};
    }
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
            << "Atlas: http://127.0.0.1:" << port << "/atlas.html\n"
            << "www: " << www_dir << "\n"
            << std::flush;

  return server.listen("127.0.0.1", port);
}
