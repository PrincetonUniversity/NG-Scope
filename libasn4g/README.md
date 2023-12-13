# libasn4g
**IMPORTANT**: This version of *libasn4g* is not maintained.

**libasn4g** is a small library to decode LTE SIB messages codified using ASN1. Generally speaking, SIB messages provide the necessary information about the network for the UE to connect. These messages are broadcasted to all the UEs in the cell and contain all sorts of configuration parameters about the network, the cell, and surrounding cells.
Currently, the decoding of the following messages is supported:
- SIB1:
  - Cell Access Related Information (PLMN Identity List, PLMN Identity, TA Code, Cell identity, and Cell Status)
  - Cell Selection Information (Minimum Receiver Level)
  - Scheduling Information (SI message type & Periodicity, SIB mapping Info, and SI Window length)
- SIB2:
  - Access Barring Information (Access Probability factor, Access Class Baring List, and Access Class Baring Time)
  - Semi static Common Channel Configuration (Random Access Parameters and PRACH Configuration)
  - UL frequency Information (UL EARFCN, UL Bandwidth, and additional emmission)
  - Multicast Broadcase Single Frequency Network Configuration
- SIB3:
  - Information/Parameters for intra-frequency cell reselections
- SIB4:
  - Information on intra-frequency neighboring cells
- SIB5:
  - Information on inter-frequency neighboring cells

## How to compile libasn4g

**NOTE**: libasn4g is not required by NG-Scope to work, it is just an addition in case you want to decode the SIBs and MIB messages.

First, the [ASN1 RRC specification](/spec/EUTRA-RRC-Definitions.asn) should be compiled into C code. For that, I suggest using [asn1c](https://github.com/vlm/asn1c). After compiling *asn1c*, you can use it to generate the source (.c) and header (.h) files required by libasn4g. You can use this [guide](https://github.com/vlm/asn1c/blob/master/doc/asn1c-usage.pdf) to compile the RRC ASN1 specification. Once that step is done, you will have as a result thousands of source and header files. Move the source files to the source folder so that libasn4g.c can refer to them. Finally, you can compile libasn4g.c along with all the other source and header files into a [shared library (.so)](https://www.cprogramming.com/tutorial/shared-libraries-linux-gcc.html) and install it in your system so that NG-Scope can use it.

## Expected output
Example of a decoded SIB3 message:
```xml
<BCCH-DL-SCH-Message>
    <message>
        <c1>
            <systemInformation>
                <criticalExtensions>
                    <systemInformation-r8>
                        <sib-TypeAndInfo>
                                <sib3>
                                    <cellReselectionInfoCommon>
                                        <q-Hyst><dB4/></q-Hyst>
                                    </cellReselectionInfoCommon>
                                    <cellReselectionServingFreqInfo>
                                        <s-NonIntraSearch>2</s-NonIntraSearch>
                                        <threshServingLow>1</threshServingLow>
                                        <cellReselectionPriority>4</cellReselectionPriority>
                                    </cellReselectionServingFreqInfo>
                                    <intraFreqCellReselectionInfo>
                                        <q-RxLevMin>-65</q-RxLevMin>
                                        <p-Max>23</p-Max>
                                        <s-IntraSearch>31</s-IntraSearch>
                                        <allowedMeasBandwidth><mbw100/></allowedMeasBandwidth>
                                        <presenceAntennaPort1><true/></presenceAntennaPort1>
                                        <neighCellConfig>
                                            10
                                        </neighCellConfig>
                                        <t-ReselectionEUTRA>1</t-ReselectionEUTRA>
                                    </intraFreqCellReselectionInfo>
                                    <ext1>
                                        <s-IntraSearch-v920>
                                            <s-IntraSearchP-r9>31</s-IntraSearchP-r9>
                                            <s-IntraSearchQ-r9>31</s-IntraSearchQ-r9>
                                        </s-IntraSearch-v920>
                                        <s-NonIntraSearch-v920>
                                            <s-NonIntraSearchP-r9>2</s-NonIntraSearchP-r9>
                                            <s-NonIntraSearchQ-r9>12</s-NonIntraSearchQ-r9>
                                        </s-NonIntraSearch-v920>
                                        <q-QualMin-r9>-30</q-QualMin-r9>
                                        <threshServingLowQ-r9>12</threshServingLowQ-r9>
                                    </ext1>
                                </sib3>
                            
                        </sib-TypeAndInfo>
                    </systemInformation-r8>
                </criticalExtensions>
            </systemInformation>
        </c1>
    </message>
</BCCH-DL-SCH-Message>
```