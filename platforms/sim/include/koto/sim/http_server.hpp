#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace koto {
namespace sim {

struct HttpRequest {
  std::string method;
  std::string path;
  std::string query;
  std::string body;
};

struct HttpResponse {
  int status = 200;
  std::string content_type = "text/plain; charset=utf-8";
  std::string body;
};

class HttpServer {
 public:
  using Handler = std::function<HttpResponse(const HttpRequest&)>;

  void get(const std::string& path, Handler handler);
  void post(const std::string& path, Handler handler);
  void set_static_dir(std::string dir);
  int listen(const char* host, std::uint16_t port);

 private:
  HttpResponse dispatch(const HttpRequest& req) const;
  HttpResponse serve_static(const std::string& path) const;

  std::unordered_map<std::string, Handler> gets_;
  std::unordered_map<std::string, Handler> posts_;
  std::string static_dir_;
};

std::string json_escape(std::string_view in);
std::string base64_encode(const std::uint8_t* data, std::size_t size);

}  // namespace sim
}  // namespace koto
