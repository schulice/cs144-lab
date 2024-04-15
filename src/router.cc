#include "router.hh"
#include "address.hh"

#include <cstddef>
#include <iostream>
#include <optional>
#include <utility>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  // Your code here.
  rule_.insert( std::pair(
    std::pair( prefix_length != 0 /*check 0.0.0.0/0*/ ? route_prefix >> ( 32 - prefix_length ) : 0, prefix_length ),
    RouterRule {
      route_prefix,
      prefix_length,
      next_hop,
      interface_num,
    } ) );
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  // Your code here.
  auto get_longest_match_rule = [&]( const uint32_t& dst ) -> std::optional<RouterRule> {
    for ( uint8_t prefix_length = 32 /* MAX ip len */; prefix_length > 0; prefix_length -= 1 ) {
      const auto key = std::pair( dst >> ( 32 - prefix_length ), prefix_length );
      if ( rule_.count( key ) )
        return rule_.find( key )->second;
    }
    // match 0.0.0.0/0
    if ( rule_.count( std::pair( 0, 0 ) ) )
      return rule_.find( std::pair( 0, 0 ) )->second;
    return std::nullopt;
  };
  for ( const auto& interface : interfaces_ ) {
    auto& dgram_queue = interface->datagrams_received();
    while ( !dgram_queue.empty() ) {
      auto dgram = std::move( dgram_queue.front() );
      dgram_queue.pop();
      const auto rule = get_longest_match_rule( dgram.header.dst );
      if ( rule.has_value() ) {
        const auto next_hop = [&]() {
          if ( rule->next_hop.has_value() )
            return rule->next_hop.value();
          return Address::from_ipv4_numeric( dgram.header.dst );
        }();
        if ( dgram.header.ttl > 1 ) {
          dgram.header.ttl -= 1;
          dgram.header.compute_checksum();
          interfaces_.at( rule->interface_num )->send_datagram( dgram, next_hop );
        }
      }
    }
  }
}
