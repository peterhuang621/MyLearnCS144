#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"

using namespace std;

// How many sequence numbers are outstanding?
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return outstanding_count;
}

// How many consecutive retransmissions have happened?
uint64_t TCPSender::consecutive_retransmissions() const
{
  return retransmissions_count;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  debug( "unimplemented push() called" );
  (void)transmit;
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  return TCPSenderMessage { Wrap32::wrap( sentno_, isn_ ), false, string {}, false, input_.has_error() };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  window_size = msg.window_size;
  if ( msg.RST || !msg.ackno.has_value() )
    return;
  const uint64_t abs_ackno = msg.ackno.value().unwrap( isn_, sentno_ );
  if ( abs_ackno > sentno_ )
    return;
  ackno_ = abs_ackno;

  bool hasackmsg { false };
  while ( outstanding_message_.size() ) {
    TCPSenderMessage& item = outstanding_message_.front();
    if ( item.sequence_length() + item.seqno.unwrap( isn_, sentno_ ) <= abs_ackno ) {
      outstanding_count -= item.sequence_length();
      outstanding_message_.pop();

      hasackmsg = true;
    } else {
      break;
    }
  }

  if ( hasackmsg ) {
    timer.reset();
    retransmissions_count = 0;
  }

  if ( outstanding_message_.empty() ) {
    timer.inValid();
  } else {
    timer.start();
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  timer.tick( ms_since_last_tick );
  if ( timer.isExpired() && outstanding_message_.size() ) {
    transmit( outstanding_message_.front() );
    if ( window_size != 0 ) {
      retransmissions_count++;
      timer.exp_Backoff();
    }
    timer.start();
  }
}

void Timer::start()
{
  valid = true;
}

void Timer::reset()
{
  curr_RTO_ms = init_RTO_ms;
}

void Timer::inValid()
{
  valid = false;
}

void Timer::tick( uint64_t time_since_last_tick )
{
  time_ms += valid * time_since_last_tick;
}

void Timer::exp_Backoff()
{
  curr_RTO_ms *= 2;
}

bool Timer::isValid() const
{
  return valid;
}

bool Timer::isExpired() const
{
  return valid && time_ms >= curr_RTO_ms;
}
