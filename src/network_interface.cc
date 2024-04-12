#include <cassert>
#include <cstdint>
#include <iostream>

#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "exception.hh"
#include "network_interface.hh"
#include "parser.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  // Your code here.
  if ( IP2Ethernet_.contains( next_hop.ipv4_numeric() ) ) {
    EthernetHeader ethheader {
      IP2Ethernet_.find( next_hop.ipv4_numeric() ),
      this->ethernet_address_,
      EthernetHeader::TYPE_IPv4,
    };
    EthernetFrame ethframe {
      ethheader,
      serialize( dgram ), // get output
    };
    transmit( ethframe );
  } else {
    // send APR request
    if ( apr_pool_.contains( next_hop.ipv4_numeric() ) ) {
      return;
    }
    ARPMessage aprmsg;
    aprmsg.opcode = ARPMessage::OPCODE_REQUEST;
    aprmsg.sender_ethernet_address = ethernet_address_;
    aprmsg.sender_ip_address = ip_address_.ipv4_numeric();
    aprmsg.target_ip_address = next_hop.ipv4_numeric();
    aprmsg.target_ethernet_address = {}; // all zero
    EthernetHeader ethheader {
      ETHERNET_BROADCAST,
      this->ethernet_address_,
      EthernetHeader::TYPE_ARP,
    };
    EthernetFrame ethframe {
      ethheader,
      serialize( aprmsg ),
    };
    transmit( ethframe );
    apr_pool_.insert( next_hop.ipv4_numeric(), time_ );
    // add it to quene
    datagrams_buffer_.push( std::pair( next_hop, dgram ) );
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // Your code here.
  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    if ( !( frame.header.dst == ethernet_address_ || frame.header.dst == ETHERNET_BROADCAST ) )
      return;
    Parser p { frame.payload };
    InternetDatagram dgram;
    dgram.parse( p );
    if ( p.has_error() )
      assert( 0 );
    datagrams_received_.push( dgram );

  } else if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    Parser p { frame.payload };
    ARPMessage aprmsg;
    aprmsg.parse( p );
    if ( p.has_error() )
      assert( 0 );
    IP2Ethernet_.insert( aprmsg.sender_ip_address, time_, aprmsg.sender_ethernet_address );
    // update IP2Ethernet so that you can tramsmit some info
    while ( !datagrams_buffer_.empty() ) {
      const auto ip = datagrams_buffer_.front().first;
      if ( !IP2Ethernet_.contains( ip.ipv4_numeric() ) )
        break;
      EthernetHeader ethheader {
        IP2Ethernet_.find( ip.ipv4_numeric() ),
        ethernet_address_,
        EthernetHeader::TYPE_IPv4,
      };
      EthernetFrame ethframe {
        ethheader,
        serialize( datagrams_buffer_.front().second ),
      };
      transmit( ethframe );
      datagrams_buffer_.pop();
    }

    if ( !( frame.header.dst == ethernet_address_ || frame.header.dst == ETHERNET_BROADCAST ) )
      return;
    if ( aprmsg.opcode == ARPMessage::OPCODE_REQUEST && aprmsg.target_ip_address == ip_address_.ipv4_numeric() ) {
      // transmit out response
      ARPMessage response {};
      response.opcode = ARPMessage::OPCODE_REPLY;
      response.sender_ethernet_address = ethernet_address_;
      response.sender_ip_address = ip_address_.ipv4_numeric();
      response.target_ethernet_address = aprmsg.sender_ethernet_address;
      response.target_ip_address = aprmsg.sender_ip_address;

      const EthernetHeader ethheader {
        frame.header.src,
        ethernet_address_,
        EthernetHeader::TYPE_ARP,
      };
      const EthernetFrame response_frame {
        ethheader,
        serialize( response ),
      };
      transmit( response_frame );
    }

  } else {
    assert( 0 );
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  time_ += ms_since_last_tick;
  IP2Ethernet_.outdate( time_, 30000 ); // 30s
  apr_pool_.outdate( time_, 5000 );     // 5s
}

// IP2EthernetLRUList
bool IP2EthernetLRUList::contains( uint32_t ip_num )
{
  return index_.contains( ip_num );
}

EthernetAddress IP2EthernetLRUList::find( uint32_t ip_num )
{
  if ( index_.find( ip_num ) == index_.end() ) {
    return EthernetAddress {};
  }
  auto p = index_.find( ip_num )->second;
  lru_list_.splice( lru_list_.begin(), lru_list_, p );
  return lru_list_.begin()->ethaddr;
}

bool IP2EthernetLRUList::insert( uint32_t ip_num, uint64_t time, const EthernetAddress& ethaddr )
{
  if ( auto p = index_.find( ip_num ); p != index_.end() ) {
    lru_list_.splice( lru_list_.begin(), lru_list_, p->second );
    p->second->ethaddr = ethaddr;
    p->second->time = time;
    return true;

  } else {
    lru_list_.push_front( Elem {
      time,
      ethaddr,
      {},
    } );
    auto insert_iter = index_.insert( std::pair( ip_num, lru_list_.begin() ) );
    if ( insert_iter.second ) {
      lru_list_.front().hashiter = std::move( insert_iter.first );
      return true;
    } else {
      lru_list_.pop_front();
      return false;
    }
  }
}

void IP2EthernetLRUList::outdate( uint64_t time, uint64_t intervel )
{
  for ( auto i = lru_list_.rbegin(); i != lru_list_.rend(); ) {
    if ( i->time + intervel <= time ) {
      index_.erase( i->hashiter );
      i = decltype( i )( lru_list_.erase( std::next( i ).base() ) );
    } else {
      break;
    }
  }
}

// ARPRequestPool
bool ARPRequestPool::insert( uint32_t ip_num, uint64_t time )
{
  if ( auto p = index_.find( ip_num ); p != index_.end() ) {
    list_.splice( list_.begin(), list_, p->second );
    p->second->time = time;
    return true;
  } else {
    list_.push_front( Elem {
      time,
      {},
    } );
    auto i = index_.insert( std::pair( ip_num, list_.begin() ) );
    if ( i.second ) {
      list_.front().hashiter = std::move( i.first );
      return true;
    } else {
      list_.pop_front();
      return false;
    }
  }
}

bool ARPRequestPool::contains( uint32_t ip_num )
{
  return index_.contains( ip_num );
}

void ARPRequestPool::outdate( uint64_t time, uint64_t intervel )
{
  for ( auto i = list_.rbegin(); i != list_.rend(); ) {
    if ( i->time + intervel <= time ) {
      index_.erase( i->hashiter );
      i = decltype( i )( list_.erase( std::next( i ).base() ) );
    } else {
      break;
    }
  }
}