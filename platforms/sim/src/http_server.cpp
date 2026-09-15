#include "koto/sim/http_server.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#endif

namespace koto {
namespace sim {
namespace {

std::string mime_of(const std::string& path) {
  if (path.size() >= 5 && path.compare(path.size() - 5, 5, ".html") == 0) {
    return "text/html; charset=utf-8";
  }
  if (path.size() >= 3 && path.compare(path.size() - 3, 3, ".js") == 0) {
    return "application/javascript; charset=utf-8";
  }
  if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".css") == 0) {
    return "text/css; charset=utf-8";
  }
  if (path.size() >= 5 && path.compare(path.size() - 5, 5, ".json") == 0) {
    return "application/json; charset=utf-8";
  }
  if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".png") == 0) {
    return "image/png";
  }
  return "application/octet-stream";
}

std::string normalize_path(std::string path) {
  if (path.empty()) {
    return "/";
  }
  auto q = path.find('?');
  if (q != std::string::npos) {
    path.resize(q);
  }
  if (path == "/") {
    return "/index.html";
  }
  return path;
}

bool path_ok(const std::string& path) {
  return path.find("..") == std::string::npos;
}

std::string recv_all(socket_t fd) {
  std::string data;
  char buf[2048];
  while (data.find("\r\n\r\n") == std::string::npos) {
    const int n = recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) {
      break;
    }
    data.append(buf, buf + n);
    if (data.size() > 1024 * 1024) {
      break;
    }
  }

  std::size_t header_end = data.find("\r\n\r\n");
  if (header_end == std::string::npos) {
    return data;
  }
  header_end += 4;

  std::size_t content_length = 0;
  const std::string headers = data.substr(0, header_end);
  const std::string key = "Content-Length:";
  auto pos = headers.find(key);
  if (pos == std::string::npos) {
    pos = headers.find("content-length:");
  }
  if (pos != std::string::npos) {
    content_length = static_cast<std::size_t>(std::atoi(headers.c_str() + pos + key.size()));
  }

  while (data.size() < header_end + content_length) {
    const int n = recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) {
      break;
    }
    data.append(buf, buf + n);
  }
  return data;
}

HttpRequest parse_request(const std::string& raw) {
  HttpRequest req;
  std::istringstream in(raw);
  std::string version;
  in >> req.method >> req.path >> version;
  auto q = req.path.find('?');
  if (q != std::string::npos) {
    req.query = req.path.substr(q + 1);
    req.path.resize(q);
  }

  const auto body_pos = raw.find("\r\n\r\n");
  if (body_pos != std::string::npos) {
    req.body = raw.substr(body_pos + 4);
  }
  return req;
}

void send_response(socket_t fd, const HttpResponse& res) {
  std::ostringstream out;
  out << "HTTP/1.1 " << res.status << " OK\r\n"
      << "Content-Type: " << res.content_type << "\r\n"
      << "Content-Length: " << res.body.size() << "\r\n"
      << "Cache-Control: no-store\r\n"
      << "Access-Control-Allow-Origin: *\r\n"
      << "Connection: close\r\n\r\n"
      << res.body;
  const std::string packet = out.str();
  send(fd, packet.data(), static_cast<int>(packet.size()), 0);
}

}  // namespace

void HttpServer::get(const std::string& path, Handler handler) {
  gets_[path] = std::move(handler);
}

void HttpServer::post(const std::string& path, Handler handler) {
  posts_[path] = std::move(handler);
}

void HttpServer::set_static_dir(std::string dir) {
  static_dir_ = std::move(dir);
}

HttpResponse HttpServer::serve_static(const std::string& path) const {
  if (static_dir_.empty()) {
    return HttpResponse{404, "text/plain; charset=utf-8", "not found"};
  }
  const std::string norm = normalize_path(path);
  if (!path_ok(norm)) {
    return HttpResponse{400, "text/plain; charset=utf-8", "bad path"};
  }
  const std::string file = static_dir_ + norm;
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    return HttpResponse{404, "text/plain; charset=utf-8", "not found"};
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  return HttpResponse{200, mime_of(norm), ss.str()};
}

HttpResponse HttpServer::dispatch(const HttpRequest& req) const {
  if (req.method == "GET") {
    auto it = gets_.find(req.path);
    if (it != gets_.end()) {
      return it->second(req);
    }
    return serve_static(req.path);
  }
  if (req.method == "POST") {
    auto it = posts_.find(req.path);
    if (it != posts_.end()) {
      return it->second(req);
    }
    return HttpResponse{404, "text/plain; charset=utf-8", "not found"};
  }
  if (req.method == "OPTIONS") {
    return HttpResponse{204, "text/plain; charset=utf-8", ""};
  }
  return HttpResponse{405, "text/plain; charset=utf-8", "method not allowed"};
}

int HttpServer::listen(const char* host, std::uint16_t port) {
#ifdef _WIN32
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
    return 1;
  }
#endif

  socket_t server = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (server == INVALID_SOCKET) {
    return 1;
  }

  int opt = 1;
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
    closesocket(server);
    return 1;
  }
  if (bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
    closesocket(server);
    return 1;
  }
  if (::listen(server, 16) == SOCKET_ERROR) {
    closesocket(server);
    return 1;
  }

  for (;;) {
    socket_t client = accept(server, nullptr, nullptr);
    if (client == INVALID_SOCKET) {
      continue;
    }
    const std::string raw = recv_all(client);
    const HttpRequest req = parse_request(raw);
    const HttpResponse res = dispatch(req);
    send_response(client, res);
    closesocket(client);
  }
}

std::string json_escape(std::string_view in) {
  std::string out;
  out.reserve(in.size() + 8);
  for (char ch : in) {
    switch (ch) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out.push_back(ch);
        break;
    }
  }
  return out;
}

std::string base64_encode(const std::uint8_t* data, std::size_t size) {
  static const char kTable[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((size + 2) / 3) * 4);
  std::size_t i = 0;
  while (i + 2 < size) {
    const std::uint32_t n =
        (static_cast<std::uint32_t>(data[i]) << 16) |
        (static_cast<std::uint32_t>(data[i + 1]) << 8) |
        static_cast<std::uint32_t>(data[i + 2]);
    out.push_back(kTable[(n >> 18) & 63]);
    out.push_back(kTable[(n >> 12) & 63]);
    out.push_back(kTable[(n >> 6) & 63]);
    out.push_back(kTable[n & 63]);
    i += 3;
  }
  if (i < size) {
    std::uint32_t n = static_cast<std::uint32_t>(data[i]) << 16;
    if (i + 1 < size) {
      n |= static_cast<std::uint32_t>(data[i + 1]) << 8;
    }
    out.push_back(kTable[(n >> 18) & 63]);
    out.push_back(kTable[(n >> 12) & 63]);
    out.push_back(i + 1 < size ? kTable[(n >> 6) & 63] : '=');
    out.push_back('=');
  }
  return out;
}

}  // namespace sim
}  // namespace koto
