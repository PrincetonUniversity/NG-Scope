LTScope
========

LTScope is a free and open-source LTE control channel decoder developed by [PAWS group](https://paws.cs.princeton.edu/) at Princeton University. 

LTScope is built atop of srsLTE, so we introduce basic information about srsLTE.

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
We, however, only tested LTScope on USRP B210 and USRP X310.

LTScope -- Build Instructions
------------------

### System requirement
We have tested LTScope on Ubuntu 16.04. 
To guarantee the performance, we strongly recommend to install LTScope on this version of Ubuntu.

First of all, we need to install the required libraries.
```
sudo apt-get install cmake libfftw3-dev libmbedtls-dev libboost-program-options-dev libconfig++-dev libsctp-dev
```
or on Fedora:
```
dnf install cmake fftw3-devel polarssl-devel lksctp-tools-devel libconfig-devel boost-devel
```

Note that depending on your flavor and version of Linux, the actual package names may be different.

* Optional requirements: 
  * srsgui:              https://github.com/srslte/srsgui - for real-time plotting.
  * libpcsclite-dev:     https://pcsclite.apdu.fr/ - for accessing smart card readers

* RF front-end driver:
  * UHD:                 https://github.com/EttusResearch/uhd
  * SoapySDR:            https://github.com/pothosware/SoapySDR
  * BladeRF:             https://github.com/Nuand/bladeRF

Download and build srsLTE: 
```
git clone https://github.com/srsLTE/srsLTE.git
cd srsLTE
mkdir build
cd build
cmake ../
make
make test
```

Install srsLTE:

```
sudo make install
sudo srslte_install_configs.sh
```

This installs srsLTE and also copies the default srsLTE config files to
the user's home directory (~/.srs).

