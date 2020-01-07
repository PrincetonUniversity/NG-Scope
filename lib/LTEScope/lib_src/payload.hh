#ifndef PAYLOAD_HH
#define PAYLOAD_HH

#include <string>
class AckPayload
{
public:
  uint32_t sequence_number, ack_number; // sequence number 
  uint64_t sent_timestamp;  // the sent timestampe used to calculate RTT
  int      int_pkt_t_us;    
  uint8_t  flag;
  const std::string str( const size_t len ) const;
};


class Payload
{
public:
  uint32_t sequence_number;
  uint64_t sent_timestamp, recv_timestamp;
  int sender_id;

  const std::string str( const size_t len ) const;
  bool operator==( const Payload & other ) const;
};

class SatPayload
{
public:
    int32_t sequence_number, ack_number;
    uint64_t sent_timestamp, recv_timestamp;
    int64_t oneway_ns;
    int sender_id;

    bool CON_CLOSE;
    const std::string str( const size_t len ) const;
    bool operator==( const SatPayload & other ) const;
};

class fixRatePayload
{
public:
    uint32_t pkt_intval;
    uint32_t con_time_s;
    uint32_t nof_pkt;
    uint32_t magic_number;
    bool CON_CLOSE;
    const std::string str( const size_t len ) const;
};


#endif
