#pragma once

#include <cassert>

#include <concepts>
#include <functional>
#include <optional>
#include <unordered_map>
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

        std::unordered_map<int, packet_to_upper_type> _protocols;
        std::vector<packet_from_upper_type>           _packet_providers;
        circle_buffer<UnderPacketType>                _packet_queue;

public:
        template <std::convertible_to<int> Index, typename UpperProtocol>
        void upper_handler_update(std::pair<Index, std::shared_ptr<UpperProtocol>> kv) {
                assert(kv.second);
                _packet_providers.push_back(
                        [wp = std::weak_ptr{kv.second}]() -> decltype(kv.second->gather_packet()) {
                                if (auto sp = wp.lock()) return sp->gather_packet();
                                return std::nullopt;
                        });
                _protocols[kv.first] = [sp = kv.second](UpperPacketType&& packet) {
                        sp->receive(std::move(packet));
                };
        }

        template <typename PacketType>
        void receive(PacketType&& in) {
                if (auto out{make_packet(std::move(in))}) dispatch(std::move(*out));
        }

        bool has_something_to_send() const { return !_packet_queue.empty(); }

        std::optional<UnderPacketType> gather_packet() {
                if (!has_something_to_send()) {
                        for (auto const& packet_provider : _packet_providers) {
                                if (auto upper_pkt{packet_provider()}) {
                                        if (auto pkt{make_packet(std::move(*upper_pkt))}) {
                                                _packet_queue.push_back(std::move(*pkt));
                                        }
                                }
                        }
                }
                return _packet_queue.pop_front();
        }

protected:
        void enqueue(UnderPacketType&& in_packet) { _packet_queue.push_back(std::move(in_packet)); }

private:
        virtual std::optional<UnderPacketType> make_packet(UpperPacketType&& in_packet) {
                return std::nullopt;
        }

        virtual std::optional<UpperPacketType> make_packet(UnderPacketType&& in_packet) {
                return std::nullopt;
        }

        virtual std::optional<UpperPacketType> make_packet(raw_packet&& in_packet) {
                return std::nullopt;
        }

        void dispatch(UpperPacketType&& in_packet) {
                if (auto prot_it{_protocols.find(in_packet.proto)}; _protocols.end() != prot_it) {
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

        std::unordered_map<int, packet_to_upper_type> _protocols;
        std::vector<packet_from_upper_type>           _packet_providers;
        circle_buffer<raw_packet>                     _packet_queue;

public:
        template <std::convertible_to<int> Index, typename UpperProtocol>
        void upper_handler_update(std::pair<Index, std::shared_ptr<UpperProtocol>> kv) {
                assert(kv.second);
                _packet_providers.push_back(
                        [wp = std::weak_ptr{kv.second}]() -> decltype(kv.second->gather_packet()) {
                                if (auto sp = wp.lock()) return sp->gather_packet();
                                return std::nullopt;
                        });
                _protocols[kv.first] = [sp = kv.second](UpperPacketType&& packet) {
                        sp->receive(std::move(packet));
                };
        }

        template <typename PacketType>
        void receive(PacketType&& in_packet) {
                if (auto pkt{make_packet(std::move(in_packet))}) dispatch(std::move(*pkt));
        }

        bool has_something_to_send() const { return !_packet_queue.empty(); }

        std::optional<raw_packet> gather_packet() {
                if (!has_something_to_send()) {
                        for (auto packet_provider : _packet_providers) {
                                std::optional<UpperPacketType> in_packet = packet_provider();
                                if (!in_packet) continue;
                                std::optional<raw_packet> in_packet_ =
                                        make_packet(std::move(in_packet.value()));
                                if (!in_packet_) continue;
                                _packet_queue.push_back(std::move(in_packet_.value()));
                        }
                }
                return _packet_queue.pop_front();
        }

protected:
        void enqueue(raw_packet&& pkt) { _packet_queue.push_back(std::move(pkt)); }

private:
        virtual std::optional<raw_packet> make_packet(UpperPacketType&& in_packet) {
                return std::nullopt;
        }

        virtual std::optional<UpperPacketType> make_packet(raw_packet&& in_packet) {
                return std::nullopt;
        }

        void dispatch(UpperPacketType&& in_packet) {
                if (auto prot_it{_protocols.find(in_packet.proto)}; _protocols.end() != prot_it) {
                        spdlog::debug("[PROCESS PACKET] PROTO {:#04X}", in_packet.proto);
                        prot_it->second(std::move(in_packet));
                } else {
                        spdlog::debug("[UNKNOWN PACKET] PROTO {:#04X}", in_packet.proto);
                }
        }
};

}  // namespace mstack
