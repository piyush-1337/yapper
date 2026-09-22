#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <memory>
#include <new>
#include <string>

namespace yapper::details {

constexpr auto cache_line =
    std::size_t{std::hardware_destructive_interference_size};

struct alignas(8) MessageHeader {
  /// size of payload (excluding this header)
  uint32_t size;

  /// no actual payload, skip the redundant bytes by size
  bool skip;
};

class RingBuffer {
 private:
  struct alignas(cache_line) ProducerState {
    /// tail known to consumer
    std::atomic<std::size_t> published_tail{0};

    /// current working tail where data will be written
    std::size_t working_tail{0};

    /// fast checking if the ring buffer is full (avoid atomic read unless
    /// absolutely necessary)
    std::size_t cached_head{0};
  } m_producer;

  struct alignas(cache_line) ConsumerState {
    /// head known to producer
    std::atomic<std::size_t> published_head{0};

    /// current working head where data will be consumed
    std::size_t working_head{0};

    /// fast checking if the ring buffer is empty
    std::size_t cached_tail{0};

  } m_consumer;

  std::size_t m_capacity;
  std::unique_ptr<std::byte[]> m_buffer;

  explicit RingBuffer(std::size_t);

 public:
  static auto create(std::size_t capacity)
      -> std::expected<std::unique_ptr<RingBuffer>, std::string>;

  // pin, no copy/move
  RingBuffer(const RingBuffer&) = delete;
  RingBuffer& operator=(const RingBuffer&) = delete;
  RingBuffer(RingBuffer&&) = delete;
  RingBuffer& operator=(RingBuffer&&) = delete;

  ~RingBuffer() = default;

  // try to claim the slot to write payload
  auto claim(std::size_t payload_size) -> std::byte*;

  // after the data is written, signal the consumer
  auto commit() -> void;

  // returns pointer to valid payload if exists
  auto peek() -> std::tuple<std::byte*, std::size_t>;

  // pop :)
  auto pop(std::size_t payload_size) -> void;
};

inline auto RingBuffer::create(std::size_t capacity)
    -> std::expected<std::unique_ptr<RingBuffer>, std::string> {
  capacity = (capacity + 7) & ~7;

  if (capacity == 0) return std::unexpected("Ring buffer capacity must be > 0");

  return std::unique_ptr<RingBuffer>(new RingBuffer(capacity));
}

inline RingBuffer::RingBuffer(std::size_t capacity)
    : m_capacity(capacity),
      m_buffer(std::make_unique_for_overwrite<std::byte[]>(capacity)) {}

inline auto RingBuffer::claim(std::size_t payload_size) -> std::byte* {
  payload_size = (payload_size + 7) & ~7;
  const auto total_size = std::size_t{sizeof(MessageHeader) + payload_size};

  if (m_producer.working_tail == m_capacity) m_producer.working_tail = 0;

  // tail to end of array
  auto cont_avail = std::size_t{m_capacity - m_producer.working_tail};

  // tiny space at the end
  if (total_size > cont_avail) {
    // add skip header
    if (cont_avail > 0) {
      auto skip_msg = MessageHeader{
          .size = static_cast<uint32_t>(cont_avail - sizeof(MessageHeader)),
          .skip = true,
      };
      std::memcpy(m_buffer.get() + m_producer.working_tail, &skip_msg,
                  sizeof(MessageHeader));
    }

    // start at the beginning
    m_producer.working_tail = 0;
  }

  // check if tail is behind head and there is no space
  if (m_producer.working_tail < m_producer.cached_head &&
      m_producer.working_tail + total_size >= m_producer.cached_head) {
    // refresh the cached_head
    m_producer.cached_head =
        m_consumer.published_head.load(std::memory_order_acquire);

    // check if there is truly no space left
    if (m_producer.working_tail < m_producer.cached_head &&
        m_producer.working_tail + total_size >= m_producer.cached_head)
      return nullptr;
  }

  // check if you are writing something that's greater than the remaining size
  if (m_producer.working_tail >= m_producer.cached_head &&
      m_producer.working_tail + total_size >=
          m_capacity + m_producer.cached_head) {
    m_producer.cached_head =
        m_consumer.published_head.load(std::memory_order_acquire);

    if (m_producer.working_tail >= m_producer.cached_head &&
        m_producer.working_tail + total_size >=
            m_capacity + m_producer.cached_head) {
      return nullptr;
    }
  }

  // claim size for the actual payload
  auto msg = MessageHeader{
      .size = static_cast<uint32_t>(payload_size),
      .skip = false,
  };

  auto header_ptr = m_buffer.get() + m_producer.working_tail;
  std::memcpy(header_ptr, &msg, sizeof(MessageHeader));

  auto payload_ptr = header_ptr + sizeof(MessageHeader);
  m_producer.working_tail += total_size;

  return payload_ptr;
}

inline auto RingBuffer::commit() -> void {
  m_producer.published_tail.store(m_producer.working_tail,
                                  std::memory_order_release);
}

inline auto RingBuffer::peek() -> std::tuple<std::byte*, std::size_t> {
  if (m_consumer.working_head == m_consumer.cached_tail) {
    m_consumer.cached_tail =
        m_producer.published_tail.load(std::memory_order_acquire);

    if (m_consumer.working_head == m_consumer.cached_tail) {
      return {nullptr, 0};
    }
  }

  auto* msg = reinterpret_cast<MessageHeader*>(m_buffer.get() +
                                               m_consumer.working_head);

  if (msg->skip) {
    m_consumer.working_head = 0;
    msg = reinterpret_cast<MessageHeader*>(m_buffer.get() +
                                           m_consumer.working_head);
  }

  return {m_buffer.get() + m_consumer.working_head + sizeof(MessageHeader),
          msg->size};
}

inline auto RingBuffer::pop(std::size_t payload_size) -> void {
  m_consumer.working_head += sizeof(MessageHeader) + payload_size;

  m_consumer.published_head.store(m_consumer.working_head,
                                  std::memory_order_release);
}

}  // namespace yapper::details
