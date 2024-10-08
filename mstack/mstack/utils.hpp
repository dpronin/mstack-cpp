#pragma once

#include <cassert>
#include <concepts>
#include <cstdint>

#include <algorithm>
#include <bit>
#include <span>

#include <net/if.h>

#include <spdlog/spdlog.h>

namespace mstack {

namespace utils {

namespace detail {

template <std::integral T>
constexpr T byteswap(T value) noexcept {
        static_assert(std::has_unique_object_representations_v<T>, "T may not have padding bits");
        auto array{std::bit_cast<std::array<std::byte, sizeof(T)>>(value)};
        std::ranges::reverse(array);
        return std::bit_cast<T>(array);
}

template <std::integral T>
constexpr T big_to_native(T v) noexcept {
        if constexpr (std::endian::little == std::endian::native)
                return byteswap(v);
        else
                return v;
}

template <std::integral T>
constexpr T native_to_big(T v) noexcept {
        return big_to_native(v);
}

template <std::integral T>
constexpr T little_to_native(T v) noexcept {
        if constexpr (std::endian::big == std::endian::native)
                return byteswap(v);
        else
                return v;
}

template <std::integral T>
constexpr T native_to_little(T v) noexcept {
        return little_to_native(v);
}

template <std::integral T>
constexpr T little_to_big(T v) noexcept {
        return byteswap(v);
}

template <std::integral T>
constexpr T big_to_little(T v) noexcept {
        return byteswap(v);
}

}  // namespace detail

template <typename T>
        requires(std::integral<T> || std::same_as<std::byte, T>)
constexpr auto hton(T value) {
        return detail::native_to_big(value);
}

template <typename T>
        requires(std::integral<T> || std::same_as<std::byte, T>)
constexpr void hton_inplace(T& value) {
        value = hton(value);
}

template <typename T>
        requires(std::integral<T> || std::same_as<std::byte, T>)
constexpr auto ntoh(T value) {
        return detail::big_to_native(value);
}

template <typename T>
        requires(std::integral<T> || std::same_as<std::byte, T>)
constexpr void ntoh_inplace(T& value) {
        value = ntoh(value);
}

template <typename T>
        requires(std::integral<T> || std::same_as<std::byte, T>)
constexpr T consume_from_net(std::byte*& ptr) {
        T ret = *(reinterpret_cast<T*>(ptr));
        ptr += sizeof(T);
        if constexpr (sizeof(ret) > 1)
                return ntoh(ret);
        else
                return ret;
}

template <typename T>
        requires(std::integral<T> || std::same_as<std::byte, T>)
constexpr void produce_to_net(std::byte*& p, T t) {
        T* ptr_ = reinterpret_cast<T*>(p);
        if constexpr (sizeof(T) > 1)
                *ptr_ = ntoh(t);
        else
                *ptr_ = t;
        p += sizeof(T);
}

inline uint32_t sum_every_16bits(std::span<std::byte const> buf) {
        uint32_t sum{0};

        assert(!(reinterpret_cast<uintptr_t>(buf.data()) & 0x1));

        auto const* ptr{reinterpret_cast<uint16_t const*>(buf.data())};

        auto count{buf.size()};
        while (count > 1) {
                /*  This is the inner loop */
                sum += *ptr++;
                count -= 2;
        }

        /*  Add left-over byte, if any */
        if (count > 0) sum += *(uint8_t*)ptr;

        return sum;
}

inline uint16_t checksum(std::span<std::byte const> buf, uint32_t start_sum = 0) {
        uint32_t sum = start_sum + sum_every_16bits(buf);

        /*  Fold 32-bit sum to 16 bits */
        while (sum >> 16)
                sum = (sum & 0xffff) + (sum >> 16);

        return static_cast<uint16_t>(~sum);
}

}  // namespace utils

}  // namespace mstack
