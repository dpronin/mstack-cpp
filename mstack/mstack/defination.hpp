#pragma once

#include <string>

namespace mstack {

enum {
        kTCPClosed,
        kTCPConnecting,
        kTCPListen,
        kTCPSynSent,
        kTCPSynReceived,
        kTCPEstablished,
        kTCPFinWait_1,
        kTCPFinWait_2,
        kTCPCloseWait,
        kTCPClosing,
        kTCPLastAck,
        kTCPTimeWait,
        kSocketDisconnected,
        kSocketConnecting,
        kSocketConnected,
};

inline std::string state_to_string(int state) {
        switch (state) {
                case kTCPClosed:
                        return "TCP_CLOSED";
                case kTCPConnecting:
                        return "TCP_CONNECTING";
                case kTCPListen:
                        return "TCP_LISTEN";
                case kTCPSynSent:
                        return "TCP_SYN_SENT";
                case kTCPSynReceived:
                        return "TCP_SYN_RECEIVED";
                case kTCPEstablished:
                        return "TCP_ESTABLISHED";
                case kTCPFinWait_1:
                        return "TCP_FIN_WAIT_1";
                case kTCPFinWait_2:
                        return "TCP_FIN_WAIT_2";
                case kTCPCloseWait:
                        return "TCP_CLOSE_WAIT";
                case kTCPClosing:
                        return "TCP_CLOSING";
                case kTCPLastAck:
                        return "TCP_LAST_ACK";
                case kTCPTimeWait:
                        return "TCP_TIME_WAIT";
        }
        return "UNKNOWN";
}

}  // namespace mstack
