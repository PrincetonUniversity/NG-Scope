#ifndef SINK_RECV_CONFIG_HH 
#define SINK_RECV_CONFIG_HH

typedef struct{
	bool remote_enable;
	char serv_IP[40];
	int  serv_port;
}dci_sink_config_t;

#endif


