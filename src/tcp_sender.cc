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
  uint64_t start = 0;
  const auto window_size = max(window_size_, (uint64_t)1);
  while (sequence_numbers_in_flight() < window_size) { 
    const auto s = input_.reader().peek();
    auto msg = make_empty_message();
    if (!is_initialized_) {
      msg.SYN = true;
      is_initialized_ = true; 
    }
    const auto payload_size = [&]() {
      const auto payload_upper_size = (uint64_t)min(window_size - sequence_numbers_in_flight(), TCPConfig::MAX_PAYLOAD_SIZE);
      return (uint64_t)min(payload_upper_size - msg.sequence_length(), s.size() - start); 
    } ();
    msg.payload = s.substr(0, payload_size);
    input_.reader().pop(payload_size);
    if (input_.reader().is_finished() && window_size > sequence_numbers_in_flight() + payload_size && !is_closed_) msg.FIN = true, is_closed_ = true;
    if (msg.sequence_length() == 0) return;  // assert
    // actually send payload
    transmit(msg);
    insert_buffer_(send_index_, msg);
    if (timer_.is_closed()) {
      timer_.reset();
      timer_.reset_RTO(initial_RTO_ms_);
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
  if (msg.RST) input_.set_error();
  if (!msg.ackno.has_value()) {
    is_initialized_ = false;
    return;
  }
  // sendindex relative to isn_, and 0 preserve for SYN
  const auto ackno_abs = msg.ackno->unwrap(isn_, receive_index_);
  if (ackno_abs > receive_index_ && send_index_ >= ackno_abs) {
    for (auto i = outstanding_seg_.begin(); i != outstanding_seg_.end();) {
      if (ackno_abs >= i->first + i->second.sequence_length()) 
        i = outstanding_seg_.erase(i);
      else break;
    }
    receive_index_ = ackno_abs;
    retransmission_time_ = 0;
    if (sequence_numbers_in_flight() == 0) {
      timer_.close();
    } else {
      timer_.reset_RTO(initial_RTO_ms_);
      timer_.reset();
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // Your code here.
  timer_.pass(ms_since_last_tick);
  if (timer_.is_activated()) {
    if (window_size_ == 0 && !is_initialized_) {
      transmit(make_empty_message());
    } else if (!outstanding_seg_.empty()) {
      transmit(outstanding_seg_.begin()->second);
      if (window_size_ != 0)
        timer_.double_RTO();
      ++retransmission_time_;
    }
  };
}

void TCPSender::insert_buffer_(uint64_t index, TCPSenderMessage msg) {
  outstanding_seg_.insert(pair(index, msg));
  send_index_ += msg.sequence_length();
}

void RetransmissionsTimer::pass(uint64_t passed_ms) {
  if (close_) return;
  tick_ms_ += passed_ms;
  if (tick_ms_ >= RTO_ms_) {
    tick_ms_ -= RTO_ms_;
    active_ = true;
  }
}

bool RetransmissionsTimer::is_activated() {
  if (active_ && !close_) {
    active_ = false;
    return true;
  }
  return false;
}

void RetransmissionsTimer::double_RTO() {
  RTO_ms_ *= 2;
}

void RetransmissionsTimer::reset_RTO(uint64_t initial_RTO_ms) {
  RTO_ms_ = initial_RTO_ms;
};

void RetransmissionsTimer::reset() {
  tick_ms_ = 0;
  active_ = false;
  close_ = false;
}

bool RetransmissionsTimer::is_closed() {
  return close_;
}

void RetransmissionsTimer::close() {
  close_ = true;
  active_ = false;
}