#pragma once

#include <memory>
#include <optional>
#include <unordered_map>

#include "ipv4_addr.hpp"

namespace mstack {

class device;

class routing_table {
public:
        struct record {
                ipv4_addr_t             via_addrv4;
                ipv4_addr_t             from_addrv4;
                std::shared_ptr<device> dev;
        };

        void reset(ipv4_addr_t const& addr);

        void update(ipv4_addr_t const& hop, record const& value);
        void update_default(record const& value);

        std::optional<record> query(ipv4_addr_t const& addr) const;
        std::optional<record> query_default() const;

private:
        std::unordered_map<ipv4_addr_t, record> table_;
        std::optional<record>                   default_;
};

}  // namespace mstack
