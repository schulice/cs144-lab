#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"

#include <cstdint>
#include <functional>
#include <map>

class RetransmissionsTimer
{
public:
  RetransmissionsTimer( uint64_t initial_RTO_ms = 0 ) : RTO_ms_( initial_RTO_ms ) {};
  void reset_RTO( uint64_t initial_RTO_ms );
  void pass( uint64_t passed_ms );
  bool is_activated();
  bool is_closed();
  void double_RTO();
  void reset();
  void close();

private:
  uint64_t tick_ms_ {};
  uint64_t RTO_ms_;
  bool active_ {};
  bool close_ {};
};

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms ), timer_( initial_RTO_ms )
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
  Writer& writer() { return input_.writer(); }
  const Writer& writer() const { return input_.writer(); }

  // Access input stream reader, but const-only (can't read from outside)
  const Reader& reader() const { return input_.reader(); }

private:
  // Variables initialized in constructor
  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  RetransmissionsTimer timer_ {};
  uint64_t receive_index_ { 0 };
  uint64_t send_index_ {};
  uint64_t window_size_ { 1 };
  uint64_t retransmission_time_ {};
  std::map<uint64_t, TCPSenderMessage> outstanding_seg_ {};
  bool is_initialized_ {};
  bool is_closed_ {};
  void insert_buffer_( uint64_t index, TCPSenderMessage msg );
};
