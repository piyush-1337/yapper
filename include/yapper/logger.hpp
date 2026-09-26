#pragma once

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <expected>
#include <filesystem>
#include <format>
#include <string>
#include <thread>
#include <utility>
#include <yapper/details/codec.hpp>
#include <yapper/details/log.hpp>
#include <yapper/details/ring_buffer.hpp>

namespace yapper {

class Yapper {
 public:
  Yapper(FILE* file, std::size_t rb_size);

  // defaults to stdout/stderr
  static auto init() -> std::expected<Yapper, std::string>;
  static auto init(std::size_t) -> std::expected<Yapper, std::string>;

  // log to some file
  static auto init(std::filesystem::path) -> std::expected<Yapper, std::string>;
  static auto init(std::filesystem::path, std::size_t)
      -> std::expected<Yapper, std::string>;

  template <typename... Args>
  auto log(std::format_string<Args...> fmt, const Args&... args) -> void;

  // pin, no move, no copy
  Yapper(Yapper&&) = delete;
  Yapper(const Yapper&) = delete;
  Yapper& operator=(const Yapper&) = delete;
  Yapper& operator=(Yapper&&) = delete;
  ~Yapper();

 private:
  FILE* m_file;
  details::RingBuffer m_rb;
  std::jthread m_writer;
};

inline Yapper::~Yapper() {
  m_writer.request_stop();
  if (m_writer.joinable()) {
    m_writer.join();
  }

  if (m_file != nullptr && m_file != stdout && m_file != stderr) {
    ::fclose(m_file);
  }
}

template <typename... Args>
auto Yapper::log(std::format_string<Args...> fmt, const Args&... args) -> void {
  const auto size = details::calculate_payload_size(args...);

  auto* cursor = m_rb.claim(size);
  if (!cursor) return;  // drop the log? maybe return a bool clarifying success?

  using FormatAndLogFn = void (*)(FILE*, const std::byte*);

  FormatAndLogFn fn_ptr = details::format_and_log_payload<Args...>;
  std::memcpy(cursor, &fn_ptr, sizeof(FormatAndLogFn));

  const char* fmt_ptr = fmt.get().data();
  std::memcpy(cursor + 8, &fmt_ptr, sizeof(const char*));

  if constexpr (sizeof...(Args) > 0) {
    details::encode_all(cursor + 16, args...);
  }

  m_rb.commit();
}

constexpr std::size_t DEFAULT_RB_SIZE = 1024 * 1024;  // 1MB

inline auto Yapper::init() -> std::expected<Yapper, std::string> {
  return init(DEFAULT_RB_SIZE);
}

inline auto Yapper::init(std::size_t rb_size)
    -> std::expected<Yapper, std::string> {
  return std::expected<Yapper, std::string>(std::in_place, stdout, rb_size);
}

inline auto Yapper::init(std::filesystem::path path)
    -> std::expected<Yapper, std::string> {
  return init(std::move(path), DEFAULT_RB_SIZE);
}

inline auto Yapper::init(std::filesystem::path path, std::size_t rb_size)
    -> std::expected<Yapper, std::string> {
  FILE* file = ::fopen(path.string().c_str(), "a");
  if (!file) return std::unexpected("Failed to open file: " + path.string());

  return std::expected<Yapper, std::string>(std::in_place, file, rb_size);
}

inline Yapper::Yapper(FILE* file, std::size_t rb_size)
    : m_file(file), m_rb(rb_size) {
  m_writer = std::jthread([this](std::stop_token stoken) {
    auto process = [this]() -> bool {
      auto [payload, size] = m_rb.peek();
      if (!payload) return false;

      using FormatAndLogFn = void (*)(FILE*, const std::byte*);
      FormatAndLogFn format_and_log_payload;
      std::memcpy(&format_and_log_payload, payload, sizeof(FormatAndLogFn));

      format_and_log_payload(m_file, payload);

      m_rb.pop(size);
      return true;
    };

    while (!stoken.stop_requested()) {
      if (process()) continue;

      // potential improvement use some sort of signalling mechanism? semaphore?
      // cond_var?
      std::this_thread::yield();
    }

    while (process()) {
    }

    std::fflush(m_file);
  });
}

}  // namespace yapper
