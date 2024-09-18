#pragma once

#include <cassert>

#include <concepts>
#include <functional>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "circle_buffer.hpp"
#include "raw_packet.hpp"

namespace mstack {

template <typename UnderPacketType, typename UpperPacketType, typename ChildType>
class base_protocol {
private:
        using packet_from_upper_type = std::function<std::optional<UpperPacketType>(void)>;
        using packet_to_upper_type   = std::function<void(UpperPacketType)>;

        std::unordered_map<int, packet_to_upper_type> protocols_;
        std::vector<packet_from_upper_type>           packet_providers_;
        circle_buffer<UnderPacketType>                packet_queue_;

public:
        template <std::convertible_to<int> Index, typename UpperProtocol>
        void upper_handler_update(std::pair<Index, std::shared_ptr<UpperProtocol>> kv) {
                assert(kv.second);
                packet_providers_.push_back(
                        [wp = std::weak_ptr{kv.second}]() -> decltype(kv.second->gather_packet()) {
                                if (auto sp = wp.lock()) return sp->gather_packet();
                                return std::nullopt;
                        });
                protocols_[kv.first] = [sp = kv.second](UpperPacketType&& packet) {
                        sp->receive(std::move(packet));
                };
        }

        template <typename PacketType>
        void receive(PacketType&& in) {
                if (auto out{make_packet(std::move(in))}) dispatch(std::move(*out));
        }

        bool has_something_to_send() const { return !packet_queue_.empty(); }

        std::optional<UnderPacketType> gather_packet() {
                if (!has_something_to_send()) {
                        for (auto const& packet_provider : packet_providers_) {
                                if (auto upper_pkt{packet_provider()}) {
                                        if (auto pkt{make_packet(std::move(*upper_pkt))}) {
                                                packet_queue_.push_back(std::move(*pkt));
                                        }
                                }
                        }
                }
                return packet_queue_.pop_front();
        }

protected:
        base_protocol()  = default;
        ~base_protocol() = default;

        base_protocol(base_protocol const&)            = delete;
        base_protocol& operator=(base_protocol const&) = delete;

        base_protocol(base_protocol&&)            = delete;
        base_protocol& operator=(base_protocol&&) = delete;

        void enqueue(UnderPacketType&& in_packet) { packet_queue_.push_back(std::move(in_packet)); }

        virtual std::optional<UnderPacketType> make_packet(UpperPacketType&& in_packet) {
                return std::nullopt;
        }

        virtual std::optional<UpperPacketType> make_packet(UnderPacketType&& in_packet) {
                return std::nullopt;
        }

        virtual std::optional<UpperPacketType> make_packet(raw_packet&& in_packet) {
                return std::nullopt;
        }

private:
        void dispatch(UpperPacketType&& in_packet) {
                if (auto prot_it{protocols_.find(in_packet.proto)}; protocols_.end() != prot_it) {
                        spdlog::debug("[PROCESS PACKET] PROTO {:#04X}", in_packet.proto);
                        prot_it->second(std::move(in_packet));
                } else {
                        spdlog::debug("[UNKNOWN PACKET] PROTO {:#04X}", in_packet.proto);
                }
        }
};

template <typename UpperPacketType, typename ChildType>
class base_protocol<raw_packet, UpperPacketType, ChildType> {
private:
        using packet_from_upper_type = std::function<std::optional<UpperPacketType>(void)>;
        using packet_to_upper_type   = std::function<void(UpperPacketType)>;

        std::unordered_map<int, packet_to_upper_type> protocols_;
        std::vector<packet_from_upper_type>           packet_providers_;
        circle_buffer<raw_packet>                     packet_queue_;

public:
        template <std::convertible_to<int> Index, typename UpperProtocol>
        void upper_handler_update(std::pair<Index, std::shared_ptr<UpperProtocol>> kv) {
                assert(kv.second);
                packet_providers_.push_back(
                        [wp = std::weak_ptr{kv.second}]() -> decltype(kv.second->gather_packet()) {
                                if (auto sp = wp.lock()) return sp->gather_packet();
                                return std::nullopt;
                        });
                protocols_[kv.first] = [sp = kv.second](UpperPacketType&& packet) {
                        sp->receive(std::move(packet));
                };
        }

        template <typename PacketType>
        void receive(PacketType&& in_packet) {
                if (auto pkt{make_packet(std::move(in_packet))}) dispatch(std::move(*pkt));
        }

        bool has_something_to_send() const { return !packet_queue_.empty(); }

        std::optional<raw_packet> gather_packet() {
                if (!has_something_to_send()) {
                        for (auto packet_provider : packet_providers_) {
                                std::optional<UpperPacketType> in_packet = packet_provider();
                                if (!in_packet) continue;
                                std::optional<raw_packet> in_packet_ =
                                        make_packet(std::move(in_packet.value()));
                                if (!in_packet_) continue;
                                packet_queue_.push_back(std::move(in_packet_.value()));
                        }
                }
                return packet_queue_.pop_front();
        }

protected:
        base_protocol()  = default;
        ~base_protocol() = default;

        base_protocol(base_protocol const&)            = delete;
        base_protocol& operator=(base_protocol const&) = delete;

        base_protocol(base_protocol&&)            = delete;
        base_protocol& operator=(base_protocol&&) = delete;

        void enqueue(raw_packet&& pkt) { packet_queue_.push_back(std::move(pkt)); }

        virtual std::optional<raw_packet> make_packet(UpperPacketType&& in_packet) {
                return std::nullopt;
        }

        virtual std::optional<UpperPacketType> make_packet(raw_packet&& in_packet) {
                return std::nullopt;
        }

private:
        void dispatch(UpperPacketType&& in_packet) {
                if (auto prot_it{protocols_.find(in_packet.proto)}; protocols_.end() != prot_it) {
                        spdlog::debug("[PROCESS PACKET] PROTO {:#04X}", in_packet.proto);
                        prot_it->second(std::move(in_packet));
                } else {
                        spdlog::debug("[UNKNOWN PACKET] PROTO {:#04X}", in_packet.proto);
                }
        }
};

template <typename UnderPacketType, typename ChildType>
class base_protocol<UnderPacketType, void, ChildType> {
private:
        circle_buffer<UnderPacketType> packet_queue_;

public:
        template <typename T>
        void receive(T&& in_pkt) {
                process(std::forward<T>(in_pkt));
        }

        virtual void process(UnderPacketType&& in_packet [[maybe_unused]]) {}

        bool has_something_to_send() const { return !packet_queue_.empty(); }

        std::optional<UnderPacketType> gather_packet() { return packet_queue_.pop_front(); }

protected:
        base_protocol()  = default;
        ~base_protocol() = default;

        base_protocol(base_protocol const&)            = delete;
        base_protocol& operator=(base_protocol const&) = delete;

        base_protocol(base_protocol&&)            = delete;
        base_protocol& operator=(base_protocol&&) = delete;

        void enqueue(UnderPacketType&& in_packet) { packet_queue_.push_back(std::move(in_packet)); }
};

}  // namespace mstack
