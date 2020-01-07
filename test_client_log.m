clear; close all
log  = load('./build/lib/LTEScope/data/client_log');
send_t = log(:,2);

t_dif = diff(send_t);
subplot(2,1,1)
plot(t_dif./1000,'.-');
ylim([0,1000]);

oneway = log(:,5);
subplot(2,1,2)
plot(oneway.*1000,'.');

oneway95 = prctile(oneway, 95);

pkt_num = length(log);
pktData = 12000 * pkt_num;
rx_t    = (log(end,4) - log(1,4)) ./ 10^9;

rate    = pktData ./ rx_t;
rateM   = rate ./ 10^6;