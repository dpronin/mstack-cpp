#pragma once

#include <cerrno>
#include <cstdint>

#include <algorithm>
#include <memory>
#include <span>

#include "logger.hpp"

namespace mstack {

class base_packet {
private:
        std::vector<std::pair<int, std::unique_ptr<std::byte[]>>> _data_stack;
        std::unique_ptr<std::byte[]>                              _raw_data;

public:
        int _data_stack_len;
        int _head;
        int _len;

public:
        explicit base_packet(std::span<std::byte const> buf)
            : _raw_data(std::make_unique_for_overwrite<std::byte[]>(buf.size())),
              _head(0),
              _len(buf.size()),
              _data_stack_len(0) {
                std::ranges::copy(buf, _raw_data.get());
        }

        base_packet(int len)
            : _raw_data(std::make_unique<std::byte[]>(len)),
              _head(0),
              _len(len),
              _data_stack_len(0) {}

        ~base_packet()                        = default;
        base_packet(base_packet&)             = delete;
        base_packet(base_packet&&)            = delete;
        base_packet& operator=(base_packet&)  = delete;
        base_packet& operator=(base_packet&&) = delete;

public:
        std::byte* get_pointer() { return &_raw_data[_head]; }

        int get_remaining_len() { return _len - _head; }

        int get_total_len() { return _data_stack_len; }

        void add_offset(int offset) { _head += offset; }

        void reflush_packet(int len) {
                _data_stack_len += _len;
                _data_stack.push_back({_len, std::move(_raw_data)});

                _head     = 0;
                _len      = len;
                _raw_data = std::make_unique<std::byte[]>(len);
        }

        void export_payload(uint8_t* buf, int len) {
                int index = 0;
                for (int i = _head + len; i < _len; i++) {
                        buf[index++] = static_cast<uint8_t>(_raw_data[i]);
                }
        }

        ssize_t export_data(std::span<std::byte> buf) {
                if (_data_stack_len > buf.size()) return -EOVERFLOW;

                reflush_packet(1);

                int index = 0;
                for (int i = _data_stack.size() - 1; i >= 0; i--) {
                        for (int j = 0; j < _data_stack[i].first; j++) {
                                buf[index++] = _data_stack[i].second[j];
                        }
                }

                return index;
        }
};

}  // namespace mstack
