#pragma once

#include <cstddef>
#include <cstring>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace yapper::details {

inline constexpr auto align_to_8(const std::size_t size) -> std::size_t {
  return (size + 7) & ~7;
}

template <typename T>
  requires std::is_trivially_copyable_v<T> &&
           (!std::is_convertible_v<T, std::string_view>)
auto encoded_size(const T& arg) -> std::size_t {
  return align_to_8(sizeof(arg));
}

template <typename T>
  requires std::is_convertible_v<T, std::string_view>
auto encoded_size(const T& arg) -> std::size_t {
  std::string_view sv{arg};
  return 8 + align_to_8(sv.size());
}

template <typename... Args>
auto calculate_payload_size(const Args&... args) -> std::size_t {
  return 16 + (0 + ... + encoded_size(args));
}

// encoders

template <typename T>
  requires std::is_trivially_copyable_v<T> &&
           (!std::is_convertible_v<T, std::string_view>)
auto encode_arg(std::byte* cursor, const T& arg) -> std::byte* {
  std::memcpy(cursor, &arg, sizeof(arg));
  return cursor + align_to_8(sizeof(arg));
}

template <typename T>
  requires std::is_convertible_v<T, std::string_view>
auto encode_arg(std::byte* cursor, const T& arg) -> std::byte* {
  std::string_view sv{arg};
  auto size{sv.size()};
  std::memcpy(cursor, &size, sizeof(size));
  std::memcpy(cursor + sizeof(size), sv.data(), size);

  return cursor + 8 + align_to_8(size);
}

template <typename... Args>
auto encode_all(std::byte* cursor, const Args&... args) -> void {
  ((cursor = encode_arg(cursor, args)), ...);
}

// decoders

template <typename T>
  requires std::is_trivially_copyable_v<T> &&
           (!std::is_convertible_v<T, std::string_view>)
auto decode_arg(const std::byte* cursor) -> std::tuple<T, const std::byte*> {
  T arg;
  std::memcpy(&arg, cursor, sizeof(arg));
  return {arg, cursor + align_to_8(sizeof(arg))};
}

template <typename T>
  requires std::is_convertible_v<T, std::string_view>
auto decode_arg(const std::byte* cursor)
    -> std::tuple<std::string_view, const std::byte*> {
  std::size_t size;
  std::memcpy(&size, cursor, sizeof(size));

  auto sv = std::string_view{reinterpret_cast<const char*>(cursor + 8), size};
  return {sv, cursor + 8 + align_to_8(size)};
}

}  // namespace yapper::details
