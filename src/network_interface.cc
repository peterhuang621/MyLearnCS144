#include <iostream>

#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "exception.hh"
#include "helpers.hh"
#include "network_interface.hh"

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
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( InternetDatagram dgram, const Address& next_hop )
{
  const uint32_t target_ip = next_hop.ipv4_numeric();
  EthernetFrame frame;

  if ( ip2eth.contains( target_ip ) ) {
    frame.header = EthernetHeader { ip2eth[target_ip].first, ethernet_address_, EthernetHeader::TYPE_IPv4 };
    frame.payload = serialize( dgram );
    transmit( std::move( frame ) );

  } else {
    if ( !arp_timer.contains( target_ip ) ) {
      ARPMessage msg;
      msg.opcode = ARPMessage::OPCODE_REQUEST;
      msg.sender_ethernet_address = ethernet_address_;
      msg.sender_ip_address = ip_address_.ipv4_numeric();
      msg.target_ip_address = target_ip;
      frame.header = EthernetHeader { ETHERNET_BROADCAST, ethernet_address_, EthernetHeader::TYPE_ARP };
      frame.payload = serialize( std::move( msg ) );
      arp_timer[target_ip] = 0;
      transmit( std::move( frame ) );
    }
    waited_dgrams[target_ip].emplace_back( std::move( dgram ) );
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST ) {
    return;
  }

  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram dgram;
    if ( parse( dgram, frame.payload ) )
      datagrams_received_.emplace( std::move( dgram ) );
    return;
  }

  EthernetFrame reply_frame;
  if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage msg;
    if ( parse( msg, frame.payload ) ) {
      ip2eth[msg.sender_ip_address] = make_pair( msg.sender_ethernet_address, 0 );
      if ( msg.opcode == ARPMessage::OPCODE_REQUEST && msg.target_ip_address == ip_address_.ipv4_numeric() ) {
        ARPMessage reply_msg;
        reply_msg.opcode = ARPMessage::OPCODE_REPLY;
        reply_msg.sender_ethernet_address = ethernet_address_;
        reply_msg.sender_ip_address = ip_address_.ipv4_numeric();
        reply_msg.target_ethernet_address = msg.sender_ethernet_address;
        reply_msg.target_ip_address = msg.sender_ip_address;
        reply_frame.header
          = EthernetHeader { msg.sender_ethernet_address, ethernet_address_, EthernetHeader::TYPE_ARP };
        reply_frame.payload = serialize( std::move( reply_msg ) );
        transmit( std::move( reply_frame ) );
      }

      for ( auto&& item : waited_dgrams[msg.sender_ip_address] ) {
        reply_frame.header
          = EthernetHeader { msg.sender_ethernet_address, ethernet_address_, EthernetHeader::TYPE_IPv4 };
        reply_frame.payload = serialize( item );
        transmit( std::move( reply_frame ) );
      }
      waited_dgrams.erase( msg.sender_ip_address );
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  static constexpr size_t IP_MAP_TTL = 30000;     // 30s
  static constexpr size_t ARP_REQUEST_TTL = 5000; // 5s
  for ( auto it = ip2eth.begin(); it != ip2eth.end(); ) {
    it->second.second += ms_since_last_tick;
    if ( it->second.second >= IP_MAP_TTL )
      it = ip2eth.erase( it );
    else
      it++;
  }

  for ( auto it = arp_timer.begin(); it != arp_timer.end(); ) {
    it->second += ms_since_last_tick;
    if ( it->second >= ARP_REQUEST_TTL ) {
      waited_dgrams.erase( it->first );
      it = arp_timer.erase( it );
    } else
      it++;
  }
}
