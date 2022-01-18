NG-Scope
========

NG-Scope is a free and open-source LTE control channel decoder developed by [PAWS group](https://paws.cs.princeton.edu/) at Princeton University. 

NG-Scope is built atop of srsLTE, so we introduce basic information about srsLTE.

srsLTE -- Common Features 
---------------

 * LTE Release 8 compliant (with selected features of Release 9)
 * FDD configuration
 * Tested bandwidths: 1.4, 3, 5, 10, 15 and 20 MHz
 * Transmission mode 1 (single antenna), 2 (transmit diversity), 3 (CCD) and 4 (closed-loop spatial multiplexing)
 * Frequency-based ZF and MMSE equalizer
 * Evolved multimedia broadcast and multicast service (eMBMS)
 * Highly optimized Turbo Decoder available in Intel SSE4.1/AVX2 (+100 Mbps) and standard C (+25 Mbps)
 * MAC, RLC, PDCP, RRC, NAS, S1AP and GW layers
 * Detailed log system with per-layer log levels and hex dumps
 * MAC layer wireshark packet capture
 * Command-line trace metrics
 * Detailed input configuration files

srsLTE -- Hardware
--------

srsLTE currently supports the Ettus Universal Hardware Driver (UHD) and the bladeRF driver. 
Thus, any hardware supported by UHD or bladeRF can be used. 
There is no sampling rate conversion, 
therefore the hardware should support 30.72 MHz clock in order 
to work correctly with LTE sampling frequencies and decode signals from live LTE base stations. 

According to the srsLTE website, srsLTE can support the following hardware: 
 * USRP B210
 * USRP B205mini
 * USRP X300
 * limeSDR
 * bladeRF

We, however, only tested NG-Scope on USRP B210 and USRP X310.

NG-Scope -- Build Instructions
------------------

### System requirement
We have tested NG-Scope on Ubuntu 16.04. 
To guarantee the performance, we strongly recommend installing NG-Scope on this version of Ubuntu.
If you have tested on other versions of Ubuntu, please let us know. 

### Essential libraries required 

On Ubuntu 16.04, we can install the required libraries via the following command:
```
sudo apt-get install cmake libfftw3-dev libmbedtls-dev libboost-program-options-dev libconfig++-dev libsctp-dev
```

We need to install the [srsgui](https://github.com/srslte/srsgui), a GUI tool for showing realtime results. 
On Ubuntu 16.04, you can install srsgui via the following commands:
```
git clone https://github.com/srsLTE/srsGUI.git
cd srsGUI
mkdir build
cd build
cmake ../
make
sudo make install
```

We also recommend to install [gnuradio](https://github.com/gnuradio/gnuradio) for the required [VOLK](https://github.com/gnuradio/volk). 
On Ubuntu, you can install gnuradio via the following commands:
```
sudo apt install gnuradio
sudo apt-get update
sudo apt install gnuradio
```

### RF front-end driver:
We need to install the RF front-end driver. 
The required RF front-end depends on which kind of software-define-radio you use. 
We only tested NG-Scope on USRP, which requires [UHD](https://github.com/EttusResearch/uhd).
We recommend installing UHD using the binaries provided by Ettus Research, via the following commands:
```
sudo add-apt-repository ppa:ettusresearch/uhd
sudo apt-get update
sudo apt-get install libuhd-dev libuhd003 uhd-host
``` 

### Compile and install NG-Scope
Download and build NG-Scope: 
```
git clone git@github.com:YaxiongXiePrinceton/NG-Scope.git
cd srsLTE
mkdir build
cd build
cmake ../
make
make test
sudo make install
sudo srslte_install_configs.sh
```
