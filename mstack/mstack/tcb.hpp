#pragma once

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>

#include "base_packet.hpp"
#include "circle_buffer.hpp"
#include "defination.hpp"
#include "packets.hpp"
#include "socket.hpp"
#include "tcp_header.hpp"

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

struct tcb_t : public std::enable_shared_from_this<tcb_t> {
        std::shared_ptr<circle_buffer<std::shared_ptr<tcb_t>>> _active_tcbs;
        std::shared_ptr<listener_t>                            _listener;
        int                                                    state;
        int                                                    next_state;
        std::optional<ipv4_port_t>                             remote_info;
        std::optional<ipv4_port_t>                             local_info;
        mutable std::mutex                                     send_queue_lock;
        std::condition_variable                                send_queue_cv;
        circle_buffer<raw_packet>                              send_queue;
        mutable std::mutex                                     receive_queue_lock;
        std::condition_variable                                receive_queue_cv;
        circle_buffer<raw_packet>                              receive_queue;
        circle_buffer<tcp_packet_t>                            ctl_packets;
        send_state_t                                           send;
        receive_state_t                                        receive;

        tcb_t(std::shared_ptr<circle_buffer<std::shared_ptr<tcb_t>>> active_tcbs,
              std::shared_ptr<listener_t>                            listener,
              ipv4_port_t                                            remote_info,
              ipv4_port_t                                            local_info)
            : _active_tcbs(active_tcbs),
              _listener(listener),
              remote_info(remote_info),
              local_info(local_info),
              state(TCP_CLOSED) {}

        void enqueue_send(raw_packet packet) {
                std::unique_lock l{send_queue_lock};
                send_queue.push_back(std::move(packet));
                active_self();
                l.unlock();
                send_queue_cv.notify_all();
        }

        void listen_finish() {
                if (this->_listener && this->_listener->acceptors) {
                        std::unique_lock l{this->_listener->lock};
                        _listener->acceptors->push_back(shared_from_this());
                        l.unlock();
                        this->_listener->cv.notify_all();
                }
        }

        void active_self() { _active_tcbs->push_back(shared_from_this()); }

        std::unique_ptr<base_packet> prepare_data_optional(int& option_len) { return {}; }

        std::optional<tcp_packet_t> make_packet() {
                std::optional<tcp_packet_t> r;

                tcp_header_t                 out_tcp;
                std::unique_ptr<base_packet> out_buffer;

                int option_len{0};

                std::unique_ptr<base_packet> data_buffer{prepare_data_optional(option_len)};

                if (data_buffer)
                        out_buffer = std::move(data_buffer);
                else
                        out_buffer = std::make_unique<base_packet>(tcp_header_t::size());

                out_tcp.src_port = local_info->port_addr.value();
                out_tcp.dst_port = remote_info->port_addr.value();
                out_tcp.ack_no   = receive.next;
                out_tcp.seq_no   = send.unacknowledged;

                // TODO
                out_tcp.window_size   = 0xFAF0;
                out_tcp.header_length = (tcp_header_t::size() + option_len) / 4;

                out_tcp.ACK = 1;

                if (this->next_state == TCP_SYN_RECEIVED) out_tcp.SYN = 1;

                out_tcp.produce(reinterpret_cast<uint8_t*>(out_buffer->get_pointer()));

                tcp_packet_t out_packet{
                        .proto       = 0x06,
                        .remote_info = this->remote_info,
                        .local_info  = this->local_info,
                        .buffer      = std::move(out_buffer),
                };

                if (this->next_state != this->state) this->state = this->next_state;

                r = std::move(out_packet);

                return r;
        }

        std::optional<tcp_packet_t> gather_packet() {
                return ctl_packets.empty() ? make_packet() : ctl_packets.pop_front();
        }

        friend std::ostream& operator<<(std::ostream& out, tcb_t const& m) {
                out << m.remote_info.value();
                out << " -> ";
                out << m.local_info.value();
                out << " ";
                out << state_to_string(m.state);
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
