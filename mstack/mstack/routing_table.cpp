#include "routing_table.hpp"

#include <spdlog/spdlog.h>

#include "device.hpp"

namespace mstack {

void routing_table::reset(ipv4_addr_t const& addr) {
        if (auto it{table_.find(addr)}; table_.end() != it) {
                spdlog::debug("[RT TBL] rem: {} via {} dev {}", it->first, it->second.addr,
                              it->second.dev->name());
                table_.erase(it);
        }
}

void routing_table::update(ipv4_addr_t const& hop, record const& via) {
        spdlog::debug("[RT TBL] upd: {} via {} dev {}", hop, via.addr, via.dev->name());
        table_.insert({hop, via});
}

void routing_table::update_default(record const& via) {
        spdlog::debug("[RT TBL] upd: default via {} dev {}", via.addr, via.dev->name());
        default_.emplace(via);
}

auto routing_table::query(ipv4_addr_t const& addr) const -> std::optional<record> {
        if (auto it{table_.find(addr)}; table_.end() != it) {
                return it->second;
        }
        return std::nullopt;
}

auto routing_table::query_default() const -> std::optional<record> { return default_; }

}  // namespace mstack
