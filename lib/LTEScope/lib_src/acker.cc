#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <string.h>

#include "acker.hh"
#include "payload.hh"

extern bool go_exit;
Acker::Acker( const Socket & s_listen, const Socket & s_send, const Socket::Address & s_remote, const bool s_server, const int s_ack_id )
  : _listen( s_listen ),
    _send( s_send ),
    _remote( s_remote ),
    _server( s_server ),
    _ack_id( s_ack_id ),
    _pkt_received(0),
    _next_ping_time( Socket::timestamp() ),
    _foreign_id( -1 ),
    _pkt_intval(0),
    _con_time_s(0),
    _nof_pkt(30000)
{}

void Acker::recv_1ms( void )
{
    if( (_nof_pkt > 0) && (_con_time_s > 0)){
	printf("Nof pkt and connection time cannot be set simultaneously!\n");
    }
    if( (_nof_pkt == 0) && (_con_time_s == 0)){
	printf("Nof_pkt and time are all zeros!\n");
	_nof_pkt = 30000;
    }
    /* wait for incoming packet OR expiry of timer */
    struct pollfd poll_fds[ 1 ];
    poll_fds[ 0 ].fd = _listen.get_sock();
    poll_fds[ 0 ].events = POLLIN;
    struct timespec timeout;

    FILE* FD;
    FD = fopen("./data/client-ack-log","w+");
    
    _pkt_received = 0;
    timeout.tv_sec = 1;
    timeout.tv_nsec = 0;

    uint64_t curr_time;
    while ( 1 ) {
	if(go_exit){
	    printf("GO EXIT! RETURN\n\n");
	    break;
	}
        fflush( NULL );
	//printf("WHILE LOOPING! pkt_rcv:%d nof_pkt:%d\n", _pkt_received, _nof_pkt);
        ppoll( poll_fds, 1, &timeout, NULL );

        if ( poll_fds[ 0 ].revents & POLLIN ) {
	    //printf("POLLIN\n");
	    /* get the data packet */
	    Socket::Packet incoming( _listen.recv() );
	    SatPayload *contents = (SatPayload *) incoming.payload.data();
	    contents->recv_timestamp = incoming.timestamp;

	    _pkt_received += 1;
	    int64_t oneway_ns = contents->recv_timestamp - contents->sent_timestamp;
	    double oneway = oneway_ns / 1.e6;

	    uint32_t sfx=0;
	    //cc_buf_getSFX(ctl_ch_buf, &sfx);
	    //curr_time = Socket::timestamp();
	    fprintf(FD,"%d\t %ld\t %ld\t %ld\t %.4f\t \n", contents->sequence_number, contents->sent_timestamp, contents->recv_timestamp, Socket::timestamp(), oneway); 
	    if( _nof_pkt > 0){
		if( _pkt_received > _nof_pkt * _pkt_intval){
		    printf("All packet received! EXIT!\n");
		    //acker.tick_number(99999);
		    fclose(FD);
		    return;
		}
	    }else if( _con_time_s > 0){
		if(contents->CON_CLOSE){
		    printf("Time is up! EXIT!\n");
		    fclose(FD);
		    return;
		}
	    }
	}
    }
    fclose(FD);
    return;
}
void Acker::recv_noACK( void )
{
    if( (_nof_pkt > 0) && (_con_time_s > 0)){
	printf("Nof pkt and connection time cannot be set simultaneously!\n");
    }
    if( (_nof_pkt == 0) && (_con_time_s == 0)){
	printf("Nof_pkt and time are all zeros!\n");
	_nof_pkt = 30000;
    }
    /* wait for incoming packet OR expiry of timer */
    struct pollfd poll_fds[ 1 ];
    poll_fds[ 0 ].fd = _listen.get_sock();
    poll_fds[ 0 ].events = POLLIN;
    struct timespec timeout;

    FILE* FD;
    FD = fopen("./client-ack-log","w+");
    
    _pkt_received = 0;
    timeout.tv_sec = 1;
    timeout.tv_nsec = 0;

    float time_passed;
    uint64_t start_time, end_time, begin_time;
    uint64_t curr_time;

    begin_time = Socket::timestamp();
    while ( 1 ) {
        if(go_exit){
            printf("GO EXIT! RETURN\n\n");
            break;
        }
	start_time  = Socket::timestamp();
        time_passed = (start_time - begin_time) / 1.e6;
	
	if(time_passed > _con_time_s + 10000){
	    break;
	}
        fflush( NULL );
	    //printf("WHILE LOOPING! pkt_rcv:%d nof_pkt:%d\n", _pkt_received, _nof_pkt);
        ppoll( poll_fds, 1, &timeout, NULL );

        if ( poll_fds[ 0 ].revents & POLLIN ) {
	    //printf("POLLIN\n");
	    /* get the data packet */
	    Socket::Packet incoming( _listen.recv() );
	    SatPayload *contents = (SatPayload *) incoming.payload.data();
	    contents->recv_timestamp = incoming.timestamp;

	    _pkt_received += 1;
	    int64_t oneway_ns = contents->recv_timestamp - contents->sent_timestamp;
	    double oneway = oneway_ns / 1.e6;

	    uint32_t sfx=0;
	    //cc_buf_getSFX(ctl_ch_buf, &sfx);
	    //curr_time = Socket::timestamp();
	    
	    if (_pkt_received % 100 == 0){ 
		printf("Received %d packets with SeqNo: %d oneway delay:%.4f (ms) \n",_pkt_received, contents->sequence_number, oneway);
	    }
	    fprintf(FD,"%d\t %ld\t %ld\t %ld\t %.4f\t %d \n", contents->sequence_number, contents->sent_timestamp, contents->recv_timestamp, Socket::timestamp(), oneway); 
	    if( _con_time_s > 0){
		if(contents->CON_CLOSE){
		    printf("Time is up! EXIT!\n");
		    fclose(FD);
		    return;
		}
	    }
	}
    }
    fclose(FD);
    return;
}

void Acker::recv_noACK_noRF( void )
{
    if( (_nof_pkt > 0) && (_con_time_s > 0)){
	printf("Nof pkt and connection time cannot be set simultaneously!\n");
    }
    if( (_nof_pkt == 0) && (_con_time_s == 0)){
	printf("Nof_pkt and time are all zeros!\n");
	_nof_pkt = 30000;
    }
    /* wait for incoming packet OR expiry of timer */
    struct pollfd poll_fds[ 1 ];
    poll_fds[ 0 ].fd = _listen.get_sock();
    poll_fds[ 0 ].events = POLLIN;
    struct timespec timeout;

    FILE* FD;
    FD = fopen("./data/client-ack-log","a+");
    
    _pkt_received = 0;
    timeout.tv_sec = 1;
    timeout.tv_nsec = 0;

    uint64_t curr_time;
    while ( 1 ) {
        if(go_exit){
            printf("GO EXIT! RETURN\n\n");
            break;
        }
        fflush( NULL );
	    //printf("WHILE LOOPING! pkt_rcv:%d nof_pkt:%d\n", _pkt_received, _nof_pkt);
        ppoll( poll_fds, 1, &timeout, NULL );

        if ( poll_fds[ 0 ].revents & POLLIN ) {
	    //printf("POLLIN\n");
	    /* get the data packet */
	    Socket::Packet incoming( _listen.recv() );
	    SatPayload *contents = (SatPayload *) incoming.payload.data();
	    contents->recv_timestamp = incoming.timestamp;

	    _pkt_received += 1;
	    int64_t oneway_ns = contents->recv_timestamp - contents->sent_timestamp;
	    double oneway = oneway_ns / 1.e6;

	    uint32_t sfx=0;
	    //cc_buf_getSFX(ctl_ch_buf, &sfx);
	    //curr_time = Socket::timestamp();
	    
	    if (_pkt_received % 100 == 0){ 
		printf("Received %d packets with SeqNo: %d oneway delay:%.4f (ms) \n",_pkt_received, contents->sequence_number, oneway);
	    }
	    fprintf(FD,"%d\t %ld\t %ld\t %ld\t %.4f\t\n", contents->sequence_number, contents->sent_timestamp, contents->recv_timestamp, Socket::timestamp(), oneway); 
	    if( _nof_pkt > 0){
		if( _pkt_received > _nof_pkt){
		    printf("All packet received! EXIT!\n");
		    //acker.tick_number(99999);
		    fclose(FD);
		    return;
		}
	    }else if( _con_time_s > 0){
		if(contents->CON_CLOSE){
		    printf("Time is up! EXIT!\n");
		    fclose(FD);
		    return;
		}
	    }
	}
    }
    fclose(FD);
    return;
}

void Acker::recv( void )
{
  /* get the data packet */
  Socket::Packet incoming( _listen.recv() );
  SatPayload *contents = (SatPayload *) incoming.payload.data();
  contents->recv_timestamp = incoming.timestamp;

  int64_t oneway_ns = contents->recv_timestamp - contents->sent_timestamp;
  double oneway = oneway_ns / 1.e9;

  if ( _server ) {

    if ( _remote == UNKNOWN ) {
      return;
    }
  }

  assert( !(_remote == UNKNOWN) );

  Socket::Address fb_destination( _remote );

  /* send ack */
  SatPayload outgoing( *contents );
  outgoing.sequence_number = -1;
  outgoing.oneway_ns = oneway_ns;

  printf("ACK: %d\n",contents->sequence_number);
  outgoing.ack_number = contents->sequence_number;
  _send.send( Socket::Packet( _remote, outgoing.str( sizeof( SatPayload ) ) ) );
   //fprintf( _log_file,"%d\t %ld\t %ld\t %ld\t %.4f\t \n",
      //contents->sequence_number, contents->sent_timestamp, contents->recv_timestamp,Socket::timestamp(), oneway ); 
}

int Acker::get_rcvPkt(void){
    return _pkt_received;
}
void Acker::set_pkt_intval(uint32_t pkt_intval){
    _pkt_intval = pkt_intval;
}

void Acker::set_con_time(uint32_t time_s){
    _con_time_s = time_s;
}

void Acker::set_nof_pkt(uint32_t nof_pkt){
    _nof_pkt = nof_pkt;
}

void Acker::notify_config( bool close_flag)
{
    if ( _server ) {
	return;
    }

    /* send NAT heartbeats */
    if ( _remote == UNKNOWN ) {
	return;
    }

    fixRatePayload contents;
    contents.pkt_intval = _pkt_intval;
    contents.con_time_s = _con_time_s; 
    contents.nof_pkt	= _nof_pkt;
    contents.CON_CLOSE	= close_flag;
    contents.magic_number	= 12345;
    printf("pkt_intval:%d cont_time_s:%d nof_pkt:%d magic_number:%d\n", 
			    contents.pkt_intval, contents.con_time_s, contents.nof_pkt, contents.magic_number);
    _send.send( Socket::Packet( _remote, contents.str( sizeof( fixRatePayload ) ) ) );
}

void Acker::tick( void )
{
  if ( _server ) {
    return;
  }

  /* send NAT heartbeats */
  if ( _remote == UNKNOWN ) {
    _next_ping_time = Socket::timestamp() + _ping_interval;
    return;
  }

  if ( _next_ping_time < Socket::timestamp() ) {
    SatPayload contents;
    contents.sequence_number = -1;
    contents.ack_number = -1;
    contents.sent_timestamp = Socket::timestamp();
    contents.recv_timestamp = 0;
    contents.sender_id = _ack_id;

    _send.send( Socket::Packet( _remote, contents.str( sizeof( SatPayload ) ) ) );

    _next_ping_time = Socket::timestamp() + _ping_interval;
  }
}

uint64_t Acker::wait_time( void ) const
{
  if ( _server ) {
    return 1000000000;
  }

  int diff = _next_ping_time - Socket::timestamp();
  if ( diff < 0 ) {
    diff = 0;
  }

  return diff;
}
