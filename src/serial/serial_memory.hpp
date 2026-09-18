/**
 * @file serial_memory.hpp
 * @brief Shared in-memory SerialPort::Impl fields and helpers (tests).
 *
 * Included from the platform serial backends.
 */

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <functional>
#include <mutex>
#include <span>
#include <string>
#include <string_view>

namespace comettextel::serial_detail {

/**
 * @brief The memory state.
 */
struct MemoryState {
    bool enabled{false};
    std::string rx_queue;
    std::string tx_log;
    std::function<std::string(std::string_view)> responder;
    std::mutex mutex;
};

/**
 * @brief Resets the memory.
 * @param state The memory state.
 */
inline void memory_reset(MemoryState& state)
{
    std::lock_guard lock(state.mutex);
    state.enabled = false;
    state.rx_queue.clear();
    state.tx_log.clear();
    state.responder = {};
}

/**
 * @brief Enables the memory.
 * @param state The memory state.
 * @param responder The responder to use.
 */
inline void memory_enable(MemoryState& state,
                          std::function<std::string(std::string_view)> responder)
{
    std::lock_guard lock(state.mutex);
    state.enabled = true;
    state.rx_queue.clear();
    state.tx_log.clear();
    state.responder = std::move(responder);
}

/**
 * @brief Pushes the RX data to the memory.
 * @param state The memory state.
 * @param data The RX data to push.
 */
inline void memory_push_rx(MemoryState& state, std::string_view data)
{
    std::lock_guard lock(state.mutex);
    if (!state.enabled) {
        return;
    }
    state.rx_queue.append(data);
}

/**
 * @brief Takes the next TX data from the memory.
 * @param state The memory state.
 * @return The TX data.
 */
inline std::string memory_take_tx(MemoryState& state)
{
    std::lock_guard lock(state.mutex);
    std::string out = std::move(state.tx_log);
    state.tx_log.clear();
    return out;
}

/**
 * @brief Writes data to the memory.
 * @param state The memory state.
 * @param data The data to write.
 * @param written The number of bytes written.
 * @return true if the write was successful; false otherwise.
 */
inline bool memory_write(MemoryState& state,
                         std::string_view data,
                         std::size_t* written)
{
    std::lock_guard lock(state.mutex);
    if (!state.enabled) {
        return false;
    }

    state.tx_log.append(data);
    if (written) {
        *written = data.size();
    }

    if (state.responder) {
        state.rx_queue.append(state.responder(data));
    }

    return true;
}

/**
 * @brief Reads data from the memory.
 * @param state The memory state.
 * @param buffer The buffer to read into.
 * @param read_count The number of bytes read.
 * @return true if the read was successful; false otherwise.
 */
inline bool memory_read(MemoryState& state,
                        std::span<std::byte> buffer,
                        std::size_t& read_count)
{
    std::lock_guard lock(state.mutex);
    if (!state.enabled) {
        return false;
    }

    read_count = 0;
    if (buffer.empty() || state.rx_queue.empty()) {
        return true;
    }

    const std::size_t n = std::min(buffer.size(), state.rx_queue.size());
    std::memcpy(buffer.data(), state.rx_queue.data(), n);
    state.rx_queue.erase(0, n);
    read_count = n;
    return true;
}

} // namespace comettextel::serial_detail
