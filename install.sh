#!/bin/bash

#install some required libraries
sudo apt-get install cmake libfftw3-dev libmbedtls-dev libboost-program-options-dev libconfig++-dev libsctp-dev qt5-default
sudo apt-get install libboost-system-dev libboost-test-dev libboost-thread-dev libqwt-qt5-dev qtbase5-dev

#install srslte gui
git clone https://github.com/srsLTE/srsGUI.git

cd srsGUI
mkdir build
cd build
cmake ../
make 
sudo make install
cd ../../

#install gnuradio
sudo apt install gnuradio
sudo apt-get update
sudo apt install gnuradio

#install gnugsl
wget ftp://ftp.gnu.org/gnu/gsl/gsl-2.6.tar.gz
tar -xvf gsl-2.6.tar.gz
cd gsl-2.6
./configure
make
sudo make install
cd ..

#install uhd driver
sudo add-apt-repository ppa:ettusresearch/uhd
sudo apt-get update
sudo apt-get install libuhd-dev libuhd003 uhd-host



