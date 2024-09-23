#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include <chrono>
#include <functional>
#include <memory>
#include <queue>
#include <vector>

#include <boost/asio/io_context.hpp>

#include "base_packet.hpp"
#include "circle_buffer.hpp"
#include "defination.hpp"
#include "packets.hpp"
#include "raw_packet.hpp"
#include "socket.hpp"

namespace mstack {

using port_addr_t = uint16_t;

struct send_state_t {
        uint32_t                  unacknowledged = 0;
        uint32_t                  next           = 0;
        uint32_t                  window         = 0;
        int8_t                    window_sale    = 0;
        uint16_t                  mss            = 0;
        uint32_t                  cwnd           = 0;
        uint32_t                  ssthresh       = 0;
        uint16_t                  dupacks        = 0;
        uint16_t                  retransmits    = 0;
        uint16_t                  backoff        = 0;
        std::chrono::milliseconds rttvar;
        std::chrono::milliseconds srtt;
        std::chrono::milliseconds rto;
};

struct receive_state_t {
        uint32_t next         = 0;
        uint32_t window       = 0;
        uint8_t  window_scale = 0;
        uint16_t mss          = 0;
};

class tcb_manager;

struct tcb_t : public std::enable_shared_from_this<tcb_t> {
        boost::asio::io_context&                               io_ctx_;
        tcb_manager&                                           mngr_;
        std::shared_ptr<circle_buffer<std::shared_ptr<tcb_t>>> active_tcbs_;
        std::shared_ptr<listener_t>                            listener_;
        int                                                    state_;
        int                                                    next_state_;
        ipv4_port_t                                            local_info_;
        ipv4_port_t                                            remote_info_;
        circle_buffer<std::vector<std::byte>>                  send_queue_;
        circle_buffer<raw_packet>                              receive_queue_;
        std::queue<std::function<void(raw_packet)>>            on_data_receive_;
        circle_buffer<tcp_packet_t>                            ctl_packets_;
        send_state_t                                           send_;
        receive_state_t                                        receive_;

        explicit tcb_t(boost::asio::io_context&                               io_ctx,
                       tcb_manager&                                           mngr,
                       std::shared_ptr<circle_buffer<std::shared_ptr<tcb_t>>> active_tcbs,
                       std::shared_ptr<listener_t>                            listener,
                       ipv4_port_t const&                                     remote_info,
                       ipv4_port_t const&                                     local_info);

        void enqueue_send(std::span<std::byte const> packet);

        void listen_finish();

        void active_self();

        std::unique_ptr<base_packet> prepare_data_optional(int& option_len);

        std::optional<tcp_packet_t> make_packet();

        std::optional<tcp_packet_t> gather_packet();

        friend std::ostream& operator<<(std::ostream& out, tcb_t const& m) {
                out << m.remote_info_;
                out << " -> ";
                out << m.local_info_;
                out << " ";
                out << state_to_string(m.state_);
                return out;
        }
};

}  // namespace mstack

template <>
struct fmt::formatter<mstack::tcb_t> : fmt::formatter<std::string> {
        auto format(mstack::tcb_t const& c, format_context& ctx) {
                return formatter<std::string>::format((std::ostringstream{} << c).str(), ctx);
        }
};
