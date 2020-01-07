#ifndef SOCKET_HH
#define SOCKET_HH

#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <time.h>
#include <string.h>

class Socket {
public:
  static uint64_t timestamp( void );

  class Address {
  private:
    struct sockaddr_in _sockaddr;

  public:
    Address( std::string ip, uint16_t port );
    Address( const struct sockaddr_in s_sockaddr ) : _sockaddr( s_sockaddr ) {}

    const struct sockaddr_in & sockaddr( void ) const { return _sockaddr; }
    const std::string str( void ) const;

    std::string ip( void ) const;

    bool operator==( const Address & other ) const { return (0 == memcmp( &_sockaddr, &other._sockaddr, sizeof( _sockaddr ))); }
  };

  class Packet {
  public:
    Address addr;
    std::string payload;
    uint64_t timestamp;

    Packet( const Address &s_addr, const std::string &s_payload )
      : addr( s_addr ), payload( s_payload ), timestamp( 0 )
    {}

    Packet( const Address &s_addr, const std::string &s_payload, const struct timespec &ts )
      : addr( s_addr ), payload( s_payload ), timestamp( ts.tv_sec * 1000000000 + ts.tv_nsec )
    {}
  };

private:
  int sock;

public:
  Socket();
  void bind( const Address & addr ) const;
  void connect( const Address & addr ) const;
  void send( const Packet & payload ) const;
  void bind_to_device( const std::string & name ) const;
  Packet recv( void ) const;
  int get_sock( void ) const { return sock; }
};

const Socket::Address UNKNOWN( "0.0.0.0", 0 );

#endif
