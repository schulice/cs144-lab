#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  (void)message;
  // add 1 to get abs seqno
  if ( message.SYN )
    zero_point_ = message.seqno + 1, initialized_zero_point_ = true;
  if ( message.RST )
    reassembler_.reader().set_error();
  if ( !initialized_zero_point_ )
    return;
  const auto insert_index = [&]() -> uint64_t {
    if ( message.SYN )
      return 0;
    return message.seqno.unwrap( zero_point_, reassembler_.writer().bytes_pushed() );
  }();
  reassembler_.insert( insert_index, message.payload, message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  const auto ackno = initialized_zero_point_ ? std::optional<Wrap32>(
                       zero_point_ + reassembler_.writer().bytes_pushed() + reassembler_.writer().is_closed() )
                                             : std::nullopt;
  const auto window_size = reassembler_.writer().available_capacity() > UINT16_MAX
                             ? (uint16_t)UINT16_MAX
                             : (uint16_t)reassembler_.writer().available_capacity();
  const auto need_reset = reassembler_.reader().has_error();
  return TCPReceiverMessage {
    ackno,
    window_size,
    need_reset,
  };
}
