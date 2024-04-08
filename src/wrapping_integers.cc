#include "wrapping_integers.hh"
#include <cstdint>
using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  return Wrap32( ( zero_point.raw_value_ + n ) % ( (uint64_t)1 << 32 ) );
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  const auto mod = ( (uint64_t)1 << 32 );
  auto raw_value = ( raw_value_ - zero_point.raw_value_ ) % mod;
  auto get_dist = [&]( uint64_t a ) -> uint64_t {
    if ( a > checkpoint )
      return a - checkpoint;
    else
      return checkpoint - a;
  };
  auto t = checkpoint / mod;
  auto approximate_dist = get_dist( t * mod + raw_value );
  if ( get_dist( ( t - 1 ) * mod + raw_value ) < approximate_dist )
    --t;
  else if ( get_dist( ( t + 1 ) * mod + raw_value ) < approximate_dist )
    ++t;
  return t * mod + raw_value;
}
