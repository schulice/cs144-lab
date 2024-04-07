#include "byte_stream.hh"
#include <string>
#include <string_view>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  // Your code here.
  return closed_;
}

void Writer::push( string data )
{
  // Your code here.
  auto avalcap = available_capacity();
  if ( data.size() > avalcap ) {
    buffer_ += data.substr( 0, avalcap );
    pushed_ += avalcap;
  } else {
    buffer_ += data;
    pushed_ += data.size();
  }
  return;
}

void Writer::close()
{
  // Your code here.
  this->closed_ = true;
}

uint64_t Writer::available_capacity() const
{
  // Your code here.
  return capacity_ - buffer_.size();
}

uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return pushed_;
}

bool Reader::is_finished() const
{
  // Your code here.
  return closed_ && buffer_.size() == 0;
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return poped_;
}

string_view Reader::peek() const
{
  // Your code here.
  return string_view( buffer_ );
}

void Reader::pop( uint64_t len )
{
  // Your code here.
  if ( len >= buffer_.size() ) {
    buffer_ = "";
  } else {
    buffer_ = buffer_.substr( len, buffer_.size() - len );
  }
  poped_ += len;
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return buffer_.size();
}
