#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>


#include "socket.hh"

using namespace std;

Socket::Address::Address( string ip, uint16_t port )
  : _sockaddr()
{
  _sockaddr.sin_family = AF_INET;
  _sockaddr.sin_port = htons( port );

  if ( inet_aton( ip.c_str(), &_sockaddr.sin_addr ) == 0 ) {
    fprintf( stderr, "Invalid IP address (%s)\n", ip.c_str() );
    exit( 1 );
  }
}

std::string Socket::Address::ip( void ) const
{
  char tmp[ 64 ];
  snprintf( tmp, 64, "%s", inet_ntoa( _sockaddr.sin_addr ) );
  return string( tmp );
}

const string Socket::Address::str( void ) const
{
  char tmp[ 64 ];
  snprintf( tmp, 64, "%s:%d", inet_ntoa( _sockaddr.sin_addr ), ntohs( _sockaddr.sin_port ) );
  return string( tmp );
}

Socket::Socket()
  : sock( socket( AF_INET, SOCK_DGRAM, 0 ) )
{
  if ( sock < 0 ) {
    perror( "socket" );
    exit( 1 );
  }

  /* Ask for timestamps */
  int ts_opt = 1;
  if ( setsockopt( sock, SOL_SOCKET, SO_TIMESTAMPNS, &ts_opt, sizeof( ts_opt ) )
       < 0 ) {
    perror( "setsockopt" );
    exit( 1 );
  }
}

void Socket::connect( const Socket::Address & addr ) const
{
  if ( ::connect( sock, (sockaddr *)&addr.sockaddr(), sizeof( addr.sockaddr() ) ) < 0 ) {
    fprintf( stderr, "Error connecting to %s\n", addr.str().c_str() );
    perror( "bind" );
    exit( 1 );
  }
}

void Socket::bind( const Socket::Address & addr ) const
{
  if ( ::bind( sock, (sockaddr *)&addr.sockaddr(), sizeof( addr.sockaddr() ) ) < 0 ) {
    fprintf( stderr, "Error binding to %s\n", addr.str().c_str() );
    perror( "bind" );
    exit( 1 );
  }
}

void Socket::send( const Socket::Packet & packet ) const
{
  ssize_t bytes_sent = sendto( sock, packet.payload.data(), packet.payload.size(), 0,
			       (sockaddr *)&packet.addr.sockaddr(), sizeof( packet.addr.sockaddr() ) );
  if ( bytes_sent != static_cast<ssize_t>( packet.payload.size() ) ) {
    perror( "sendto" );
  }
}

void Socket::bind_to_device( const std::string & name ) const
{
  if ( setsockopt( sock, SOL_SOCKET, SO_BINDTODEVICE, name.c_str(), name.size() ) < 0 ) {
    fprintf( stderr, "Error binding to %s\n", name.c_str() );
    perror( "setsockopt SO_BINDTODEVICE" );
    exit( 1 );
  }
}

Socket::Packet Socket::recv( void ) const
{
  /* data structure to receive timestamp, source address, and payload */
  struct sockaddr_in remote_addr;
  struct msghdr header;
  struct iovec msg_iovec;

  const int BUF_SIZE = 2048;

  char msg_payload[ BUF_SIZE ];
  char msg_control[ BUF_SIZE ];
  header.msg_name = &remote_addr;
  header.msg_namelen = sizeof( remote_addr );
  msg_iovec.iov_base = msg_payload;
  msg_iovec.iov_len = BUF_SIZE;
  header.msg_iov = &msg_iovec;
  header.msg_iovlen = 1;
  header.msg_control = msg_control;
  header.msg_controllen = BUF_SIZE;
  header.msg_flags = 0;

  ssize_t received_len = recvmsg( sock, &header, 0 );
  if ( received_len < 0 ) {
    perror( "recvmsg" );
    exit( 1 );
  }

  if ( received_len > BUF_SIZE ) {
    fprintf( stderr, "Received oversize datagram (size %d) and limit is %d\n",
	     static_cast<int>( received_len ), BUF_SIZE );
    exit( 1 );
  }

  /* verify presence of timestamp */
  struct cmsghdr *ts_hdr = CMSG_FIRSTHDR( &header );
  assert( ts_hdr );
  assert( ts_hdr->cmsg_level == SOL_SOCKET );
  assert( ts_hdr->cmsg_type == SO_TIMESTAMPNS );

  return Socket::Packet( Socket::Address( remote_addr ),
			 string( msg_payload, received_len ),
			 *(struct timespec *)CMSG_DATA( ts_hdr ) );
}

uint64_t Socket::timestamp( void )
{
  struct timespec ts;

  if ( clock_gettime( CLOCK_REALTIME, &ts ) < 0 ) {
    perror( "clock_gettime" );
    exit( 1 );
  }

  uint64_t ret = ts.tv_sec * 1000000000 + ts.tv_nsec;
  return ret;
}
