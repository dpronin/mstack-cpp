#pragma once

#include <memory>

#include "packets.hpp"
#include "tcb.hpp"

namespace mstack {

class tcp_transmit {
public:
        static void tcp_send_ack();
        static void tcp_send_syn_ack();
        static void tcp_send_rst();
        static void tcp_send_ctl();
        static int  generate_iss();
        static bool tcp_handle_close_state(std::shared_ptr<tcb_t> in_tcb, tcp_packet_t& in_packet);
        static bool tcp_handle_listen_state(std::shared_ptr<tcb_t> in_tcb, tcp_packet_t& in_packet);
        static bool tcp_handle_syn_sent(std::shared_ptr<tcb_t> in_tcb, tcp_packet_t& in_packet);
        static bool tcp_check_segment(std::shared_ptr<tcb_t> in_tcb, tcp_packet_t& in_packet);
        static void tcp_in(std::shared_ptr<tcb_t> in_tcb, tcp_packet_t&& in_packet);
};

}  // namespace mstack
