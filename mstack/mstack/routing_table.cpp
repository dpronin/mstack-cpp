#include "routing_table.hpp"

#include <cassert>

#include <spdlog/spdlog.h>

#include "device.hpp"

namespace mstack {

void routing_table::reset(ipv4_addr_t const& addr) {
        if (auto it{table_.find(addr)}; table_.end() != it) {
                spdlog::debug("[RT TBL] rem: {} via {} from {} dev {}", it->first,
                              it->second.via_addrv4, it->second.from_addrv4,
                              it->second.dev->name());
                table_.erase(it);
        }
}

void routing_table::update(ipv4_addr_t const& hop, record const& record) {
        assert(record.dev);
        spdlog::debug("[RT TBL] upd: {} via {} from {} dev {}", hop, record.via_addrv4,
                      record.from_addrv4, record.dev->name());
        table_.emplace(hop, record);
}

void routing_table::update_default(record const& record) {
        assert(record.dev);
        spdlog::debug("[RT TBL] upd: default via {} from {} dev {}", record.via_addrv4,
                      record.from_addrv4, record.dev->name());
        default_.emplace(record);
}

auto routing_table::query(ipv4_addr_t const& addr) const -> std::optional<record> {
        if (auto it{table_.find(addr)}; table_.end() != it) {
                return it->second;
        }
        return std::nullopt;
}

auto routing_table::query_default() const -> std::optional<record> { return default_; }

}  // namespace mstack
