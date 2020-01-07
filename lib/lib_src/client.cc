#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <string.h>

#include "client.hh"
#include "payload.hh"
extern srslte_ue_cell_usage ue_cell_usage;
extern pthread_mutex_t mutex_usage;

Client::Client( const Socket & s_send, const Socket::Address & s_remote)
  : _send( s_send ),
    _remote( s_remote ),
    _pkt_received(0),
    _next_ping_time( Socket::timestamp() )
{
    last_tti = 0;
    nof_cell = 0;
    for(int i=0;i<max_cell;i++){
	cellMaxPrb[i] = 0;
    }
}

void Client::recv( void )
{
    /* get the data packet */
    Socket::Packet incoming( _send.recv() );
    SatPayload *contents = (SatPayload *) incoming.payload.data();
    contents->recv_timestamp = incoming.timestamp;

    _pkt_received++;

    int64_t oneway_ns = contents->recv_timestamp - contents->sent_timestamp;
    double oneway = oneway_ns / 1.e9;

    if ( _remote == UNKNOWN ) {
	return;
    }
    assert( !(_remote == UNKNOWN) );
    
    uint32_t current_tti;
    pthread_mutex_lock(&mutex_usage);
    srslte_UeCell_get_status(&ue_cell_usage, last_tti, &current_tti, nof_cell, cell_dl_prb, ue_dl_prb, mcs_tb1, mcs_tb2, tbs, tbs_hm);
    pthread_mutex_unlock(&mutex_usage);
    /* send ack */
    AckPayload outgoing;
    printf("ACK: %d\n",contents->sequence_number);
    outgoing.sequence_number = contents->sequence_number;
    outgoing.sent_timestamp = contents->sent_timestamp;

    _send.send( Socket::Packet( _remote, outgoing.str( sizeof( AckPayload ) ) ) );
    //fprintf( _log_file,"%d\t %ld\t %ld\t %ld\t %.4f\t \n",
      //contents->sequence_number, contents->sent_timestamp, contents->recv_timestamp,Socket::timestamp(), oneway ); 
}
void Client::init_connection( void )
{
    if ( _remote == UNKNOWN ) {
	return;
    }
    assert( !(_remote == UNKNOWN) );
    /* send ack */
    AckPayload outgoing;
    outgoing.sequence_number	= 1111;
    outgoing.sent_timestamp	= 9999;
    outgoing.flag		= 1;
    _send.send( Socket::Packet( _remote, outgoing.str( sizeof( AckPayload ) ) ) );
    _send.send( Socket::Packet( _remote, outgoing.str( sizeof( AckPayload ) ) ) );
}

void Client::close_connection( void )
{
    if ( _remote == UNKNOWN ) {
	return;
    }
    assert( !(_remote == UNKNOWN) );
    /* send ack */
    AckPayload outgoing;
    outgoing.sequence_number	= 1111;
    outgoing.sent_timestamp	= 9999;
    outgoing.flag		= 2;
    _send.send( Socket::Packet( _remote, outgoing.str( sizeof( AckPayload ) ) ) );
    _send.send( Socket::Packet( _remote, outgoing.str( sizeof( AckPayload ) ) ) );
}

int Client::get_rcvPkt(void){
    return _pkt_received;
}
void Client::set_cell_prb(int cell_num){
    nof_cell = cell_num;
    pthread_mutex_lock(&mutex_usage);
    srslte_UeCell_get_maxPRB(&ue_cell_usage, cellMaxPrb, nof_cell);
    pthread_mutex_unlock(&mutex_usage);
}
uint64_t Client::wait_time( void ) const
{
  int diff = _next_ping_time - Socket::timestamp();
  if ( diff < 0 ) {
    diff = 0;
  }

  return diff;
}
