#pragma once

#include <cassert>

#include <concepts>
#include <functional>
#include <optional>
#include <unordered_map>
#include <utility>

#include <boost/asio/io_context.hpp>

#include <spdlog/spdlog.h>

#include "circle_buffer.hpp"
#include "raw_packet.hpp"

namespace mstack {

template <typename UnderPacketType, typename UpperPacketType>
class base_protocol {
private:
        boost::asio::io_context&                                      io_ctx_;
        std::unordered_map<int, std::function<void(UpperPacketType)>> upper_protos_;
        std::function<void(UnderPacketType)>                          under_proto_;
        circle_buffer<UnderPacketType>                                packet_queue_;

public:
        template <std::convertible_to<int> Index, typename UpperProto>
        void upper_proto_update(Index id, UpperProto& proto) {
                upper_protos_[id] = [&proto](UpperPacketType&& packet) {
                        proto.receive(std::move(packet));
                };
        }

        template <typename UnderProtocol>
        void under_proto_update(UnderProtocol& proto) {
                under_proto_ = [&proto](UnderPacketType&& pkt) { proto.receive(std::move(pkt)); };
        }

        void receive(UpperPacketType&& in) { process(std::move(in)); }

        void receive(UnderPacketType&& in) {
                if (auto out{make_packet(std::move(in))}) dispatch(std::move(*out));
        }

protected:
        explicit base_protocol(boost::asio::io_context& io_ctx) : io_ctx_(io_ctx) {}
        ~base_protocol() = default;

        base_protocol(base_protocol const&)            = delete;
        base_protocol& operator=(base_protocol const&) = delete;

        base_protocol(base_protocol&&)            = delete;
        base_protocol& operator=(base_protocol&&) = delete;

        void enqueue(UnderPacketType&& pkt) {
                packet_queue_.push(std::move(pkt));
                io_ctx_.post([this] {
                        if (under_proto_) under_proto_(packet_queue_.pop().value());
                });
        }

        virtual void process(UpperPacketType&& in_pkt [[maybe_unused]]) = 0;

        virtual std::optional<UpperPacketType> make_packet(UnderPacketType&& in_pkt
                                                           [[maybe_unused]]) {
                return std::nullopt;
        }

        virtual std::optional<UpperPacketType> make_packet(raw_packet&& in_pkt [[maybe_unused]]) {
                return std::nullopt;
        }

private:
        void dispatch(UpperPacketType&& in_pkt) {
                if (auto prot_it{upper_protos_.find(in_pkt.proto)};
                    upper_protos_.end() != prot_it) {
                        spdlog::debug("[PROCESS PACKET] PROTO {:#04X}", in_pkt.proto);
                        prot_it->second(std::move(in_pkt));
                } else {
                        spdlog::debug("[UNKNOWN PACKET] PROTO {:#04X}", in_pkt.proto);
                }
        }
};

template <typename UpperPacketType>
class base_protocol<void, UpperPacketType> {
private:
        boost::asio::io_context&                                      io_ctx_;
        std::unordered_map<int, std::function<void(UpperPacketType)>> upper_protos_;
        std::function<void(raw_packet&&)>                             under_proto_;
        circle_buffer<raw_packet>                                     packet_queue_;

public:
        template <std::convertible_to<int> Index, typename UpperProto>
        void upper_proto_update(Index id, UpperProto& proto) {
                upper_protos_[id] = [&proto](UpperPacketType&& packet) {
                        proto.receive(std::move(packet));
                };
        }

        void under_handler_update(std::function<void(raw_packet&&)> handler) {
                under_proto_ = std::move(handler);
        }

        void receive(UpperPacketType&& in) { process(std::move(in)); }

        void receive(raw_packet&& in) {
                if (auto out{make_packet(std::move(in))}) dispatch(std::move(*out));
        }

protected:
        explicit base_protocol(boost::asio::io_context& io_ctx) : io_ctx_(io_ctx) {}
        ~base_protocol() = default;

        base_protocol(base_protocol const&)            = delete;
        base_protocol& operator=(base_protocol const&) = delete;

        base_protocol(base_protocol&&)            = delete;
        base_protocol& operator=(base_protocol&&) = delete;

        void enqueue(raw_packet&& pkt) {
                packet_queue_.push(std::move(pkt));
                io_ctx_.post([this] {
                        if (under_proto_) under_proto_(packet_queue_.pop().value());
                });
        }

        virtual void process(UpperPacketType&& in_pkt [[maybe_unused]]) {}

        virtual std::optional<UpperPacketType> make_packet(raw_packet&& in_pkt [[maybe_unused]]) {
                return std::nullopt;
        }

private:
        void dispatch(UpperPacketType&& in_packet) {
                if (auto prot_it{upper_protos_.find(in_packet.proto)};
                    upper_protos_.end() != prot_it) {
                        spdlog::debug("[PROCESS PACKET] PROTO {:#04X}", in_packet.proto);
                        prot_it->second(std::move(in_packet));
                } else {
                        spdlog::debug("[UNKNOWN PACKET] PROTO {:#04X}", in_packet.proto);
                }
        }
};

template <typename UnderPacketType>
class base_protocol<UnderPacketType, void> {
protected:
        boost::asio::io_context& io_ctx_;

private:
        std::function<void(UnderPacketType&&)> under_proto_;
        circle_buffer<UnderPacketType>         packet_queue_;

public:
        template <typename UnderProtocol>
        void under_proto_update(UnderProtocol& proto) {
                under_proto_ = [&proto](UnderPacketType&& pkt) { proto.receive(std::move(pkt)); };
        }

        void receive(UnderPacketType&& in) { process(std::move(in)); }

protected:
        explicit base_protocol(boost::asio::io_context& io_ctx) : io_ctx_(io_ctx) {}
        ~base_protocol() = default;

        base_protocol(base_protocol const&)            = delete;
        base_protocol& operator=(base_protocol const&) = delete;

        base_protocol(base_protocol&&)            = delete;
        base_protocol& operator=(base_protocol&&) = delete;

        void enqueue(UnderPacketType&& pkt) {
                packet_queue_.push(std::move(pkt));
                io_ctx_.post([this] {
                        if (under_proto_) under_proto_(packet_queue_.pop().value());
                });
        }

        virtual void process(UnderPacketType&& in_pkt [[maybe_unused]]) {}
};

}  // namespace mstack
