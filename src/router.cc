#include "router.hh"
#include "debug.hh"

#include <iostream>

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

  routing_tables[prefix_length].emplace(
    make_pair( prefix_length ? ( route_prefix >> ( 32 - prefix_length ) ) : 0, info { interface_num, next_hop } ) );
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  for ( auto&& interface : interfaces_ ) {
    queue<InternetDatagram>& datagram_received = interface->datagrams_received();
    while ( datagram_received.size() ) {
      InternetDatagram datagram = std::move( datagram_received.front() );
      datagram_received.pop();

      if ( datagram.header.ttl > 1 ) {
        datagram.header.ttl--;
        datagram.header.compute_checksum();

        optional<info> routing_path = match( datagram.header.dst );
        if ( routing_path.has_value() ) {
          interfaces_[routing_path->interface_num]->send_datagram(
            datagram, routing_path->next_hop.value_or( Address::from_ipv4_numeric( datagram.header.dst ) ) );
        }
      }
    }
  }
}

optional<info> Router::match( uint32_t request_ip )
{
  if ( routing_tables[32].contains( request_ip ) )
    return optional<info>( routing_tables[32][request_ip] );

  for ( int i = 31; i >= 1; i-- ) {
    for ( auto&& item : routing_tables[i] ) {
      if ( ( request_ip >> ( 32 - i ) ) == item.first ) {
        return optional<info>( item.second );
      }
    }
  }

  if ( !routing_tables[0].empty() ) {
    return routing_tables[0].begin()->second;
  }

  return optional<info> {};
}
