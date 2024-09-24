#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <queue>

#include <boost/asio/io_context.hpp>

#include "base_packet.hpp"
#include "defination.hpp"
#include "packets.hpp"
#include "socket.hpp"
#include "tcp_header.hpp"

namespace mstack {

using port_addr_t = uint16_t;

struct send_state_t {
        uint32_t                  unacknowledged;
        uint32_t                  next;
        uint32_t                  window;
        int8_t                    window_sale;
        uint16_t                  mss;
        uint32_t                  cwnd;
        uint32_t                  ssthresh;
        uint16_t                  dupacks;
        uint16_t                  retransmits;
        uint16_t                  backoff;
        std::chrono::milliseconds rttvar;
        std::chrono::milliseconds srtt;
        std::chrono::milliseconds rto;
};

struct receive_state_t {
        uint32_t next;
        uint32_t window;
        uint8_t  window_scale;
        uint16_t mss;
};

class tcb_manager;

class tcb_t : public std::enable_shared_from_this<tcb_t> {
private:
        boost::asio::io_context&    io_ctx_;
        tcb_manager&                mngr_;
        std::shared_ptr<listener_t> listener_;

        int proto_;
        int state_;
        int next_state_;

        ipv4_port_t local_info_;
        ipv4_port_t remote_info_;

        std::deque<std::byte> send_queue_;
        std::deque<std::byte> receive_queue_;

        std::queue<std::pair<std::span<std::byte>, std::function<void(size_t)>>> on_data_receive_;
        std::queue<tcp_packet_t>                                                 ctl_packets_;

        send_state_t    send_{};
        receive_state_t receive_{};

        explicit tcb_t(boost::asio::io_context&    io_ctx,
                       tcb_manager&                mngr,
                       std::shared_ptr<listener_t> listener,
                       ipv4_port_t const&          remote_info,
                       ipv4_port_t const&          local_info,
                       int                         proto,
                       int                         state,
                       int                         next_state);

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

        void listen_finish();

        void process(tcp_packet_t&& in_packet);

        std::shared_ptr<listener_t> listener() const { return listener_; }

        ipv4_port_t const& local_info() const { return local_info_; }
        ipv4_port_t const& remote_info() const { return remote_info_; }
        int                proto() const { return proto_; }

private:
        bool make_pkt_and_send();

        std::optional<tcp_packet_t> gather_packet();

        void enqueue_send(std::span<std::byte const> packet);

        std::unique_ptr<base_packet> prepare_data_optional(int& option_len);

        std::optional<tcp_packet_t> make_packet();

        void tcp_send_ack();

        void tcp_send_syn_ack();

        void tcp_send_rst();

        void tcp_send_ctl();

        bool tcp_handle_close_state(tcp_header_t const& tcph);

        bool tcp_handle_listen_state(tcp_header_t const& tcph, tcp_packet_t const& in_packet);

        bool tcp_handle_syn_sent(tcp_header_t const& tcph);

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
