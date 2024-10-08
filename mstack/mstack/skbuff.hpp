#pragma once

#include <cassert>
#include <cstddef>

#include <algorithm>
#include <memory>
#include <span>
#include <utility>

namespace mstack {

class skbuff {
private:
        std::unique_ptr<std::byte[]> data_;
        size_t                       capacity_;

        std::byte* start_;
        std::byte* end_;

        std::byte* head_;
        std::byte* tail_;

public:
        skbuff() = default;

        explicit skbuff(std::unique_ptr<std::byte[]> buf, size_t len, size_t head_off = 0)
            : data_{std::move(buf)},
              capacity_{len},
              start_{data_.get()},
              end_{start_ + capacity_},
              head_{start_ + head_off},
              tail_{end_} {
                assert(start_);
                assert(!(head_ > tail_));
        }

        ~skbuff() = default;

        skbuff(skbuff const& other) : capacity_(other.capacity_) {
                if (this != &other) {
                        data_ = std::make_unique_for_overwrite<std::byte[]>(capacity_);
                        std::copy_n(other.data_.get(), capacity_, data_.get());
                        start_ = data_.get();
                        end_   = start_ + capacity_;
                        head_  = start_ + other.headroom();
                        tail_  = end_ - other.tailroom();
                }
        }

        skbuff& operator=(skbuff const& other) {
                if (this != &other) {
                        auto copy{other};
                        using std::swap;
                        swap(*this, copy);
                }
                return *this;
        }

        skbuff(skbuff&&)            = default;
        skbuff& operator=(skbuff&&) = default;

public:
        std::byte const* head() const { return head_; }
        std::byte*       head() { return head_; }

        std::byte const* tail() const { return tail_; }
        std::byte*       tail() { return tail_; }

        std::span<std::byte>       payload() { return {head(), tail()}; }
        std::span<std::byte const> payload() const { return {head(), tail()}; }

        size_t capacity() const { return capacity_; }

        size_t headroom() const { return head() - start_; }
        size_t tailroom() const { return end_ - tail(); }

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
