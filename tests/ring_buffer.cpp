#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <yapper/details/ring_buffer.hpp>
#include <thread>
#include <atomic>

TEST_CASE("Ring buffer", "[ring_buffer]") {
  SECTION("add and remove") {
    auto rb_res = yapper::details::RingBuffer::create(100);
    REQUIRE(rb_res.has_value());
    auto& rb = *rb_res.value();

    // empty
    auto [payload, size] = rb.peek();
    REQUIRE(payload == nullptr);
    REQUIRE(size == 0);

    auto data = std::uint64_t{0x1432BABE};

    // claim 8 bytes
    auto* ptr = rb.claim(8);
    REQUIRE(ptr != nullptr);

    // write and commit
    std::memcpy(ptr, &data, sizeof(data));
    rb.commit();

    // try consume
    auto [read_payload, read_size] = rb.peek();
    REQUIRE(read_payload != nullptr);
    REQUIRE(read_size == 8);

    auto read_data = std::uint64_t{};
    std::memcpy(&read_data, read_payload, sizeof(read_data));
    REQUIRE(read_data == data);

    rb.pop(read_size);

    // should be empty
    auto [final_payload, final_size] = rb.peek();
    REQUIRE(final_payload == nullptr);
    REQUIRE(final_size == 0);
  }

  SECTION("wrapping and skip header in play") {
    auto rb_result = yapper::details::RingBuffer::create(64);
    REQUIRE(rb_result.has_value());
    auto& rb = *rb_result.value();

    // write 24 bytes (total 32)
    auto msg1 = std::uint64_t{111};
    auto* ptr1 = rb.claim(24);
    REQUIRE(ptr1 != nullptr);
    std::memcpy(ptr1, &msg1, sizeof(msg1));
    rb.commit();

    // write 16 bytes (total 24)
    auto msg2 = std::uint64_t{222};
    auto* ptr2 = rb.claim(16);
    REQUIRE(ptr2 != nullptr);
    std::memcpy(ptr2, &msg2, sizeof(msg2));
    rb.commit();

    // write 8 bytes (total 16) this should return nullptr
    auto msg3 = std::uint64_t{333};
    auto* ptr3 = rb.claim(8);
    REQUIRE(ptr3 == nullptr);

    // read 1
    auto [read_ptr1, size1] = rb.peek();
    REQUIRE(size1 == 24);
    auto out1 = std::uint64_t{};
    std::memcpy(&out1, read_ptr1, sizeof(out1));
    REQUIRE(out1 == 111);
    rb.pop(size1);

    ptr3 = rb.claim(8);
    REQUIRE(ptr3 != nullptr);
    std::memcpy(ptr3, &msg3, sizeof(msg3));
    rb.commit();

    // read 2
    auto [read_ptr2, size2] = rb.peek();
    REQUIRE(size2 == 16);
    auto out2 = std::uint64_t{};
    std::memcpy(&out2, read_ptr2, sizeof(out2));
    REQUIRE(out2 == 222);
    rb.pop(size2);

    // read 3
    auto [read_ptr3, size3] = rb.peek();
    REQUIRE(size3 == 8);
    auto out3 = std::uint64_t{};
    std::memcpy(&out3, read_ptr3, sizeof(out3));
    REQUIRE(out3 == 333);
    rb.pop(size3);

    // verify buffer is empty
    auto [empty_ptr, empty_size] = rb.peek();
    REQUIRE(empty_ptr == nullptr);
    REQUIRE(empty_size == 0);
  }

  SECTION("Continuous cycling and multi-wrap endurance") {
    auto rb_result = yapper::details::RingBuffer::create(128);
    REQUIRE(rb_result.has_value());
    auto& rb = *rb_result.value();

    constexpr int cycles = 1000;

    // single-threaded interleave: fill, drain, wrap repeatedly
    uint64_t sequence_in = 0;
    uint64_t sequence_out = 0;

    for (int i = 0; i < cycles; ++i) {
      sequence_in++;
      std::byte* ptr = rb.claim(sizeof(sequence_in));

      while (ptr == nullptr) {
        // full, pop one to make room and retry
        auto [payload, size] = rb.peek();
        if (payload != nullptr) {
          sequence_out++;
          uint64_t val;
          std::memcpy(&val, payload, sizeof(val));
          REQUIRE(val == sequence_out);
          rb.pop(size);
        }
        ptr = rb.claim(sizeof(sequence_in));
      }

      std::memcpy(ptr, &sequence_in, sizeof(sequence_in));
      rb.commit();
    }

    // drain remaining
    while (true) {
      auto [payload, size] = rb.peek();
      if (payload == nullptr) break;

      sequence_out++;
      uint64_t val;
      std::memcpy(&val, payload, sizeof(val));
      rb.pop(size);
    }

    // verify no messages lost
    REQUIRE(sequence_in == sequence_out);
  }

  SECTION("Uneven wrapping to force skip messages") {
    auto rb_result = yapper::details::RingBuffer::create(128);
    REQUIRE(rb_result.has_value());
    auto& rb = *rb_result.value();

    for (int i = 0; i < 5; ++i) {
        auto* ptr = rb.claim(16);
        REQUIRE(ptr != nullptr);
        rb.commit();
    }
    
    // almost full (120/128), should fail
    auto* fail_ptr = rb.claim(16);
    REQUIRE(fail_ptr == nullptr);
    
    // pop two to make room for wrap
    for (int i = 0; i < 2; ++i) {
        auto [payload, size] = rb.peek();
        REQUIRE(size == 16);
        rb.pop(size);
    }
    
    // writes skip message at end, wraps to 0
    auto* ptr = rb.claim(16);
    REQUIRE(ptr != nullptr);
    rb.commit();
    
    // drain remaining unwrapped messages
    for (int i = 0; i < 3; ++i) {
        auto [p, s] = rb.peek();
        REQUIRE(s == 16);
        rb.pop(s);
    }
    
    // peek processes skip message, reads wrapped message
    auto [p2, s2] = rb.peek();
    REQUIRE(s2 == 16);
    rb.pop(s2);
    
    auto [empty, empty_s] = rb.peek();
    REQUIRE(empty == nullptr);
  }

  SECTION("multithreading check") {
    auto rb_result = yapper::details::RingBuffer::create(1024);
    REQUIRE(rb_result.has_value());
    auto& rb = *rb_result.value();

    constexpr uint64_t total_messages = 500000;
    
    std::thread producer([&]() {
      for (uint64_t i = 1; i <= total_messages; ++i) {
        std::byte* ptr = nullptr;
        while ((ptr = rb.claim(sizeof(i))) == nullptr) {
          std::this_thread::yield();
        }
        std::memcpy(ptr, &i, sizeof(i));
        rb.commit();
      }
    });

    std::atomic<bool> success{true};
    std::thread consumer([&]() {
      for (uint64_t i = 1; i <= total_messages; ++i) {
        std::byte* payload = nullptr;
        std::size_t size = 0;
        
        while (true) {
          auto [p, s] = rb.peek();
          if (p != nullptr) {
            payload = p;
            size = s;
            break;
          }
          std::this_thread::yield();
        }
        
        uint64_t val;
        std::memcpy(&val, payload, sizeof(val));
        if (val != i) {
          success = false;
          break;
        }
        rb.pop(size);
      }
    });

    producer.join();
    consumer.join();
    
    REQUIRE(success == true);
  }

  SECTION("Edge cases") {
    auto rb_result = yapper::details::RingBuffer::create(128);
    REQUIRE(rb_result.has_value());
    auto& rb = *rb_result.value();

    // zero-byte payload
    auto* ptr_zero = rb.claim(0);
    REQUIRE(ptr_zero != nullptr);
    rb.commit();

    auto [peek_zero, size_zero] = rb.peek();
    REQUIRE(peek_zero != nullptr);
    REQUIRE(size_zero == 0);
    rb.pop(size_zero);

    // max payload from offset 8 (112 payload + 8 header = 120)
    auto* ptr_112 = rb.claim(112);
    REQUIRE(ptr_112 != nullptr);
    rb.commit();
    
    auto [peek_112, size_112] = rb.peek();
    REQUIRE(size_112 == 112);
    rb.pop(size_112);
    
    // overflow refusal (head at 0, must leave 8-byte gap)
    auto* ptr_too_big = rb.claim(120); 
    REQUIRE(ptr_too_big == nullptr);
    
    auto* ptr_max_at_0 = rb.claim(112);
    REQUIRE(ptr_max_at_0 != nullptr);
    rb.commit();
    
    auto [peek_max_0, size_max_0] = rb.peek();
    REQUIRE(size_max_0 == 112);
    rb.pop(size_max_0);
    
    // idempotent peeks across skip boundary
    auto* ptr_wrap = rb.claim(16);
    REQUIRE(ptr_wrap != nullptr);
    rb.commit();
    
    // first peek processes skip, resets head to 0
    auto [peek_w1, size_w1] = rb.peek();
    REQUIRE(size_w1 == 16);
    
    // second peek should return the same result
    auto [peek_w2, size_w2] = rb.peek();
    REQUIRE(peek_w1 == peek_w2);
    REQUIRE(size_w1 == size_w2);
    
    rb.pop(size_w1);
  }
}
