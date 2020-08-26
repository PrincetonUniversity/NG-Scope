LTScope
========

LTScope is a free and open-source LTE control channel decoder developed by [PAWS group](https://paws.cs.princeton.edu/) at Princeton University. 

LTScope is built atop of srsLTE.

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
Thus, any hardware supported by UHD or bladeRF can be used. There is no sampling rate conversion, therefore the hardware should support 30.72 MHz clock in order to work correctly with LTE sampling frequencies and decode signals from live LTE base stations. 

We have tested the following hardware: 
 * USRP B210
 * USRP B205mini
 * USRP X300
 * limeSDR
 * bladeRF

srsLTE -- Build Instructions
------------------

* Mandatory requirements: 
  * Common:
    * cmake              https://cmake.org/
    * libfftw            http://www.fftw.org/
    * PolarSSL/mbedTLS   https://tls.mbed.org
  * srsUE:
    * Boost:             http://www.boost.org
  * srsENB:
    * Boost:             http://www.boost.org
    * lksctp:            http://lksctp.sourceforge.net/
    * config:            http://www.hyperrealm.com/libconfig/
  * srsEPC:
    * Boost:             http://www.boost.org
    * lksctp:            http://lksctp.sourceforge.net/
    * config:            http://www.hyperrealm.com/libconfig/

For example, on Ubuntu 17.04, one can install the required libraries with:
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


Execution Instructions
----------------------

The srsUE, srsENB and srsEPC applications include example configuration files
that should be copied (manually or by using the convenience script) and modified,
if needed, to meet the system configuration.
On many systems they should work out of the box.

By default, all applications will search for config files in the user's home
directory (~/.srs) upon startup.

Note that you have to execute the applications with root privileges to enable
real-time thread priorities and to permit creation of virtual network interfaces.

srsENB and srsEPC can run on the same machine as a network-in-the-box configuration.
srsUE needs to run on a separate machine.

If you have installed the software suite using ```sudo make install``` and
have installed the example config files using ```sudo srslte_install_configs.sh```,
you may just start all applications with their default parameters.

### srsEPC

On machine 1, run srsEPC as follows:

```
sudo srsepc
```

Using the default configuration, this creates a virtual network interface
named "srs_spgw_sgi" on machine 1 with IP 172.16.0.1. All connected UEs
will be assigned an IP in this network.

### srsENB

Also on machine 1, but in another console, run srsENB as follows:

```
sudo srsenb
```

### srsUE

On machine 2, run srsUE as follows:

```
sudo srsue
```

Using the default configuration, this creates a virtual network interface
named "tun_srsue" on machine 2 with an IP in the network 172.16.0.x.
Assuming the UE has been assigned IP 172.16.0.2, you may now exchange
IP traffic with machine 1 over the LTE link. For example, run a ping to 
the default SGi IP address:

```
ping 172.16.0.1
```

Support
========

Mailing list: http://www.softwareradiosystems.com/mailman/listinfo/srslte-users
