#pragma once

#include <cassert>
#include <cstddef>

#include <memory>
#include <span>
#include <utility>

namespace mstack {

class base_packet {
private:
        std::unique_ptr<std::byte[]> data_;
        size_t                       capacity_;

        std::byte* start_;
        std::byte* end_;

        std::byte* head_;
        std::byte* tail_;

public:
        explicit base_packet(std::unique_ptr<std::byte[]> buf, size_t len, size_t head_off = 0)
            : data_{std::move(buf)},
              capacity_{len},
              start_{data_.get()},
              end_{start_ + capacity_},
              head_{start_ + head_off},
              tail_{end_} {
                assert(start_);
                assert(!(head_ > tail_));
        }

        ~base_packet() = default;

        base_packet(base_packet const&)            = delete;
        base_packet& operator=(base_packet const&) = delete;

        base_packet(base_packet&&)            = default;
        base_packet& operator=(base_packet&&) = default;

public:
        std::byte const* head() const { return head_; }
        std::byte*       head() { return head_; }

        std::byte const* tail() const { return tail_; }
        std::byte*       tail() { return tail_; }

        std::span<std::byte>       payload() { return {head(), tail()}; }
        std::span<std::byte const> payload() const { return {head(), tail()}; }

        size_t capacity() const { return capacity_; }

        size_t push_front_room() const { return head() - start_; }
        size_t pop_front_room() const { return payload().size(); }

        size_t push_back_room() const { return end_ - tail(); }
        size_t pop_back_room() const { return payload().size(); }

        std::byte* pop_front(size_t n) {
                head_ += n;
                assert(!(head_ > tail_));
                return head_;
        }

        std::byte* push_front(size_t n) {
                head_ -= n;
                assert(!(start_ > head_));
                return head_;
        }

        std::byte* pop_back(size_t n) {
                tail_ -= n;
                assert(!(head_ > tail_));
                return tail_;
        }

        std::byte* push_back(size_t n) {
                tail_ += n;
                assert(!(tail_ > end_));
                return tail_;
        }
};

}  // namespace mstack
