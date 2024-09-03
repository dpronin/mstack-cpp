#pragma once

#include <string>

namespace mstack {

constexpr inline int TUNTAP_DEV = 0x01;

constexpr inline int TCP_CLOSED       = 0x10;
constexpr inline int TCP_LISTEN       = 0x11;
constexpr inline int TCP_SYN_SENT     = 0x12;
constexpr inline int TCP_SYN_RECEIVED = 0x13;
constexpr inline int TCP_ESTABLISHED  = 0x14;
constexpr inline int TCP_FIN_WAIT_1   = 0x15;
constexpr inline int TCP_FIN_WAIT_2   = 0x16;
constexpr inline int TCP_CLOSE_WAIT   = 0x17;
constexpr inline int TCP_CLOSING      = 0x18;
constexpr inline int TCP_LAST_ACK     = 0x19;
constexpr inline int TCP_TIME_WAIT    = 0x20;

constexpr inline int SOCKET_UNCONNECTED = 0x21;
constexpr inline int SOCKET_CONNECTING  = 0x22;
constexpr inline int SOCKET_CONNECTED   = 0x23;

inline std::string state_to_string(int state) {
        switch (state) {
                case TCP_CLOSED:
                        return "TCP_CLOSED";
                case TCP_LISTEN:
                        return "TCP_LISTEN";
                case TCP_SYN_SENT:
                        return "TCP_SYN_SENT";
                case TCP_SYN_RECEIVED:
                        return "TCP_SYN_RECEIVED";
                case TCP_ESTABLISHED:
                        return "TCP_ESTABLISHED";
                case TCP_FIN_WAIT_1:
                        return "TCP_FIN_WAIT_1";
                case TCP_FIN_WAIT_2:
                        return "TCP_FIN_WAIT_2";
                case TCP_CLOSE_WAIT:
                        return "TCP_CLOSE_WAIT";
                case TCP_CLOSING:
                        return "TCP_CLOSING";
                case TCP_LAST_ACK:
                        return "TCP_LAST_ACK";
                case TCP_TIME_WAIT:
                        return "TCP_TIME_WAIT";
        }
        return "UNKNOWN";
}

}  // namespace mstack
