#pragma once

#include <cstddef>
#include <cstring>
#include <format>
#include <tuple>
#include <yapper/details/codec.hpp>

namespace yapper::details {

template <typename... Args>
auto format_and_log_payload(FILE* file, const std::byte* payload) -> void {
  const char* fmt_str;
  std::memcpy(&fmt_str, payload + 8, sizeof(const char*));

  const std::byte* cursor = payload + 16;

  auto decode_one = [&]<typename T>() {
    auto [val, next_cursor] = decode_arg<T>(cursor);
    cursor = next_cursor;
    return val;
  };

  auto args_tuple = std::tuple{decode_one.template operator()<Args>()...};

  std::apply(
      [&](auto&&... unpacked) {
        std::string out =
            std::vformat(fmt_str, std::make_format_args(unpacked...));
        out += '\n';
        std::fwrite(out.data(), 1, out.size(), file);
      },
      args_tuple);
}

}  // namespace yapper::details
