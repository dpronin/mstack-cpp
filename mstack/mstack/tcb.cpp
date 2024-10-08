#include "tcb.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <concepts>
#include <functional>
#include <memory>
#include <queue>
#include <random>
#include <variant>
#include <vector>

#include <spdlog/spdlog.h>

#include <boost/circular_buffer.hpp>

#include "defination.hpp"
#include "ethernet_header.hpp"
#include "ipv4_header.hpp"
#include "skbuff.hpp"
#include "socket.hpp"
#include "tcb.hpp"
#include "tcb_manager.hpp"
#include "tcp_header.hpp"
#include "tcp_packet.hpp"
#include "utils.hpp"

namespace {

enum : uint8_t {
        kNOP  = 1,
        kMSS  = 2,
        kWS   = 3,
        kSACK = 4,
        kTSO  = 8,
};

struct nop {};

struct mss {
        uint16_t value;
};

struct window_scale {
        uint8_t shift_cnt;
};

struct sack {};

struct timestamp {
        uint32_t ts_value;
        uint32_t ts_echo_reply;
};

using tcp_options =
        std::vector<std::variant<std::monostate, mss, sack, timestamp, nop, window_scale>>;

tcp_options decode_options(std::span<std::byte const> buf) {
        tcp_options opts;

        while (!buf.empty()) {
                switch (auto const type{static_cast<uint8_t>(buf[0])}; type) {
                        case kNOP: {
                                buf = buf.subspan(1);
                        } break;
                        case kMSS: {
                                assert(!(buf.size() < 2));
                                auto const len{static_cast<uint8_t>(buf[1])};
                                assert(4 == len && !(buf.size() < len));
                                mss v;
                                std::memcpy(&v.value, &buf[2], 2);
                                mstack::utils::ntoh_inplace(v.value);
                                opts.push_back(v);
                                buf = buf.subspan(len);
                        } break;
                        case kWS: {
                                assert(!(buf.size() < 2));
                                auto const len{static_cast<uint8_t>(buf[1])};
                                assert(3 == len && !(buf.size() < len));
                                opts.push_back(window_scale{
                                        .shift_cnt = static_cast<uint8_t>(buf[3]),
                                });
                                buf = buf.subspan(len);
                        } break;
                        case kSACK: {
                                assert(!(buf.size() < 2));
                                auto const len{static_cast<uint8_t>(buf[1])};
                                assert(2 == len && !(buf.size() < len));
                                // skip this field
                                buf = buf.subspan(len);
                        } break;
                        case kTSO: {
                                assert(!(buf.size() < 2));
                                auto const len{static_cast<uint8_t>(buf[1])};
                                assert(10 == len && !(buf.size() < len));
                                // skip this field
                                buf = buf.subspan(len);
                        } break;
                        default:
                                return opts;
                }
        }

        return opts;
}

void encode_options(std::span<std::byte> buf, tcp_options const& opts) {
        for (auto const& opt : opts) {
                std::visit(
                        [&](auto const& opt) {
                                using type = std::decay_t<decltype(opt)>;
                                if constexpr (std::same_as<type, nop>) {
                                        assert(!buf.empty());

                                        buf[0] = static_cast<std::byte>(kNOP);

                                        buf = buf.subspan(1);
                                } else if constexpr (std::same_as<type, mss>) {
                                        assert(!(buf.size() < 4));

                                        buf[0] = static_cast<std::byte>(kMSS);
                                        buf[1] = static_cast<std::byte>(4);

                                        auto v{opt};
                                        mstack::utils::hton_inplace(v.value);
                                        std::memcpy(&buf[2], &v.value, 2);

                                        buf = buf.subspan(4);
                                } else if constexpr (std::same_as<type, window_scale>) {
                                        assert(!(buf.size() < 3));

                                        buf[0] = static_cast<std::byte>(kWS);
                                        buf[1] = static_cast<std::byte>(3);
                                        buf[2] = static_cast<std::byte>(opt.shift_cnt);

                                        buf = buf.subspan(3);

                                } else if constexpr (std::same_as<type, sack>) {
                                        assert(!(buf.size() < 2));

                                        buf[0] = static_cast<std::byte>(kSACK);
                                        buf[1] = static_cast<std::byte>(2);

                                        buf = buf.subspan(2);
                                } else if constexpr (std::same_as<type, timestamp>) {
                                        assert(false);
                                }
                        },
                        opt);
        }
}

}  // namespace

namespace mstack {

tcb_t::tcb_t(boost::asio::io_context&                  io_ctx,
             tcb_manager&                              mngr,
             endpoint const&                           remote_ep,
             endpoint const&                           local_ep,
             int                                       state,
             int                                       next_state,
             std::function<void(boost::system::error_code const& ec,
                                endpoint const&                  remote_ep,
                                endpoint const&                  local_ep,
                                std::weak_ptr<tcb_t>)> on_connection_established)
    : io_ctx_(io_ctx),
      mngr_(mngr),
      remote_ep_(remote_ep),
      local_ep_(local_ep),
      state_(state),
      next_state_(next_state),
      on_connection_established_(std::move(on_connection_established)) {
        assert(on_connection_established_);

        rcv_.state.window = 0xFAF0u;
        rcv_.pq           = std::make_unique<boost::circular_buffer<std::byte>>(rcv_.state.window);
}

void tcb_t::async_read_some(std::span<std::byte>                                          buf,
                            std::function<void(boost::system::error_code const&, size_t)> cb) {
        if (!rcv_.pq->empty()) {
                buf = buf.subspan(0, std::min(rcv_.pq->size(), buf.size()));
                std::copy_n(rcv_.pq->begin(), buf.size(), buf.begin());
                rcv_.pq->erase_begin(buf.size());
                rcv_.state.next += buf.size();
                io_ctx_.post([this, len = buf.size(), cb = std::move(cb)] {
                        cb({}, len);
                        make_and_send_pkt();
                });
        } else {
                on_data_receive_.push({
                        buf,
                        [cb = std::move(cb)](size_t nbytes) { cb({}, nbytes); },
                });
        }
}

void tcb_t::async_write(std::span<std::byte const>                                    buf,
                        std::function<void(boost::system::error_code const&, size_t)> cb) {
        enqueue_app_data(buf);
        io_ctx_.post([sz = buf.size(), cb = std::move(cb)] { cb({}, sz); });
}

void tcb_t::make_and_send_pkt() { enqueue(make_packet()); }

size_t tcb_t::app_data_unacknowleged() const {
        return std::min(send_.pq->size(),
                        static_cast<size_t>(send_.state.seq_nr_next - send_.state.seq_nr_unack));
}

size_t tcb_t::app_data_to_send_left() const {
        return std::min(static_cast<size_t>(send_.state.mss),
                        send_.pq->size() - app_data_unacknowleged());
}

bool tcb_t::has_app_data_to_send() const { return 0 != app_data_to_send_left(); }

void tcb_t::enqueue_app_data(std::span<std::byte const> pkt) {
        assert(!(send_.pq->size() + pkt.size() > send_.pq->capacity()));

        send_.pq->insert(send_.pq->end(), pkt.begin(), pkt.end());
        io_ctx_.post([this] {
                while (has_app_data_to_send())
                        make_and_send_pkt();
        });
}

void tcb_t::listen_finish() {
        io_ctx_.post([this] {
                on_connection_established_({}, remote_ep_, local_ep_, weak_from_this());
        });
}

tcp_packet tcb_t::make_packet() {
        auto const seg_len{app_data_to_send_left()};

        auto const additional_room{ethernetv2_header_t::size() + ipv4_header_t::size()};

        auto const room{additional_room + tcp_header_t::fixed_size() + seg_len};

        auto skb_out = skbuff{
                std::make_unique_for_overwrite<std::byte[]>(room),
                room,
                additional_room,
        };

        assert(0 == (tcp_header_t::fixed_size() & 0x3));

        auto const out_tcp = tcp_header_t{
                .src_port = local_ep_.addrv4_port,
                .dst_port = remote_ep_.addrv4_port,
                .seq_no   = 0 == seg_len ? send_.state.seq_nr_unack : send_.state.seq_nr_next,
                .ack_no   = rcv_.state.next,

                .data_offset = tcp_header_t::fixed_size() >> 2,
                .reserved    = 0,

                .CWR = 0,
                .ECE = 0,
                .URG = 0,
                .ACK = 1,
                .PSH = seg_len > 0,
                .RST = 0,
                .SYN = kTCPSynReceived == next_state_,
                .FIN = 0,

                .window         = rcv_.state.window,
                .chsum          = 0,
                .urgent_pointer = 0,
        };

        assert(!((send_.pq->begin() + app_data_unacknowleged() + seg_len) > send_.pq->end()));

        std::copy_n(send_.pq->begin() + app_data_unacknowleged(), seg_len,
                    out_tcp.produce_to_net(skb_out.head()));
        send_.state.seq_nr_next += seg_len;

        if (next_state_ != state_) state_ = next_state_;

        return {
                .proto     = tcb_manager::PROTO,
                .remote_ep = remote_ep_,
                .local_ep  = local_ep_,
                .skb       = std::move(skb_out),
        };
}

uint32_t tcb_t::generate_isn() { return std::random_device{}(); }

bool tcb_t::tcp_handle_close_state(tcp_header_t const& tcph) {
        assert(kTCPClosed == state_);

        /**
         *  If the state is CLOSED (i.e., TCB does not exist) then
         *  all data in the incoming segment is discarded.  An incoming
         *  segment containing a RST is discarded.
         */
        if (tcph.RST) {
                return true;
        }
        /**
         *  An incoming segment not
         *  containing a RST causes a RST to be sent in response.  The
         *  acknowledgment and sequence field values are selected to make the
         *  reset sequence acceptable to the TCP that sent the offending
         *  segment.
         *
         *  If the ACK bit is off, sequence number zero is used,
         *      <SEQ=0><ACK=SEG.SEQ+SEG.LEN><CTL=RST,ACK>
         */
        if (!tcph.ACK) {
                return true;
        }

        /**
         *  If the ACK bit is on,
         *       <SEQ=SEG.ACK><CTL=RST>. Return.
         */

        if (tcph.ACK) {
                return true;
        }

        return false;
}

bool tcb_t::tcp_handle_listen_state(tcp_header_t const& tcph, std::span<std::byte const> opts) {
        //  If the state is LISTEN then
        assert(kTCPListen == state_);

        spdlog::debug("[TCP LISTEN] {}", tcph);

        /**
         *  first check for an RST
         *  An incoming RST should be ignored.  Return.
         */
        if (tcph.RST) {
                return true;
        }

        /**
         *  TODO: second check for an ACK
         *  Any acknowledgment is bad if it arrives on a connection still in
         *  the LISTEN state.  An acceptable reset segment should be formed
         *  for any arriving ACK-bearing segment.  The RST should be
         *  formatted as follows: <SEQ=SEG.ACK><CTL=RST>
         */
        if (tcph.ACK) {
                // TOSO: Send RST
                return true;
        }

        /**
         *  TODO: third check for a SYN
         *  If the SYN bit is set, check the security.  If the
         *  security/compartment on the incoming segment does not exactly
         *  match the security/compartment in the TCB then send a reset and
         *  return. <SEQ=SEG.ACK><CTL=RST>
         *
         *  If the SEG.PRC is greater than the TCB.PRC then if allowed by
         *  the user and the system set TCB.PRC<-SEG.PRC, if not allowed
         *  send a reset and return. <SEQ=SEG.ACK><CTL=RST>
         *
         *  If the SEG.PRC is less than the TCB.PRC then continue.
         */
        if (tcph.SYN) {
        }
        /**
         *  Set RCV.NXT to SEG.SEQ+1, IRS is set to SEG.SEQ and any other
         *  control or text should be queued for processing later.  ISN
         *  should be selected and a SYN segment sent of the form:
         *  <SEQ=ISN><ACK=RCV.NXT><CTL=SYN,ACK>
         *
         *  SND.NXT is set to ISN+1 and SND.UNA to ISN.  The connection
         *  state should be changed to SYN-RECEIVED.  Note that any other
         *  incoming control or data (combined with SYN) will be processed
         *  in the SYN-RECEIVED state, but processing of SYN and ACK should
         *  not be repeated.  If the listen was not fully specified (i.e.,
         *  the foreign socket was not fully specified), then the
         *  unspecified fields should be filled in now.
         */

        if (tcph.SYN) {
                for (auto const& opt : decode_options(opts)) {
                        std::visit(
                                [this](auto const& opt) {
                                        using type = std::decay_t<decltype(opt)>;
                                        if constexpr (std::same_as<type, nop>) {
                                        } else if constexpr (std::same_as<type, mss>) {
                                                send_.state.mss = opt.value;
                                        } else if constexpr (std::same_as<type, window_scale>) {
                                        } else if constexpr (std::same_as<type, sack>) {
                                        } else if constexpr (std::same_as<type, timestamp>) {
                                        }
                                },
                                opt);
                }

                uint32_t const isn{generate_isn()};

                rcv_.state.next    = tcph.seq_no + 1;
                send_.state.window = tcph.window;
                send_.pq = std::make_unique<boost::circular_buffer<std::byte>>(send_.state.window);
                send_.state.seq_nr_next  = isn + 1;
                send_.state.seq_nr_unack = isn;
                next_state_              = kTCPSynReceived;
                make_and_send_pkt();

                spdlog::debug("[SEND SYN ACK]");

                return true;
        }

        /**
         *  TODO: fourth other text or control
         *  must have an ACK and thus would be discarded by the ACK
         *  processing.  An incoming RST segment could not be valid, since
         *  it could not have been sent in response to anything sent by this
         *  incarnation of the connection.  So you are unlikely to get here,
         *  but if you do, drop the segment, and return.
         */
        return true;
}

bool tcb_t::tcp_handle_syn_sent(tcp_header_t const& tcph, std::span<std::byte const> opts) {
        // If the state is SYN-SENT then
        assert(kTCPSynSent == state_);

        // first check the ACK bit
        if (tcph.SYN && tcph.ACK) {
                if (!(tcph.ack_no < send_.state.seq_nr_unack) &&
                    !(tcph.ack_no > send_.state.seq_nr_next)) {
                        rcv_.state.next    = tcph.seq_no + 1;
                        send_.state.window = tcph.window;
                        send_.pq           = std::make_unique<boost::circular_buffer<std::byte>>(
                                send_.state.window);
                        send_.state.seq_nr_unack = tcph.ack_no;

                        for (auto const& opt : decode_options(opts)) {
                                std::visit(
                                        [this](auto const& opt) {
                                                using type = std::decay_t<decltype(opt)>;
                                                if constexpr (std::same_as<type, nop>) {
                                                } else if constexpr (std::same_as<type, mss>) {
                                                        send_.state.mss = opt.value;
                                                } else if constexpr (std::same_as<type,
                                                                                  window_scale>) {
                                                } else if constexpr (std::same_as<type, sack>) {
                                                } else if constexpr (std::same_as<type,
                                                                                  timestamp>) {
                                                }
                                        },
                                        opt);
                        }

                        state_      = kTCPEstablished;
                        next_state_ = kTCPEstablished;

                        make_and_send_pkt();

                        listen_finish();
                } else {
                        // TODO: send RST
                        return true;
                }
                /**
                 *  If SEG.ACK =< ISN, or SEG.ACK > SND.NXT, send a reset (unless
                 *  the RST bit is set, if so drop the segment and return)
                 *      <SEQ=SEG.ACK><CTL=RST>
                 *  and discard the segment.  Return.
                 *  If SND.UNA =< SEG.ACK =< SND.NXT then the ACK is acceptable.
                 */
        }

        // second check the RST bit
        if (tcph.RST) {
                /**
                 *  If the ACK was acceptable then signal the user "error:
                 *  connection reset", drop the segment, enter CLOSED state,
                 *  delete TCB, and return.  Otherwise (no ACK) drop the segment
                 *  and return.
                 */
        }

        // third check the security and precedence
        /**
         *  If the security/compartment in the segment does not exactly
         *  match the security/compartment in the TCB, send a reset
         *
         *  If there is an ACK
         *
         *      <SEQ=SEG.ACK><CTL=RST>
         *
         *  Otherwise
         *
         *      <SEQ=0><ACK=SEG.SEQ+SEG.LEN><CTL=RST,ACK>
         *
         *  If there is an ACK
         *
         *  The precedence in the segment must match the precedence in the
         *  TCB, if not, send a reset
         *
         *      <SEQ=SEG.ACK><CTL=RST>
         *
         *  If there is no ACK
         *
         *  If the precedence in the segment is higher than the precedence
         *  in the TCB then if allowed by the user and the system raise
         *  the precedence in the TCB to that in the segment, if not
         *  allowed to raise the prec then send a reset.
         *
         *      <SEQ=0><ACK=SEG.SEQ+SEG.LEN><CTL=RST,ACK>
         *
         *  If the precedence in the segment is lower than the precedence
         *  in the TCB continue.
         *
         *  If a reset was sent, discard the segment and return.
         */

        // fourth check the SYN bit

        /**
         *   This step should be reached only if the ACK is ok, or there is
         *   no ACK, and it the segment did not contain a RST.
         *
         *   If the SYN bit is on and the security/compartment and precedence
         *   are acceptable then, RCV.NXT is set to SEG.SEQ+1, IRS is set to
         *   SEG.SEQ.  SND.UNA should be advanced to equal SEG.ACK (if there
         *   is an ACK), and any segments on the retransmission queue which
         *   are thereby acknowledged should be removed.
         *
         *   If SND.UNA > ISN (our SYN has been ACKed), change the connection
         *   state to ESTABLISHED, form an ACK segment
         *
         *      <SEQ=SND.NXT><ACK=RCV.NXT><CTL=ACK>
         *
         *   and send it.  Data or controls which were queued for
         *   transmission may be included.  If there are other controls or
         *   text in the segment then continue processing at the sixth step
         *   below where the URG bit is checked, otherwise return.
         *
         *   Otherwise enter SYN-RECEIVED, form a SYN,ACK segment
         *
         *      <SEQ=ISN><ACK=RCV.NXT><CTL=SYN,ACK>
         *
         *   and send it.  If there are other controls or text in the
         *   segment, queue them for processing after the ESTABLISHED state
         *   has been reached, return.
         */

        // fifth, if neither of the SYN or RST bits is set then drop the segment
        // and return.
        return true;
}

bool tcb_t::tcp_check_segment(tcp_header_t const& tcph, uint16_t seglen) {
        /**
         *  SYN-RECEIVED STATE
         *  ESTABLISHED STATE
         *  FIN-WAIT-1 STATE
         *  FIN-WAIT-2 STATE
         *  CLOSE-WAIT STATE
         *  CLOSING STATE
         *  LAST-ACK STATE
         *  TIME-WAIT STATE
         *
         *      Segments are processed in sequence.  Initial tests on arrival
         *      are used to discard old duplicates, but further processing is
         *      done in SEG.SEQ order.  If a segment's contents straddle the
         *      boundary between old and new, only the new parts should be
         *      processed.
         *
         *      There are four cases for the acceptability test for an incoming
         *      segment:
         *
         *      Segment Receive  Test
         *      Length  Window
         *      ------- -------  -------------------------------------------
         *
         *      0       0     SEG.SEQ = RCV.NXT
         *
         *      0      >0     RCV.NXT =< SEG.SEQ < RCV.NXT+RCV.WND
         *
         *      >0       0     not acceptable
         *
         *      >0      >0     RCV.NXT =< SEG.SEQ < RCV.NXT+RCV.WND
         *                  or RCV.NXT =< SEG.SEQ+SEG.LEN-1 < RCV.NXT+RCV.WND
         *
         *      If the RCV.WND is zero, no segments will be acceptable, but
         *      special allowance should be made to accept valid ACKs, URGs and
         *      RSTs.
         *
         *      If an incoming segment is not acceptable, an acknowledgment
         *      should be sent in reply (unless the RST bit is set, if so drop
         *      the segment and return):
         *
         *      <SEQ=SND.NXT><ACK=RCV.NXT><CTL=ACK>
         *
         *      After sending the acknowledgment, drop the unacceptable segment
         *      and return.
         *      In the following it is assumed that the segment is the idealized
         *      segment that begins at RCV.NXT and does not exceed the window.
         *      One could tailor actual segments to fit this assumption by
         *      trimming off any portions that lie outside the window (including
         *      SYN and FIN), and only processing further if the segment then
         *      begins at RCV.NXT.  Segments with higher begining sequence
         *      numbers may be held for later processing.
         */

        switch (state_) {
                case kTCPSynReceived:
                case kTCPEstablished:
                case kTCPFinWait_1:
                case kTCPFinWait_2:
                case kTCPCloseWait:
                case kTCPClosing:
                case kTCPLastAck:
                case kTCPTimeWait:
                        if (seglen == 0 && rcv_.state.window == 0)
                                return tcph.seq_no == rcv_.state.next;

                        if (seglen == 0 && rcv_.state.window > 0) {
                                // RCV.NXT =< SEG.SEQ < RCV.NXT+RCV.WND
                                return rcv_.state.next <= tcph.seq_no &&
                                       tcph.seq_no < rcv_.state.next + rcv_.state.window;
                        }

                        if (seglen > 0 && rcv_.state.window == 0) {
                                return false;
                        }

                        if (seglen > 0 && rcv_.state.window > 0) {
                                // RCV.NXT =< SEG.SEQ < RCV.NXT+RCV.WND
                                //        or RCV.NXT =< SEG.SEQ+SEG.LEN-1 < RCV.NXT +
                                //        RCV.WND
                                return (rcv_.state.next <= tcph.seq_no &&
                                        rcv_.state.next + rcv_.state.window) ||
                                       (rcv_.state.next <= tcph.seq_no + seglen - 1 &&
                                        tcph.seq_no + seglen - 1 <
                                                rcv_.state.next + rcv_.state.window);
                        }
        }

        return false;
}

void tcb_t::process(tcp_packet&& pkt_in) {
        assert(!(pkt_in.skb.payload().size() < tcp_header_t::fixed_size()));
        auto const tcph{tcp_header_t::consume_from_net(pkt_in.skb.head())};

        auto const hlen{tcph.data_offset << 2};
        auto const optlen{hlen - tcp_header_t::fixed_size()};
        pkt_in.skb.pop_front(hlen - optlen);

        auto const opts{std::as_bytes(pkt_in.skb.payload().subspan(0, optlen))};
        pkt_in.skb.pop_front(optlen);

        auto const segment{std::as_bytes(pkt_in.skb.payload())};

        spdlog::debug("[TCP] RECEIVE h={}, hlen={}, optlen={}, seglen={}", tcph, hlen, optlen,
                      segment.size());

        spdlog::debug("[TCP] [CHECK TCP_CLOSED] {}", *this);
        if (state_ == kTCPClosed && tcp_handle_close_state(tcph)) return;

        spdlog::debug("[TCP] [CHECK TCP_LISTEN] {}", *this);
        if (state_ == kTCPListen && tcp_handle_listen_state(tcph, opts)) return;

        spdlog::debug("[TCP] [CHECK TCP_SYN_SENT] {}", *this);
        if (state_ == kTCPSynSent && tcp_handle_syn_sent(tcph, opts)) return;

        // first check sequence number
        if (!tcp_check_segment(tcph, segment.size())) {
                spdlog::warn("[SEGMENT SEQ FAIL]");
                if (!tcph.RST) {
                        // <SEQ=SND.NXT><ACK=RCV.NXT><CTL=ACK>
                        // TODO: send ACK
                }
                return;
        }

        // TODO: second check the RST bit
        if (tcph.RST) {
                switch (state_) {
                        /**
                         *  SYN-RECEIVED STATE
                         *      If the RST bit is set
                         *      If this connection was initiated with a passive OPEN
                         (i.e.,
                         *      came from the LISTEN state), then return this connection
                         to
                         *      LISTEN state and return.  The user need not be informed.
                         If
                         *      this connection was initiated with an active OPEN (i.e.,
                         * came from SYN-SENT state) then the connection was refused,
                         signal
                         *      the user "connection refused".  In either case, all
                         segments
                         *      on the retransmission queue should be removed.  And in
                         the
                         *      active OPEN case, enter the CLOSED state and delete the
                         TCB,
                         *      and return.
                         */
                        case kTCPSynReceived:
                                return;
                        /**
                         *  ESTABLISHED
                         *  FIN-WAIT-1
                         *  FIN-WAIT-2
                         *  CLOSE-WAIT
                         *      If the RST bit is set then, any outstanding RECEIVEs and
                         * SEND should receive "reset" responses.  All segment queues
                         should
                         * be flushed.  Users should also receive an unsolicited general
                         *      "connection reset" signal.  Enter the CLOSED state,
                         delete
                         * the TCB, and return.
                         */
                        case kTCPEstablished:
                        case kTCPFinWait_1:
                        case kTCPFinWait_2:
                        case kTCPCloseWait:
                                return;
                        /**
                         *  CLOSING STATE
                         *  LAST-ACK STATE
                         *  TIME-WAIT
                         *      If the RST bit is set then, enter the CLOSED state,
                         delete
                         * the TCB, and return.
                         */
                        case kTCPClosing:
                        case kTCPLastAck:
                        case kTCPTimeWait:
                                return;
                }
        }

        // TODO: third check security and precedence
        /**
         *  SYN-RECEIVED
         *
         *      If the security/compartment and precedence in the segment do not
         *      exactly match the security/compartment and precedence in the TCB
         *      then send a reset, and return.
         *
         *  ESTABLISHED STATE
         *
         *      If the security/compartment and precedence in the segment do not
         *      exactly match the security/compartment and precedence in the TCB
         *      then send a reset, any outstanding RECEIVEs and SEND should
         *      receive "reset" responses.  All segment queues should be
         *      flushed.  Users should also receive an unsolicited general
         *      "connection reset" signal.  Enter the CLOSED state, delete the
         *      TCB, and return.
         *
         *  Note this check is placed following the sequence check to prevent
         *  a segment from an old connection between these ports with a
         *  different security or precedence from causing an abort of the
         *  current connection.
         */

        // TODO: fourth, check the SYN bit
        if (tcph.SYN) {
                switch (state_) {
                                /**
                                 *  SYN-RECEIVED
                                 *  ESTABLISHED STATE
                                 *  FIN-WAIT STATE-1
                                 *  FIN-WAIT STATE-2
                                 *  CLOSE-WAIT STATE
                                 *  CLOSING STATE
                                 *  LAST-ACK STATE
                                 *  TIME-WAIT STATE
                                 *
                                 *      If the SYN is in the window it is an error, send a
                                 *      reset, any outstanding RECEIVEs and SEND should
                                 * receive "reset" responses, all segment queues should be
                                 * flushed, the user should also receive an unsolicited
                                 * general "connection reset" signal, enter the CLOSED
                                 * state, delete the TCB, and return.
                                 *
                                 *      If the SYN is not in the window this step would not
                                 *      be reached and an ack would have been sent in the
                                 * first step (sequence number check).
                                 */
                        case kTCPSynReceived:
                        case kTCPEstablished:
                        case kTCPFinWait_1:
                        case kTCPFinWait_2:
                        case kTCPCloseWait:
                        case kTCPClosing:
                        case kTCPLastAck:
                        case kTCPTimeWait:
                                return;
                }
        }

        // fifth check the ACK field
        if (tcph.ACK) {
                switch (state_) {
                        /**
                         *  SYN-RECEIVED STATE
                         *      If SND.UNA =< SEG.ACK =< SND.NXT then enter ESTABLISHED
                         *      state and continue processing. If the segment acknowledgment
                         * is not acceptable, form a reset segment, <SEQ=SEG.ACK><CTL=RST>
                         */
                        case kTCPSynReceived:
                                if (send_.state.seq_nr_unack <= tcph.ack_no &&
                                    tcph.ack_no <= send_.state.seq_nr_next) {
                                        send_.state.seq_nr_unack =
                                                std::clamp(tcph.ack_no, send_.state.seq_nr_unack,
                                                           send_.state.seq_nr_next);
                                        state_      = kTCPEstablished;
                                        next_state_ = kTCPEstablished;
                                        listen_finish();
                                } else {
                                        // TODO: send RST
                                        return;
                                }
                                break;
                        /**
                         *  ESTABLISHED STATE
                         *      If SND.UNA < SEG.ACK =< SND.NXT then, set SND.UNA <-
                         *      SEG.ACK. Any segments on the retransmission queue which are
                         *      thereby entirely acknowledged are removed.  Users should
                         * receive positive acknowledgments for buffers which have been SENT
                         *      and fully acknowledged (i.e., SEND buffer should be returned
                         * with "ok" response).  If the ACK is a duplicate (SEG.ACK <
                         * SND.UNA), it can be ignored.  If the ACK acks something not yet
                         * sent (SEG.ACK > SND.NXT) then send an ACK, drop the segment, and
                         * return.
                         *
                         *      If SND.UNA < SEG.ACK =< SND.NXT, the send window should be
                         *      updated.  If (SND.WL1 < SEG.SEQ or (SND.WL1 = SEG.SEQ and
                         *      SND.WL2 =< SEG.ACK)), set SND.WND <- SEG.WND, set
                         *      SND.WL1 <- SEG.SEQ, and set SND.WL2 <- SEG.ACK.
                         *
                         *      Note that SND.WND is an offset from SND.UNA, that SND.WL1
                         *      records the sequence number of the last segment used to
                         *      update SND.WND, and that SND.WL2 records the acknowledgment
                         *      number of the last segment used to update SND.WND.  The
                         * check here prevents using old segments to update the window.
                         */
                        case kTCPEstablished:
                        case kTCPFinWait_1:
                        case kTCPFinWait_2:
                        case kTCPCloseWait:
                        case kTCPClosing:
                                send_.pq->erase_begin(
                                        std::min(send_.pq->size(),
                                                 static_cast<size_t>(tcph.ack_no -
                                                                     send_.state.seq_nr_unack)));

                                send_.state.seq_nr_unack =
                                        std::clamp(tcph.ack_no, send_.state.seq_nr_unack,
                                                   send_.state.seq_nr_next);

                                if (tcph.ack_no > send_.state.seq_nr_next) {
                                        make_and_send_pkt();
                                        return;
                                }

                                /**
                                 *  FIN-WAIT-1 STATE
                                 *      In addition to the processing for the ESTABLISHED
                                 *      state, if our FIN is now acknowledged then enter
                                 *      FIN-WAIT-2 and continue processing in that state.
                                 */
                                if (state_ == kTCPFinWait_1) {
                                        next_state_ = kTCPFinWait_2;
                                }

                                /**
                                 *  FIN-WAIT-2 STATE
                                 *      In addition to the processing for the ESTABLISHED
                                 *      state, if the retransmission queue is empty, the
                                 * user's CLOSE can be acknowledged ("ok") but do not delete
                                 * the TCB.
                                 */
                                if (state_ == kTCPFinWait_2) {
                                        // * CLOSE FINISH
                                }
                                /**
                                 *  CLOSE-WAIT STATE
                                 *      Do the same processing as for the ESTABLISHED state.
                                 */

                                if (state_ == kTCPCloseWait) {
                                }
                                /**
                                 *  CLOSING STATE
                                 *      In addition to the processing for the ESTABLISHED
                                 *      state, if the ACK acknowledges our FIN then enter
                                 * the TIME-WAIT state, otherwise ignore the segment.
                                 */
                                if (state_ == kTCPClosing) {
                                        next_state_ = kTCPTimeWait;
                                }
                                break;
                        /**
                         *  LAST-ACK STATE
                         *      The only thing that can arrive in this state is an
                         *      acknowledgment of our FIN.  If our FIN is now  acknowledged,
                         *      delete the TCB, enter the CLOSED state, and return.
                         */
                        case kTCPLastAck:
                                next_state_ = kTCPClosed;
                                return;
                        /**
                         *  TIME-WAIT STATE
                         *      The only thing that can arrive in this state is a
                         *      retransmission of the remote FIN.  Acknowledge it, and
                         *      restart the 2 MSL timeout.
                         */
                        case kTCPTimeWait:
                                make_and_send_pkt();
                                break;
                }
        }

        // TODO: sixth, check the URG bit
        if (tcph.URG) {
                switch (state_) {
                        /**
                         *  ESTABLISHED STATE
                         *  FIN-WAIT-1 STATE
                         *  FIN-WAIT-2 STATE
                         *      If the URG bit is set, RCV.UP <- max(RCV.UP,SEG.UP), and
                         * signal the user that the remote side has urgent data if the
                         * urgent pointer (RCV.UP) is in advance of the data consumed. If
                         * the user has already been signaled (or is still in the "urgent
                         *      mode") for this continuous sequence of urgent data, do
                         not
                         *      signal the user again.
                         */
                        case kTCPEstablished:
                        case kTCPFinWait_1:
                        case kTCPFinWait_2:

                        /**
                         *  CLOSE-WAIT STATE
                         *  CLOSING STATE
                         *  LAST-ACK STATE
                         *  TIME-WAIT
                         *      This should not occur, since a FIN has been received from
                         * the remote side.  Ignore the URG.
                         */
                        case kTCPCloseWait:
                        case kTCPClosing:
                        case kTCPLastAck:
                        case kTCPTimeWait:
                                break;
                }
        }

        // seventh, process the segment text
        if (segment.size() > 0) {
                switch (state_) {
                        /**
                         *   ESTABLISHED STATE
                         *   FIN-WAIT-1 STATE
                         *   FIN-WAIT-2 STATE
                         *
                         *      Once in the ESTABLISHED state, it is possible to deliver
                         *      segment text to user RECEIVE buffers.  Text from segments
                         *      can be moved into buffers until either the buffer is full or
                         * the segment is empty.  If the segment empties and carries an PUSH
                         *      flag, then the user is informed, when the buffer is
                         * returned, that a PUSH has been received.
                         *
                         *      When the TCP takes responsibility for delivering the data
                         *      to the user it must also acknowledge the receipt of the
                         * data.
                         *
                         *      Once the TCP takes responsibility for the data it advances
                         *      RCV.NXT over the data accepted, and adjusts RCV.WND as
                         *      apporopriate to the current buffer availability.  The total
                         *      of RCV.NXT and RCV.WND should not be reduced.
                         *
                         *      Please note the window management suggestions in
                         *              section 3.7.
                         *
                         *      Send an acknowledgment of the form:
                         *
                         *              <SEQ=SND.NXT><ACK=RCV.NXT><CTL=ACK>
                         *
                         *      This acknowledgment should be piggybacked on a segment
                         *      being transmitted if possible without incurring undue delay
                         */
                        case kTCPEstablished:
                        case kTCPFinWait_1:
                        case kTCPFinWait_2: {
                                spdlog::debug("[TCP] RECEIVE DATA {}", segment.size());

                                if (!(tcph.seq_no < rcv_.state.next + rcv_.pq->size())) {
                                        if (!on_data_receive_.empty()) {
                                                assert(rcv_.pq->empty());

                                                auto [buf, cb] =
                                                        std::move(on_data_receive_.front());
                                                on_data_receive_.pop();

                                                buf = buf.subspan(
                                                        0, std::min(segment.size(), buf.size()));

                                                std::ranges::copy(segment.subspan(0, buf.size()),
                                                                  buf.begin());
                                                rcv_.state.next += buf.size();

                                                rcv_.pq->resize(segment.size() - buf.size());

                                                std::ranges::copy(segment.subspan(buf.size()),
                                                                  rcv_.pq->begin());

                                                io_ctx_.post([this, len = buf.size(),
                                                              cb = std::move(cb)] {
                                                        cb(len);
                                                        make_and_send_pkt();
                                                });
                                        } else {
                                                assert(!(rcv_.pq->size() + segment.size() >
                                                         rcv_.pq->capacity()));

                                                rcv_.pq->resize(rcv_.pq->size() + segment.size());
                                                std::ranges::copy(segment,
                                                                  rcv_.pq->end() - segment.size());
                                        }
                                }

                                break;
                        }
                        /**
                         *  CLOSE-WAIT STATE
                         *  CLOSING STATE
                         *  LAST-ACK STATE
                         *  TIME-WAIT STATE
                         *      This should not occur, since a FIN has been received from
                         *      the remote side.  Ignore the segment text.
                         */
                        case kTCPCloseWait:
                        case kTCPClosing:
                        case kTCPLastAck:
                        case kTCPTimeWait:
                                break;
                }
        }

        // eighth, check the FIN bit
        if (tcph.FIN) {
                switch (state_) {
                        /**
                         *      Do not process the FIN if the state is CLOSED, LISTEN or
                         *      SYN-SENT since the SEG.SEQ cannot be validated; drop the
                         *      segment and return.
                         *
                         *      If the FIN bit is set, signal the user "connection
                         *      closing" and return any pending RECEIVEs with same
                         *      message, advance RCV.NXT over the FIN, and send an
                         *      acknowledgment for the FIN.  Note that FIN implies PUSH
                         *      for any segment text not yet delivered to the user.
                         */

                        /**
                         *      SYN-RECEIVED STATE
                         *      ESTABLISHED STATE
                         *              Enter the CLOSE-WAIT state.
                         */
                        case kTCPSynReceived:
                        case kTCPEstablished:
                                rcv_.state.next += 1;
                                next_state_ = kTCPCloseWait;
                                make_and_send_pkt();
                                [[fallthrough]];
                        /**
                         *  FIN-WAIT-1 STATE
                         *      If our FIN has been ACKed (perhaps in this segment),
                         *      then enter TIME-WAIT, start the time-wait timer,
                         * turn off the other timers; otherwise enter the CLOSING
                         * state.
                         */
                        case kTCPFinWait_1:
                                if (next_state_ == kTCPFinWait_2) {
                                        next_state_ = kTCPTimeWait;
                                } else {
                                        next_state_ = kTCPClosing;
                                }
                                [[fallthrough]];
                                /**
                                 *  FIN-WAIT-2 STATE
                                 *      Enter the TIME-WAIT state.  Start the time-wait
                                 * timer, turn off the other timers.
                                 */
                        case kTCPFinWait_2:
                                // * start time_wait
                                next_state_ = kTCPTimeWait;
                                [[fallthrough]];
                                /**
                                 *  CLOSE-WAIT STATE
                                 *      Remain in the CLOSE-WAIT state.
                                 */
                        case kTCPCloseWait:
                                /**
                                 *  CLOSING STATE
                                 *      Remain in the CLOSING state.
                                 */
                        case kTCPClosing:
                                /**
                                 *  LAST-ACK STATE
                                 *      Remain in the LAST-ACK state.
                                 */
                        case kTCPLastAck:
                                /**
                                 *  TIME-WAIT STATE
                                 *      Remain in the TIME-WAIT state.  Restart the 2 MSL
                                 * time-wait timeout.
                                 */
                        case kTCPTimeWait:
                                return;
                }
        }
}

void tcb_t::enqueue(tcp_packet&& out_pkt) {
        spdlog::debug("[TCP] ENQUEUE {}", out_pkt);
        mngr_.enqueue(std::move(out_pkt));
}

void tcb_t::start_connecting() {
        rcv_.state.mss = 1446;  // due to MTU being 1500 by default

        auto const opts{
                tcp_options{
                        {
                                mss{.value = rcv_.state.mss},
                                nop{},
                                nop{},
                                sack{},
                        },
                },
        };

        auto const additional_room{ethernetv2_header_t::size() + ipv4_header_t::size()};

        auto const room{additional_room + tcp_header_t::fixed_size() + 8};

        auto skb_out = skbuff{
                std::make_unique_for_overwrite<std::byte[]>(room),
                room,
                additional_room,
        };

        assert(0 == (tcp_header_t::fixed_size() & 0x3));

        auto const out_tcp = tcp_header_t{
                .src_port    = local_ep_.addrv4_port,
                .dst_port    = remote_ep_.addrv4_port,
                .seq_no      = generate_isn(),
                .ack_no      = 0,
                .data_offset = static_cast<uint16_t>(skb_out.payload().size() >> 2),
                .reserved    = 0,

                .CWR = 0,
                .ECE = 0,
                .URG = 0,
                .ACK = 0,
                .PSH = 0,
                .RST = 0,
                .SYN = 1,
                .FIN = 0,

                .window         = rcv_.state.window,
                .chsum          = 0,
                .urgent_pointer = 0,
        };

        send_.state.seq_nr_unack = out_tcp.seq_no;
        send_.state.seq_nr_next  = out_tcp.seq_no + 1;

        encode_options({out_tcp.produce_to_net(skb_out.head()), 8}, opts);

        state_      = kTCPSynSent;
        next_state_ = kTCPEstablished;

        enqueue({
                .proto     = 0x06,
                .remote_ep = remote_ep_,
                .local_ep  = local_ep_,
                .skb       = std::move(skb_out),
        });
}

}  // namespace mstack
