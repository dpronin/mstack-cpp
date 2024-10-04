#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include <chrono>
#include <functional>
#include <memory>
#include <queue>
#include <span>
#include <utility>

#include <boost/asio/io_context.hpp>
#include <boost/circular_buffer.hpp>

#include "base_packet.hpp"
#include "socket.hpp"
#include "tcp_header.hpp"
#include "tcp_packet.hpp"

namespace mstack {

using port_addr_t = uint16_t;

struct send {
        struct {
                uint32_t seq_nr_unack;
                uint32_t seq_nr_next;
                uint16_t window;
                uint8_t  window_scale;
                uint16_t mss;
                uint32_t cwnd;
                uint32_t ssthresh;
                uint16_t dupacks;
                uint16_t retransmits;
                uint16_t backoff;

                std::chrono::milliseconds rttvar;
                std::chrono::milliseconds srtt;
                std::chrono::milliseconds rto;
        } state;
        std::unique_ptr<boost::circular_buffer<std::byte>> pq;
};

struct receive {
        struct {
                uint32_t next;
                uint16_t window;
                uint8_t  window_scale;
                uint16_t mss;
        } state;
        std::unique_ptr<boost::circular_buffer<std::byte>> pq;
};

class tcb_manager;

class tcb_t : public std::enable_shared_from_this<tcb_t> {
private:
        boost::asio::io_context& io_ctx_;
        tcb_manager&             mngr_;

        ipv4_port_t remote_info_;
        ipv4_port_t local_info_;
        int         proto_;
        int         state_;
        int         next_state_;

        std::function<void(boost::system::error_code const& ec,
                           ipv4_port_t const&               remote_info,
                           ipv4_port_t const&               local_info,
                           std::weak_ptr<tcb_t>)>
                on_connection_established_;

        std::queue<std::pair<std::span<std::byte>, std::function<void(size_t)>>> on_data_receive_;

        send    send_;
        receive rcv_;

        explicit tcb_t(boost::asio::io_context&                  io_ctx,
                       tcb_manager&                              mngr,
                       ipv4_port_t const&                        remote_info,
                       ipv4_port_t const&                        local_info,
                       int                                       proto,
                       int                                       state,
                       int                                       next_state,
                       std::function<void(boost::system::error_code const& ec,
                                          ipv4_port_t const&               remote_info,
                                          ipv4_port_t const&               local_info,
                                          std::weak_ptr<tcb_t>)> on_connection_established);

public:
        template <typename... Args>
        static std::shared_ptr<tcb_t> create_shared(Args&&... args) {
                return std::shared_ptr<tcb_t>{new tcb_t{std::forward<Args>(args)...}};
        }

        static uint32_t generate_isn();

        ~tcb_t() = default;

        tcb_t(tcb_t const&)            = delete;
        tcb_t& operator=(tcb_t const&) = delete;

        tcb_t(tcb_t&&)            = delete;
        tcb_t& operator=(tcb_t&&) = delete;

        void async_read_some(std::span<std::byte>                                          buf,
                             std::function<void(boost::system::error_code const&, size_t)> cb);
        void async_write(std::span<std::byte const>                                    buf,
                         std::function<void(boost::system::error_code const&, size_t)> cb);

        void start_connecting();

        void process(tcp_packet&& in_packet);

        ipv4_port_t const& local_info() const { return local_info_; }
        ipv4_port_t const& remote_info() const { return remote_info_; }
        int                proto() const { return proto_; }

private:
        size_t app_data_unacknowleged() const;
        size_t app_data_to_send_left() const;

        bool has_app_data_to_send() const;

        void enqueue(tcp_packet&& out_pkt);

        void listen_finish();

        void make_and_send_pkt();

        void enqueue_app_data(std::span<std::byte const> packet);

        std::unique_ptr<base_packet> prepare_data_optional(int& option_len);

        tcp_packet make_packet();

        bool tcp_handle_close_state(tcp_header_t const& tcph);

        bool tcp_handle_listen_state(tcp_header_t const& tcph, std::span<std::byte const> opts);

        bool tcp_handle_syn_sent(tcp_header_t const& tcph, std::span<std::byte const> opts);

        bool tcp_check_segment(tcp_header_t const& tcph, uint16_t seglen);

        friend std::ostream& operator<<(std::ostream& out, tcb_t const& m);
};

inline std::ostream& operator<<(std::ostream& out, tcb_t const& m) {
        out << m.remote_info_;
        out << " -> ";
        out << m.local_info_;
        out << " ";
        out << state_to_string(m.state_);
        return out;
}

}  // namespace mstack

template <>
struct fmt::formatter<mstack::tcb_t> : fmt::formatter<std::string> {
        auto format(mstack::tcb_t const& c, format_context& ctx) {
                return formatter<std::string>::format((std::ostringstream{} << c).str(), ctx);
        }
};
