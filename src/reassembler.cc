#include "reassembler.hh"
#include "debug.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // debug( "count_bytes_pending={}", count_bytes_pending() );
  if ( writer().is_closed() ) {
    return;
  }

  const uint64_t sz = data.size();
  if ( sz == 0 ) {
    if ( is_last_substring ) {
      update_end_ind( first_index );
      if ( is_wholetask_finished() ) {
        output_.writer().close();
      }
    }
    return;
  }

  const uint64_t current_available_ind = writer().bytes_pushed();
  const uint64_t ceiling_available_ind = current_available_ind + writer().available_capacity() - 1;
  // debug( "{} {} {}", current_available_ind, ceiling_available_ind, first_index );
  if ( ceiling_available_ind < current_available_ind || first_index > ceiling_available_ind
       || first_index + sz - 1 < current_available_ind ) {
    return;
  }

  uint64_t data_start_ind = 0, data_end_ind = sz - 1, last_ind = first_index + sz - 1;
  if ( is_last_substring )
    update_end_ind( last_ind + 1 );

  if ( first_index < current_available_ind ) {
    data_start_ind += current_available_ind - first_index;
    first_index = current_available_ind;
  }

  if ( last_ind > ceiling_available_ind ) {
    data_end_ind -= last_ind - ceiling_available_ind;
    last_ind = ceiling_available_ind;
  }

  data = data.substr( data_start_ind, data_end_ind - data_start_ind + 1 );
  // debug( "cut data={}", data );

  auto it = buf.lower_bound( first_index );
  if ( it != buf.begin() ) {
    auto prev = std::prev( it );
    if ( prev->first + prev->second.size() >= first_index ) {
      if ( prev->first + prev->second.size() < first_index + data.size() ) {
        data = prev->second.substr( 0, first_index - prev->first ) + data;
        first_index = prev->first;
        total_pending -= prev->second.size();
        buf.erase( prev );
      } else {
        return;
      }
    }
  }

  while ( it != buf.end() && it->first <= first_index + data.size() - 1 ) {
    uint64_t overlap = ( first_index + data.size() ) - it->first;
    if ( overlap < it->second.size() ) {
      data += it->second.substr( overlap );
    }
    total_pending -= it->second.size();
    it = buf.erase( it );
  }

  // debug( "should in {} at {}", data, first_index );
  total_pending += data.size();
  buf[first_index] = std::move( data );

  while ( buf.size() && writer().bytes_pushed() == buf.begin()->first ) {
    total_pending -= buf.begin()->second.size();
    output_.writer().push( std::move( buf.begin()->second ) );
    buf.erase( buf.begin() );
  }
  if ( is_wholetask_finished() ) {
    output_.writer().close();
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  return total_pending;
}

bool Reassembler::is_wholetask_finished() const
{
  return end_ind.has_value() && end_ind == writer().bytes_pushed();
}

void Reassembler::update_end_ind( const uint64_t new_index )
{
  if ( end_ind.has_value() )
    end_ind = max( end_ind.value(), new_index );
  else
    end_ind = new_index;
}
