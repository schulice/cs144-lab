#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <sys/types.h>

#include "exception.hh"
#include "network_interface.hh"

// \brief A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.
class Router
{
public:
  // Add an interface to the router
  // \param[in] interface an already-constructed network interface
  // \returns The index of the interface after it has been added to the router
  size_t add_interface( std::shared_ptr<NetworkInterface> interface )
  {
    interfaces_.push_back( notnull( "add_interface", std::move( interface ) ) );
    return interfaces_.size() - 1;
  }

  // Access an interface by index
  std::shared_ptr<NetworkInterface> interface( const size_t N ) { return interfaces_.at( N ); }

  // Add a route (a forwarding rule)
  void add_route( uint32_t route_prefix,
                  uint8_t prefix_length,
                  std::optional<Address> next_hop,
                  size_t interface_num );

  // Route packets between the interfaces
  void route();

private:
  // The router's collection of network interfaces
  std::vector<std::shared_ptr<NetworkInterface>> interfaces_ {};

  struct RouterRule
  {
    uint32_t route_prefix;
    uint8_t prefix_length;
    std::optional<Address> next_hop;
    size_t interface_num;
  };

  struct pair_hasher
  {
    std::size_t operator()( const std::pair<uint32_t, uint8_t>& p ) const
    {
      const auto hash_first = std::hash<uint32_t> {}( p.first );
      const auto hash_second = std::hash<uint32_t> {}( p.second );
      // combine func form boost
      return hash_second + 0x9e3779b9 + ( hash_first << 6 ) + ( hash_first >> 2 );
    }
  };
  // use hash map only save the prefix then query can only need to look 32 times weastly
  std::unordered_map<std::pair<uint32_t, uint8_t>, RouterRule, pair_hasher> rule_ {};
};
