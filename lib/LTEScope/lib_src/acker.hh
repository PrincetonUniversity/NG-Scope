#ifndef ACKER_HH
#define ACKER_HH
#include <fstream>
#include "socket.hh"

class SaturateServo;

class Acker
{
private:
    const std::string _name;
    FILE* _log_file;

    const Socket _listen;
    const Socket _send;
    Socket::Address _remote;
    const bool _server;
    const int _ack_id;

    uint32_t _pkt_received;
    uint64_t _next_ping_time;
    static const int _ping_interval = 1000000000;
    int _foreign_id;
    
    uint32_t _pkt_intval;
    uint32_t _con_time_s;
    uint32_t _nof_pkt;

public:
    Acker(const Socket & s_listen,
	 const Socket & s_send,
	 const Socket::Address & s_remote,
	 const bool s_server,
	 const int s_ack_id );
    void recv( void );
    void recv_noACK( void );
    void recv_noACK_noRF( void );
    void recv_1ms( void );

    void tick( void );
    void notify_config(bool);
    void set_pkt_intval(uint32_t);
    void set_con_time(uint32_t);
    void set_nof_pkt(uint32_t);

    int get_rcvPkt( void );
    void set_remote( const Socket::Address & s_remote ) { _remote = s_remote; }

    uint64_t wait_time( void ) const;

    Acker( const Acker & ) = delete;
    const Acker & operator=( const Acker & ) = delete;
};

#endif
