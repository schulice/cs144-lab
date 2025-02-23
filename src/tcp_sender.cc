#include "tcp_sender.hh"
#include "tcp_config.hh"
#include "tcp_sender_message.hh"
#include <cstdint>
#include <netinet/in.h>

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return send_index_ - receive_index_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return retransmission_time_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // Your code here.
  const auto window_size = max( window_size_, static_cast<uint64_t>( 1 ) );
  while ( sequence_numbers_in_flight() < window_size ) {
    const auto s = input_.reader().peek();
    auto msg = make_empty_message();
    if ( !is_initialized_ ) {
      msg.SYN = true;
      is_initialized_ = true;
    }
    const auto payload_size = [&]() {
      const auto windows_free_space
        = static_cast<uint64_t>( min( window_size - sequence_numbers_in_flight(), TCPConfig::MAX_PAYLOAD_SIZE ) );
      const auto payload_upper = windows_free_space - msg.sequence_length();
      return static_cast<uint64_t>( min( payload_upper, s.size() ) );
    }();
    msg.payload = s.substr( 0, payload_size );
    input_.reader().pop( payload_size );
    if ( !is_closed_ && input_.reader().is_finished()
         && window_size > sequence_numbers_in_flight() + msg.sequence_length() ) {
      msg.FIN = true;
      is_closed_ = true;
    }
    // nothing need to be send
    if ( msg.sequence_length() == 0 )
      break;
    insert_buffer_( send_index_, msg );
    transmit( msg );
    if ( timer_.is_closed() ) {
      timer_.reset();
      timer_.reset_RTO( initial_RTO_ms_ );
    }
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  // Your code here.
  return TCPSenderMessage {
    isn_ + send_index_,
    false,
    "",
    false,
    input_.has_error(),
  };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  window_size_ = msg.window_size;
  if ( msg.RST )
    input_.set_error();
  if ( !msg.ackno.has_value() ) {
    is_initialized_ = false;
    return;
  }
  // sendindex relative to isn_, and 0 preserve for SYN
  const auto ackno_abs = msg.ackno->unwrap( isn_, receive_index_ );
  if ( ackno_abs <= receive_index_ || ackno_abs > send_index_ ) {
    return;
  }
  auto i = outstanding_seg_.begin();
  while ( i != outstanding_seg_.end() && i->first + i->second.sequence_length() <= ackno_abs ) {
    i++;
  }
  outstanding_seg_.erase( outstanding_seg_.begin(), i );
  receive_index_ = ackno_abs;
  retransmission_time_ = 0;
  if ( sequence_numbers_in_flight() == 0 ) {
    timer_.close();
  } else {
    timer_.reset_RTO( initial_RTO_ms_ );
    timer_.reset();
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // Your code here.
  timer_.pass( ms_since_last_tick );
  if ( timer_.is_activated() ) {
    if ( !outstanding_seg_.empty() && retransmission_time_ < TCPConfig::TIMEOUT_DFLT) {
      transmit( outstanding_seg_.begin()->second );
      if ( window_size_ != 0) {
        timer_.double_RTO();
      }
      ++retransmission_time_;
    }
  };
}

void TCPSender::insert_buffer_( uint64_t index, TCPSenderMessage msg )
{
  outstanding_seg_.insert( pair( index, msg ) );
  send_index_ += msg.sequence_length();
}

void RetransmissionsTimer::pass( uint64_t passed_ms )
{
  if ( close_ )
    return;
  tick_ms_ += passed_ms;
  if ( tick_ms_ >= RTO_ms_ ) {
    tick_ms_ -= RTO_ms_;
    active_ = true;
  }
}

bool RetransmissionsTimer::is_activated()
{
  if ( active_ && !close_ ) {
    active_ = false;
    return true;
  }
  return false;
}

void RetransmissionsTimer::double_RTO()
{
  RTO_ms_ *= 2;
}

void RetransmissionsTimer::reset_RTO( uint64_t initial_RTO_ms )
{
  RTO_ms_ = initial_RTO_ms;
};

void RetransmissionsTimer::reset()
{
  tick_ms_ = 0;
  active_ = false;
  close_ = false;
}

bool RetransmissionsTimer::is_closed()
{
  return close_;
}

void RetransmissionsTimer::close()
{
  close_ = true;
  active_ = false;
}
