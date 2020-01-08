#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <string.h>

#include "overhead.h"
#include "client.hh"
#include "payload.hh"

#define NOF_BITS_PER_PKT 12000  // 1400Bytes, change later
extern srslte_ue_cell_usage ue_cell_usage;
extern pthread_mutex_t mutex_usage;
//extern srslte_lteCCA_rate lteCCA_rate;

Client::Client( const Socket & s_send, const Socket::Address & s_remote, FILE* file_fd)
  : _log_file(file_fd),
    _send( s_send ),
    _remote( s_remote ),
    _pkt_received(0),
    _max_ack_number(0),
    _nof_cell(0),
    _last_tti(0),
    _last_exp_rate( 500 ),
    _last_est_rate( 500 ),
    _start_time( 0 ),
    _blk_ack(1),
    _256QAM(false),
    _slow_start (true),
    _overhead_factor(0.1),
    _last_rate( 500)
{
    for(int i=0;i<MAX_NOF_CA;i++){
	cellMaxPrb[i]	= 0;
	ca_active[i]	= false;
	last_tbs[i]	= 0;
	last_tbs_hm[i]	= 0;
    }
}
int ave_empty_prb(int* cell_dl_prb, int len, uint16_t cell_prb){
    int prb_sum = 0;
    for(int i=0;i<len;i++){
	prb_sum += cell_prb - cell_dl_prb[i];
	printf("%d|%d ",cell_prb, cell_dl_prb[i]);
    } 
    printf("\n");
    return prb_sum / len;
}
int ave_ue_prb(int* ue_dl_prb, int len){
    int prb_sum = 0;
    for(int i=0;i<len;i++){
	prb_sum += ue_dl_prb[i];
    }
    return prb_sum / len;
}

int ave_phy_rate(int* tbs, int* ue_dl_prb, int len)
{
    int tbs_sum = 0;
    int prb_sum = 0;
    for(int i=0;i<len;i++){
	if(tbs[i] > 0 && ue_dl_prb[i] > 0){
	    tbs_sum += tbs[i];	
	    prb_sum += ue_dl_prb[i];	
	}
    }
    if(prb_sum == 0){
	return 0; 
    }else{
	return tbs_sum / prb_sum;  
    }
}
float ave_ue_usage(int* ue_dl_prb, int max_cell_prb){
    int ue_prb_sum = 0;
    for(int i=0;i<NOF_REPORT_SF;i++){
	ue_prb_sum += ue_dl_prb[i]; 
    }
    float ave_ue_prb	= (float) ue_prb_sum / (float) NOF_REPORT_SF;
    float  usage	= ave_ue_prb / (float) max_cell_prb;
    usage		*= 100;
    printf("ave_ue_prb:%f usage:%f\n",ave_ue_prb, usage);
    return usage;
}

int average_ue_rate(int* tbs, int len){
    int tbs_sum = 0;
    for(int i=0;i<len;i++){
	tbs_sum += tbs[i];
    }
    int rate = tbs_sum / len;
    return rate;
}
int calculate_rate_cell(int* cell_dl_prb, int*ue_dl_prb, int* tbs, int* tbs_hm, int MAX_PRB, int *probe_tbs, int* probe_tbs_hm){
    //int cell_prb_len	    = 10;
    int ue_prb	    = ave_ue_prb(ue_dl_prb, NOF_REPORT_SF);
    int empty_prb   = ave_empty_prb(&cell_dl_prb[NOF_REPORT_SF- cell_prb_len], cell_prb_len, MAX_PRB);

    //int ue_tbs_len  = 5;
    int ave_tbs	    = ave_phy_rate(&tbs[NOF_REPORT_SF - ue_tbs_len], &ue_dl_prb[NOF_REPORT_SF - ue_tbs_len], ue_tbs_len);
    int ave_tbs_hm  = ave_phy_rate(&tbs_hm[NOF_REPORT_SF - ue_tbs_len], &ue_dl_prb[NOF_REPORT_SF - ue_tbs_len], ue_tbs_len);
    
    int probe_prb   = ue_prb + empty_prb;

    if(probe_prb > MAX_PRB){
	probe_prb = MAX_PRB;
    }
    printf("MAX_PRB:%d ue_prb:%d empty_prb:%d ave_tbs:%d hm:%d probe_prb:%d\n", MAX_PRB, ue_prb, empty_prb, ave_tbs, ave_tbs_hm, probe_prb);
    *probe_tbs	    = probe_prb * ave_tbs;
    *probe_tbs_hm   = probe_prb * ave_tbs_hm;
    return 0;
}
int sum_array(int* arr, int len){
    int sum_ret = 0;
    for(int i=0;i<len;i++){
	sum_ret += arr[i];
    }
    return sum_ret;
}
int tuning_tbs(int tbs){
    int rateM	    = tbs / 1000;
    if(rateM <=0){ rateM = 1;}
    float overhead  = overhead_ratio[rateM-1];
    int tuned_tbs = (int) ( (1 - overhead) * tbs);

    printf("||| RateM:%d overhead %.4f tuned_tbs:%d\n", rateM, overhead, tuned_tbs);
    return tuned_tbs;
}

int tbs_to_rate(int tbs){
    if(tbs == 0){
	printf("TBS estimation is 0!\n");
	return 2000;
    }
    int int_pkt_t_us = (int) ( (1000 * NOF_BITS_PER_PKT) / tbs); 
    return int_pkt_t_us;
}

int rate_us_to_rate_bit(int rate_us){
    int nof_pkt_per_sec  = 1000000 / rate_us;
    int nof_bits_per_sec = nof_pkt_per_sec * NOF_BITS_PER_PKT;
    return nof_bits_per_sec;
}
int bits_per_sec_to_us(int rate_bits){
    int nof_pkt_p_sec	= rate_bits / NOF_BITS_PER_PKT;
    int rate_us		= 1000000 / nof_pkt_p_sec; 
    return rate_us;
}
int rate_combine_3_rates(int exp_rate, int est_rate, int tx_rate){
    int exp_rate_bits	= rate_us_to_rate_bit(exp_rate);
    int est_rate_bits	= rate_us_to_rate_bit(est_rate);
    int tx_rate_bits	= rate_us_to_rate_bit(tx_rate);
    int rate_bits_p_sec = exp_rate_bits - ( (exp_rate_bits - est_rate_bits) * (tx_rate_bits - est_rate_bits)) / exp_rate_bits;
    int rate_us	    =  bits_per_sec_to_us(rate_bits_p_sec);
    return rate_us;
}
int rate_us_to_rateM(int rate_t_us){
    int nof_pkt_per_sec = 1000000 / rate_t_us;
    int nof_bits	= nof_pkt_per_sec * NOF_BITS_PER_PKT;
    int rateM		= nof_bits / 1000000; 
    return rateM;
}
int rateM_to_rate_us(int rateM){
    int nof_bits_per_sec    = rateM * 1000000;
    int nof_pkt_per_sec	    = nof_bits_per_sec / NOF_BITS_PER_PKT;
    int rate_t_us	    = 1000000 / nof_pkt_per_sec;
    return rate_t_us;
} 
int rate_combine(int exp_rate_us, int est_rate_us){
    int exp_rateM   = rate_us_to_rateM(exp_rate_us);
    int est_rateM   = rate_us_to_rateM(est_rate_us);
    if(exp_rateM <= 0) exp_rateM = 1;
    if(est_rateM <= 0) est_rateM = 1;

    printf("exp rateM:%d est_rateM:%d\n",exp_rateM, est_rateM);
    if(exp_rateM < est_rateM){
	return est_rate_us;
    }

    int set_rateM	= exp_rateM - (exp_rateM - est_rateM) * (exp_rateM - est_rateM) / exp_rateM; 
    int set_rate_t_us	= rateM_to_rate_us(set_rateM);
    printf("set_rateM:%d rate_t_us:%d\n",set_rateM,set_rate_t_us);
    return set_rate_t_us;
}

void Client::recv( void )
{
    /* get the data packet */
    Socket::Packet incoming( _send.recv() );
    Payload *contents = (Payload *) incoming.payload.data();
    contents->recv_timestamp = incoming.timestamp;

    _pkt_received++;

    int64_t oneway_ns = contents->recv_timestamp - contents->sent_timestamp;
    double oneway = oneway_ns / 1.e9;

    if ( _remote == UNKNOWN ) {
	return;
    }
    assert( !(_remote == UNKNOWN) );
    
    uint32_t current_tti = 0;
    pthread_mutex_lock(&mutex_usage);
    srslte_UeCell_get_status(&ue_cell_usage, _last_tti, &current_tti, ca_active, cell_dl_prb, ue_dl_prb, mcs_tb1, mcs_tb2, tbs, tbs_hm);
    pthread_mutex_unlock(&mutex_usage);

    printf("current tti:%d last tti:%d\n",current_tti, _last_tti); 
    
    int probe_rate = 0, probe_rate_hm = 0;
    int ave_ue_rate = 0, ave_ue_rate_hm = 0;
    int set_rate = 500;
    if(current_tti == _last_tti){
        //TODO directly send the same data	
        set_rate = _last_rate;
    }else{
        _last_tti = current_tti; 
        int probe_tbs = 0, probe_tbs_hm = 0;	
	float ue_usage = ave_ue_usage(ue_dl_prb[0], cellMaxPrb[0]);
	printf("Usage:%f\n",ue_usage);
	if(ue_usage > 70){
	    _slow_start = false;
	}
        int ue_tbs		= average_ue_rate(tbs[0], NOF_REPORT_SF);
        int ue_tbs_hm		= average_ue_rate(tbs_hm[0], NOF_REPORT_SF);
	printf("UE-Rate \n");
	int tuned_ue_tbs	= tuning_tbs(ue_tbs);
	int tuned_ue_tbs_hm	= tuning_tbs(ue_tbs_hm);

	printf("CELL-Rate \n");
        calculate_rate_cell(cell_dl_prb[0], ue_dl_prb[0], tbs[0], tbs_hm[0], cellMaxPrb[0], &probe_tbs, &probe_tbs_hm);
	// in case we don't have any PBRs allocated to this user
        if(probe_tbs == 0 && probe_tbs_hm == 0){
            probe_tbs	    = last_tbs[0];
            probe_tbs_hm    = last_tbs_hm[0];
        }else{
            last_tbs[0]	    = probe_tbs;
            last_tbs_hm[0]  = probe_tbs_hm;
        }
	int tuned_probe_tbs	= tuning_tbs(probe_tbs);	
	int tuned_probe_tbs_hm	= tuning_tbs(probe_tbs_hm);
        
            
        for(int i=1;i<MAX_NOF_CA;i++){
            if( ca_active[i] ){
		ue_tbs		= average_ue_rate(tbs[i], NOF_REPORT_SF);
        	ue_tbs_hm	= average_ue_rate(tbs_hm[i], NOF_REPORT_SF);
		printf("UE-Rate \n");
		tuned_ue_tbs	+= tuning_tbs(ue_tbs);
		tuned_ue_tbs_hm	+= tuning_tbs(ue_tbs_hm);

		printf("CELL-Rate \n");
        	probe_tbs	= 0; 
        	probe_tbs_hm	= 0;
        	calculate_rate_cell(cell_dl_prb[i], ue_dl_prb[i], tbs[i], tbs_hm[i], cellMaxPrb[i], &probe_tbs, &probe_tbs_hm);
        	// in case we don't have any PBRs allocated to this user
        	if(probe_tbs == 0 && probe_tbs_hm == 0){
        	    probe_tbs	    = last_tbs[i];
        	    probe_tbs_hm    = last_tbs_hm[i];
        	}else{
        	    last_tbs[i]	    = probe_tbs;
        	    last_tbs_hm[i]  = probe_tbs_hm;
        	}
		tuned_probe_tbs	    += tuning_tbs(probe_tbs);	
		tuned_probe_tbs_hm  += tuning_tbs(probe_tbs_hm);
            }
        }
	probe_rate	= tbs_to_rate(tuned_probe_tbs);
	probe_rate_hm	= tbs_to_rate(tuned_probe_tbs_hm);
	ave_ue_rate	= tbs_to_rate(tuned_ue_tbs);
	ave_ue_rate_hm	= tbs_to_rate(tuned_ue_tbs_hm);
        if(_256QAM){
            //set_rate = probe_rate_hm;
	    _last_exp_rate  = probe_rate_hm;
	    _last_est_rate  = ave_ue_rate_hm;
        }else{
            //set_rate = probe_rate;
	    _last_exp_rate  = probe_rate;
	    _last_est_rate  = ave_ue_rate;
        }
	if(_slow_start){
	    set_rate	= _last_exp_rate;
	}else{
	    set_rate	= _last_exp_rate;
	    //set_rate = rate_combine(_last_exp_rate, _last_est_rate);	
	}
        if(set_rate <= 0){
            set_rate = 250;
        }
        _last_rate  = set_rate;
        printf("RET --- SetRate:%d  Rate:%d Rate hm:%d UE Rate%d Rate hm:%d\n",set_rate, probe_rate, probe_rate_hm, ave_ue_rate, ave_ue_rate_hm);
    }

    if( (_pkt_received % _blk_ack) == 0){
	//printf("_Pkt_received:%d blk_ack:%d\n", _pkt_received, _blk_ack);
	/* send ack */
	AckPayload outgoing;
	//printf("ACK: %d\n",contents->sequence_number);
	if(contents->sequence_number >= _max_ack_number){
	    _max_ack_number = contents->sequence_number;
	}
	outgoing.sequence_number = contents->sequence_number;
	outgoing.ack_number	 = _max_ack_number;
	outgoing.int_pkt_t_us    = set_rate;
	outgoing.sent_timestamp  = contents->sent_timestamp;

	_send.send( Socket::Packet( _remote, outgoing.str( sizeof( AckPayload ) ) ) );
    }
    fprintf( _log_file,"%d\t %ld\t %ld\t %ld\t %.4f\t %d\t %d\t %d\t%d\t\n",
      contents->sequence_number, contents->sent_timestamp, contents->recv_timestamp,Socket::timestamp(), oneway, current_tti, _last_exp_rate, _last_est_rate, set_rate ); 
    return;
}

void Client::recv_noRF(srslte_lteCCA_rate* lteCCA_rate )
{
    /* get the data packet */
    Socket::Packet incoming( _send.recv() );
    Payload *contents = (Payload *) incoming.payload.data();
    contents->recv_timestamp = incoming.timestamp;
    _pkt_received++;

    int     tx_rate_us = contents->tx_rate_us;

    int64_t oneway_ns = contents->recv_timestamp - contents->sent_timestamp;
    double oneway = oneway_ns / 1.e9;

    if ( _remote == UNKNOWN ) {
	return;
    }
    assert( !(_remote == UNKNOWN) );
    uint64_t curr_time		= Socket::timestamp();
    if( _start_time == 0){
	_start_time = curr_time;
    }
    uint64_t time_passed_ms	= (curr_time - _start_time) / 1000000;
    
    int set_rate = 500;  
    uint32_t interval = 20;
    int cell_usage_thread = 70;
    if(lteCCA_rate->cell_usage > cell_usage_thread){
	_slow_start = false;
    }
    if(_slow_start){
	if(_256QAM){
	    if(lteCCA_rate->full_load > 3000){
		lteCCA_rate->full_load = 1000;
	    }
	    if(lteCCA_rate->full_load_hm > 3000){
		lteCCA_rate->full_load_hm = 1000;
	    }
	    if( time_passed_ms < interval){
		set_rate = lteCCA_rate->full_load * 4;	
	    }else if(time_passed_ms > interval){
		set_rate = lteCCA_rate->full_load * 2;	
	    }else if(time_passed_ms > 2*interval){
		set_rate = lteCCA_rate->full_load * 1.25;	
	    }else if(time_passed_ms > 3*interval){
		set_rate = lteCCA_rate->full_load * 1.1;	
	    }else{
		printf("The time passed value incorrect!\n");
	    }
	}else{
	    if( time_passed_ms < interval){
		set_rate = lteCCA_rate->full_load_hm * 4;	
	    }else if(time_passed_ms > interval){
		set_rate = lteCCA_rate->full_load_hm * 2;	
	    }else if(time_passed_ms > 2*interval){
		set_rate = lteCCA_rate->full_load_hm * 1.25;	
	    }else if(time_passed_ms > 3*interval){
		set_rate = lteCCA_rate->full_load_hm * 1.1;	
	    }else{
		printf("The time passed value incorrect!\n");
	    }
	}
    }else{
	if (lteCCA_rate->probe_rate > 3000){
	    lteCCA_rate->probe_rate = 1000;
	}
	if (lteCCA_rate->probe_rate_hm > 3000){
	    lteCCA_rate->probe_rate_hm = 1000;
	}
	if(!_256QAM){
	   // if( (lteCCA_rate->probe_rate < lteCCA_rate->ue_rate) && (tx_rate_us < lteCCA_rate->ue_rate)){
	   //     int rate_us = rate_combine_3_rates(lteCCA_rate->probe_rate, lteCCA_rate->ue_rate, tx_rate_us); 
	   //     set_rate = rate_us; 
	   // }else{
	   //     set_rate = lteCCA_rate->probe_rate; 
	   // }
	    set_rate = lteCCA_rate->probe_rate; 
	}else{
	   // if( (lteCCA_rate->probe_rate_hm < lteCCA_rate->ue_rate_hm) && (tx_rate_us < lteCCA_rate->ue_rate_hm)){
	   //     int rate_us = rate_combine_3_rates(lteCCA_rate->probe_rate_hm, lteCCA_rate->ue_rate_hm, tx_rate_us); 
	   //     set_rate = rate_us; 
	   // }else{
	   //     set_rate = lteCCA_rate->probe_rate_hm; 
	   // }
	    set_rate = lteCCA_rate->probe_rate_hm; 
	}	
    }
    if( (_pkt_received % _blk_ack) == 0){
	//printf("_Pkt_received:%d blk_ack:%d\n", _pkt_received, _blk_ack);
	/* send ack */
	AckPayload outgoing;
	//printf("ACK: %d\n",contents->sequence_number);
	if(contents->sequence_number >= _max_ack_number){
	    _max_ack_number = contents->sequence_number;
	}
	outgoing.sequence_number = contents->sequence_number;
	outgoing.ack_number	 = _max_ack_number;
	outgoing.int_pkt_t_us    = set_rate;
	outgoing.sent_timestamp  = contents->sent_timestamp;
	_send.send( Socket::Packet( _remote, outgoing.str( sizeof( AckPayload ) ) ) );
    }
    fprintf( _log_file,"%d\t %ld\t %ld\t %ld\t %.4f\t %d\t %d\t %d\t %d\t %d\t\n",
    contents->sequence_number, contents->sent_timestamp, contents->recv_timestamp, curr_time, oneway, 
	set_rate, lteCCA_rate->ue_rate_hm, tx_rate_us, lteCCA_rate->probe_rate_hm, _slow_start); 
    return;
}
void Client::init_connection(void)
{
    if ( _remote == UNKNOWN ) {
	return;
    }
    assert( !(_remote == UNKNOWN) );
    /* send ack */
    AckPayload outgoing;
    outgoing.sequence_number	= 1111;
    outgoing.ack_number		= 8888;
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
    outgoing.ack_number		= 8888;
    outgoing.sent_timestamp	= 9999;
    outgoing.flag		= 2;
    _send.send( Socket::Packet( _remote, outgoing.str( sizeof( AckPayload ) ) ) );
    _send.send( Socket::Packet( _remote, outgoing.str( sizeof( AckPayload ) ) ) );
}

void Client::set_256QAM(bool is_256QAM){
    _256QAM = is_256QAM;
}
void Client::set_blk_ack(int blk_ack){
    _blk_ack = blk_ack;
    printf("we set blk_ack to %d from %d\n",_blk_ack, blk_ack); 
}
void Client::set_overhead_factor(float factor){
    _overhead_factor = factor; 
}
int Client::get_rcvPkt(void){
    return _pkt_received;
}
void Client::set_cell_num_prb(void){
    pthread_mutex_lock(&mutex_usage);

    srslte_UeCell_get_maxPRB(&ue_cell_usage, cellMaxPrb);
    _nof_cell = srslte_UeCell_get_nof_cell(&ue_cell_usage);

    pthread_mutex_unlock(&mutex_usage);
}

