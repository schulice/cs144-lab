#include "reassembler.hh"
#include <cstdint>
#include <utility>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // Your code here.
  if ( output_.writer().is_closed() )
    return;

  // get an alias
  const auto out_index = output_.writer().bytes_pushed();

  // set end_index
  if ( is_last_substring && end_index_ == 0xffffffff ) {
    end_index_ = first_index + data.size();
    // close the writer
    if ( out_index == end_index_ ) {
      output_.writer().close();
      return;
    }
  }

  if ( data.size() == 0 )
    return;

  // get width
  const auto width = [&] {
    auto p = output_.writer().available_capacity();
    if ( end_index_ != 0xffffffff ) {
      p = min( p, end_index_ - out_index );
    }
    return p;
  }();
  if ( width == 0 )
    return;

  // [out, out + width - 1] comp [first, first + size - 1]
  if ( first_index > out_index + width - 1 )
    return;
  if ( first_index + data.size() - 1 < out_index )
    return;
  if ( first_index < out_index ) {
    data = data.substr( out_index - first_index, data.size() - ( out_index - first_index ) );
    first_index = out_index;
  }
  if ( first_index + data.size() - 1 > out_index + width - 1 ) {
    // len = datasize - (first + size - out - wid)
    data = data.substr( 0, out_index + width - first_index );
  }

  // insert elem to map
  auto [p, v] = unassemble_subs_.insert( pair( first_index, "" ) );
  if ( !v ) {
    auto q = unassemble_subs_.find( first_index );
    if ( q->second.size() > data.size() )
      return;
    pending_ += data.size() - q->second.size();
    q->second = std::move( data );
    p = q;
  } else {
    pending_ += data.size();
    p->second = std::move( data );
  }

  // forward check
  if ( p != unassemble_subs_.begin() ) {
    auto pre = p;
    --pre;
    if ( p->first + p->second.size() <= pre->first + pre->second.size() ) {
      pending_ -= p->second.size();
      unassemble_subs_.erase( p );
      return;
    }
    if ( p->first < pre->first + pre->second.size() ) {
      const auto delta = pre->first + pre->second.size() - p->first;
      pending_ -= delta; // pending_ add psize above
      pre->second += p->second.substr( delta, p->second.size() - delta );
      std::swap( p, pre );
      unassemble_subs_.erase( pre );
    }
  }

  // next check
  auto q = p;
  ++q;
  while ( q != unassemble_subs_.end() && q->first + q->second.size() <= p->first + p->second.size() ) {
    pending_ -= q->second.size();
    q = unassemble_subs_.erase( q );
  }
  while ( q != unassemble_subs_.end() && q->first <= p->first + p->second.size() ) {
    auto delta = p->first + p->second.size() - q->first;
    pending_ -= delta;
    p->second += q->second.substr( delta, q->second.size() - delta );
    q = unassemble_subs_.erase( q );
  }

  // can output
  if ( unassemble_subs_.begin()->first == out_index ) {
    pending_ -= unassemble_subs_.begin()->second.size();
    output_.writer().push( std::move( unassemble_subs_.begin()->second ) );
    unassemble_subs_.erase( unassemble_subs_.begin() );
  }

  // close the writer
  if ( output_.writer().bytes_pushed() == end_index_ ) {
    output_.writer().close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  // Your code here.
  return pending_;
}
