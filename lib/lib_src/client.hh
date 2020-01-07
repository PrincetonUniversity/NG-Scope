#ifndef CLIENT_HH
#define CLIENT_HH
#include <fstream>
#include "socket.hh"
#include "srslte/srslte.h"

#define max_cell 3
class Client
{
private:
    FILE* _log_file;
    const Socket _send;
    Socket::Address _remote;
    
    uint32_t _pkt_received;
    uint64_t _next_ping_time;
    static const int _ping_interval = 1000000000;
	
public:
    Client( const Socket & s_send, 
	    const Socket::Address & s_remote);
    
    int	     nof_cell;
    uint16_t cellMaxPrb[max_cell];

    uint32_t last_tti;
    uint16_t cell_dl_prb[max_cell][NOF_REPORT_SF];
    uint16_t ue_dl_prb[max_cell][NOF_REPORT_SF];
    uint32_t mcs_tb1[max_cell][NOF_REPORT_SF];
    uint32_t mcs_tb2[max_cell][NOF_REPORT_SF];
    int	     tbs[max_cell][NOF_REPORT_SF];
    int	     tbs_hm[max_cell][NOF_REPORT_SF];

    void    init_connection( void );
    void    close_connection( void );
    void    recv( void );

    void    set_cell_prb(int);

    int	    get_rcvPkt( void );
    void    set_remote( const Socket::Address & s_remote ) { _remote = s_remote; }

    uint64_t wait_time( void ) const;
//    Client( const Client & ) = delete;
//    const Client & operator=( const Client & ) = delete;
};

#endif
