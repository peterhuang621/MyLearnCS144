#include "tcp_receiver.hh"
#include "debug.hh"
#include <cstdint>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    reader().set_error();
    return;
  }
  if ( message.SYN && !zero_checkpoint_.has_value() ) {
    zero_checkpoint_ = Wrap32( message.seqno );
  }
  if ( zero_checkpoint_.has_value() ) {
    const uint64_t check_point = writer().bytes_pushed() + 1;
    const uint64_t abs_ind
      = ( message.seqno + static_cast<uint32_t>( message.SYN ) ).unwrap( zero_checkpoint_.value(), check_point )
        - 1;
    reassembler_.insert( abs_ind, message.payload, message.FIN );
  }
}

TCPReceiverMessage TCPReceiver::send() const
{
  return TCPReceiverMessage {
    ( zero_checkpoint_.has_value()
        ? Wrap32::wrap( writer().bytes_pushed() + 1 + static_cast<uint64_t>( writer().is_closed() ),
                        zero_checkpoint_.value() )
        : optional<Wrap32> {} ),
    static_cast<uint16_t>( min( writer().available_capacity(), static_cast<uint64_t>( UINT16_MAX ) ) ),
    reader().has_error() };
}