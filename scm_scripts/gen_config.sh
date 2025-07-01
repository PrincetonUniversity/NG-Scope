#!/bin/bash

cfgfile="temp_config.cfg"

if [ -f "$cfgfile" ]; then
  rm "$cfgfile"
fi


echo "nof_rf_dev = 1;" > $cfgfile
echo "rnti=9185;" >> $cfgfile
echo "disable_plot = false;" >> $cfgfile
echo "remote_enable= true;" >> $cfgfile
echo "decode_single_ue= false;" >> $cfgfile
echo "scm_mode= true;" >> $cfgfile

echo "rf_config0 = {" >> $cfgfile
echo "    rf_freq   	= $1;" >> $cfgfile
echo "    N_id_2  		= -1;" >> $cfgfile
echo "    rf_args 		= \"type=x300\";" >> $cfgfile
echo "    nof_thread  	= 3;" >> $cfgfile
echo "    disable_plot    = true;" >> $cfgfile
echo "    log_dl  		= true;" >> $cfgfile
echo "    log_ul  		= true;" >> $cfgfile
echo "    log_phich		= false;" >> $cfgfile
echo "}" >> $cfgfile


echo "dci_log_config = {" >> $cfgfile
echo "    log_dl  = true;" >> $cfgfile
echo "    log_ul  = true;" >> $cfgfile
echo "    log_interval = 5; // in seconds" >> $cfgfile
echo "}" >> $cfgfile
