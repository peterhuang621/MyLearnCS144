#include "wrapping_integers.hh"
#include "debug.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // debug( "unimplemented wrap( {}, {} ) called", n, zero_point.raw_value_ );
  return zero_point + n;
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // debug( "unimplemented unwrap( {}, {} ) called", zero_point.raw_value_, checkpoint );
  static constexpr uint64_t TWO31 = 1UL << 31;
  static constexpr uint64_t TWO32 = 1UL << 32;

  auto const ckpt32 = wrap( checkpoint, zero_point );
  uint64_t const dis = raw_value_ - ckpt32.raw_value_;

  if ( dis <= TWO31 || checkpoint + dis < TWO32 ) {
    return checkpoint + dis;
  }
  return checkpoint + dis - TWO32;
}
