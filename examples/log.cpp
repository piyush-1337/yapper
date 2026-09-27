#include <yapper/yapper.hpp>

int main(int argc, char* argv[]) {
  auto yapper = yapper::create();
  for (int i{}; i < 10; i++) {
    yapper->log("[YAPPER]: Hello this is from {} and the number is {}",
                "yapper", i);
  }
  return 0;
}
