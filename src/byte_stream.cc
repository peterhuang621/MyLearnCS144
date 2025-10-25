#include "byte_stream.hh"
#include "debug.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

// Push data to stream, but only as much as available capacity allows.
void Writer::push( string data )
{
  uint64_t available_bytes = available_capacity();
  bytesready = data.size();
  if ( bytesready == 0 || available_bytes == 0 )
    return;

  available_bytes = min( available_bytes, bytesready );
  if ( available_bytes < bytesready )
    data = data.substr( 0, available_bytes );
  buffer.emplace_back( std::move( data ) );
  bytessent += available_bytes;
  bytesinbuffer += available_bytes;
}

// Signal that the stream has reached its ending. Nothing more will be written.
void Writer::close()
{
  close_status = true;
}

// Has the stream been closed?
bool Writer::is_closed() const
{
  return close_status;
}

// How many bytes can be pushed to the stream right now?
uint64_t Writer::available_capacity() const
{

  return capacity_ - bytesinbuffer;
}

// Total number of bytes cumulatively pushed to the stream
uint64_t Writer::bytes_pushed() const
{
  return bytessent;
}

// Peek at the next bytes in the buffer -- ideally as many as possible.
// It's not required to return a string_view of the *whole* buffer, but
// if the peeked string_view is only one byte at a time, it will probably force
// the caller to do a lot of extra work.
string_view Reader::peek() const
{
  if ( bytesinbuffer == 0 )
    return {};
  return buffer.front();
}

// Remove `len` bytes from the buffer.
void Reader::pop( uint64_t len )
{
  uint64_t available_bytes = min( len, bytes_buffered() );
  if ( available_bytes == 0 )
    return;
  bytesreceived += available_bytes;
  uint64_t tmp, sz;
  while ( available_bytes ) {
    sz = buffer.front().size();
    if ( sz > available_bytes ) {
      buffer.front() = buffer.front().substr( available_bytes );
    } else {
      buffer.pop_front();
    }
    tmp = min( available_bytes, sz );
    available_bytes -= tmp;
    bytesinbuffer -= tmp;
  }
}

// Is the stream finished (closed and fully popped)?
bool Reader::is_finished() const
{
  return writer().is_closed() && bytesinbuffer == 0;
}

// Number of bytes currently buffered (pushed and not popped)
uint64_t Reader::bytes_buffered() const
{
  return bytesinbuffer;
}

// Total number of bytes cumulatively popped from stream
uint64_t Reader::bytes_popped() const
{
  return bytesreceived;
}
