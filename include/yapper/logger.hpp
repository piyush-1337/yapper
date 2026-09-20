#pragma once

#include <print>

namespace yapper {

inline auto print() -> void { std::println("log frrom yapper"); }

class Yapper {
public:
  Yapper();
  Yapper(Yapper &&) = default;
  Yapper(const Yapper &) = default;
  Yapper &operator=(Yapper &&) = default;
  Yapper &operator=(const Yapper &) = default;
  ~Yapper();

private:
};

inline Yapper::Yapper() {}

inline Yapper::~Yapper() {}

} // namespace yapper
