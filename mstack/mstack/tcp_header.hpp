#pragma once

#include <cstddef>

#include <ostream>

#include "utils.hpp"

namespace mstack {

struct tcp_header_t {
        using port_addr_t = uint16_t;

        port_addr_t src_port;
        port_addr_t dst_port;
        uint32_t    seq_no;
        uint32_t    ack_no;

        uint16_t data_offset : 4;
        uint16_t reserved : 4;
        uint16_t CWR : 1;
        uint16_t ECE : 1;
        uint16_t URG : 1;
        uint16_t ACK : 1;
        uint16_t PSH : 1;
        uint16_t RST : 1;
        uint16_t SYN : 1;
        uint16_t FIN : 1;

        uint16_t window;
        uint16_t checksum;
        uint16_t urgent_pointer;

        static constexpr size_t fixed_size() { return sizeof(tcp_header_t); }

        static tcp_header_t consume(std::byte* ptr) {
                tcp_header_t tcp_header{};

                tcp_header.src_port = utils::consume<port_addr_t>(ptr);
                tcp_header.dst_port = utils::consume<port_addr_t>(ptr);
                tcp_header.seq_no   = utils::consume<uint32_t>(ptr);
                tcp_header.ack_no   = utils::consume<uint32_t>(ptr);

                auto const header_length_flags = utils::consume<uint16_t>(ptr);

                tcp_header.data_offset    = (header_length_flags >> 12) & 0xf;
                tcp_header.reserved       = (header_length_flags >> 8) & 0xf;
                tcp_header.CWR            = (header_length_flags >> 7) & 0x1;
                tcp_header.ECE            = (header_length_flags >> 6) & 0x1;
                tcp_header.URG            = (header_length_flags >> 5) & 0x1;
                tcp_header.ACK            = (header_length_flags >> 4) & 0x1;
                tcp_header.PSH            = (header_length_flags >> 3) & 0x1;
                tcp_header.RST            = (header_length_flags >> 2) & 0x1;
                tcp_header.SYN            = (header_length_flags >> 1) & 0x1;
                tcp_header.FIN            = (header_length_flags >> 0) & 0x1;
                tcp_header.window         = utils::consume<uint16_t>(ptr);
                tcp_header.checksum       = utils::consume<uint16_t>(ptr);
                tcp_header.urgent_pointer = utils::consume<uint16_t>(ptr);

                return tcp_header;
        }

        ptrdiff_t produce(std::byte* ptr) {
                std::byte const* f{ptr};
                utils::produce(ptr, src_port);
                utils::produce(ptr, dst_port);
                utils::produce(ptr, seq_no);
                utils::produce(ptr, ack_no);
                utils::produce<uint16_t>(ptr, data_offset << 12 | CWR << 7 | ECE << 6 | URG << 5 |
                                                      ACK << 4 | PSH << 3 | RST << 2 | SYN << 1 |
                                                      FIN);
                utils::produce(ptr, window);
                utils::produce(ptr, checksum);
                utils::produce(ptr, urgent_pointer);
                return ptr - f;
        }

        friend std::ostream& operator<<(std::ostream& out, tcp_header_t const& m) {
                out << "[TCP PACKET] ";
                out << m.src_port;
                out << " -> " << m.dst_port;
                out << " SEQ_NO: " << m.seq_no;
                out << " ACK_NO: " << m.ack_no;
                out << " [ ";
                if (m.ACK == 1) out << "ACK ";
                if (m.SYN == 1) out << "SYN ";
                if (m.PSH == 1) out << "PSH ";
                if (m.RST == 1) out << "RST ";
                if (m.FIN == 1) out << "FIN ";
                out << " ]";

                return out;
        }
};

}  // namespace mstack

template <>
struct fmt::formatter<mstack::tcp_header_t> : fmt::formatter<std::string> {
        auto format(mstack::tcp_header_t const& c, format_context& ctx) {
                return formatter<std::string>::format((std::ostringstream{} << c).str(), ctx);
        }
};
