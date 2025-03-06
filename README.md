NG-Scope
========

NG-Scope is a versatile system designed to support the efficient decoding of control channels from multiple base stations. From a block-level processing perspective, data flows through  NG-Scope as follows:
 
1. 	Signal Reception and Synchronization:
NG-Scope receives wireless signals from a cell and performs cell synchronization using the PSS and SSS signals transmitted by the corresponding base station. This process ensures accurate timing alignment with the cell's subframes.
 
2. 	Subframe Segmentation:
Upon successful synchronization, NG-Scope identifies the boundaries of each subframe and divides the received IQ data into segments of 1ms, representing individual subframes. All IQ segments are then stored inside a dedicated subframe buffer.
 
3. 	Parallel DCI Decoding:
NG-Scope optimizes its performance by implementing a thread pool for DCI decoding. Multiple decoder instances are spawned to efficiently decode the control channel of each cell simultaneously. By doing so, the system avoids excessive subframe buffering and ensures real-time processing.
 
4. 	DCI Message Storage:
The decoded DCI messages are temporarily stored inside a DCI buffer. As the decoding occurs in parallel, the sequence of decoded DCIs may not be in chronological order initially.
 
5. 	Status Tracker:
To handle the out-of-order decoded DCIs, NG-Scope implements a Status Tracker. This component fetches the decoded DCIs from the buffer, orders them sequentially, and stores them in a ring buffer. Each cell has a separate ring buffer to maintain its decoded DCI messages.
 
6. 	DCI Logging:
With the DCIs now properly ordered by the Status Tracker, NG-Scope implements a DCI logger to log the decoded DCIs. The logged DCIs are presented in the correct order, providing a clear and accurate representation of the control channel data.
 
7. 	GUI Visualization:
NG-Scope goes a step further by implementing a user-friendly GUI that allows users to visualize the decoded DCI messages. This intuitive interface enhances the overall user experience and facilitates data interpretation.
 
8. 	DCI Streaming via Network:
To enable remote access and sharing of decoded DCIs, NG-Scope includes a DCI Sink. This component streams the decoded DCIs to remote locations via the network, allowing seamless data transfer and analysis.
 
9. 	Real-Time Processing Library:
NG-Scope incorporates a powerful real-time processing library that operates on the DCIs stored inside the ring buffer. Standard functions, such as calculating the cell's bandwidth usage and the capacity of individual mobile devices, are provided. Additionally, the library is designed to be easily extensible for the implementation of further functionalities.
 
In conclusion, NG-Scope offers a comprehensive and efficient workflow for decoding control channels from multiple base stations. Its parallel processing capabilities, intelligent data ordering, and user-friendly features make it an invaluable tool for analyzing and interpreting cellular communication data in real-time.

## NG-Scope Version 2.1 Release Notes

We are excited to announce the release of NG-Scope 2.1, featuring significant updates and new functionalities. Let's explore what's new:
 
Updated srsRan Library: NG-Scope 2.1 now integrates the latest srsRan library release, version 23.04. This update brings improved performance, enhanced compatibility, and ensures NG-Scope stays up-to-date with the latest advancements in the srsRan ecosystem.
 
Introducing GUI Interface: With NG-Scope 2.1, we introduce a user-friendly GUI interface that provides a visual representation of IQ constellations, Channel CSI, and other debugging information. The intuitive interface empowers users to analyze and interpret data with ease, enhancing the overall user experience.
 
DCI Streaming Framework: To streamline the analysis process, NG-Scope 2.1 implements a robust DCI streaming framework. This framework enables real-time streaming of decoded Downlink Control Information (DCI) to remote devices, applications, or programs using various network connections, including the Internet, Ethernet, and wireless networks. By minimizing latency, this feature allows for prompt analysis and monitoring of control information, leading to more accurate insights.

Basic cell configuration will be output to cellcfg.txt. Real-time reference signal received power readings will be output to rsrp.txt. Examples of these files can be found at ./ngscope/cellcfg.txt and ./ngscope/rsrp.txt.

## NG-Scope Version 2.0 Release Notes
 
We are thrilled to announce the release of NG-Scope 2.0, packed with exciting updates and new features. Here's what's in store:
 
Code Refactoring and Enhanced Compatibility: NG-Scope's code base has undergone significant refactoring for improved performance and maintainability. We have upgraded the srsRan library to release 20.04, which includes a reconstructed physical layer library and updated APIs. NG-Scope 2.0 seamlessly integrates with this update.

Expanded Base-Station Support: Commercial base stations has two operation mode, TDD and FDD. NG-Scope 2.0 now supports the decoding of both FDD and TDD base stations, covering all types of LTE base stations. More specifically, we have tested and verified the TDD decoding functionality on CBRS cells that operates at 3.5GHz.

The first step towards decoding 5G Base Station: NG-Scope 2.0 represents a significant milestone in our journey towards decoding 5G base stations. Through extensive testing and verification, we have ensured the capability of NG-Scope 2.0 to successfully decode 5G Non-Standalone (NSA) base stations. Our findings indicate that a majority of 5G NSA deployments in the US continue to utilize the LTE RAN infrastructure while implementing a 5G core network. NG-Scope 2.0 empowers users to gain insights into this evolving landscape of hybrid LTE and 5G deployments, enabling comprehensive analysis and monitoring of 5G NSA base stations.

Improved Parallelism: Building upon the foundation of NG-Scope 2.0, the parallel decoding platform has undergone significant improvements. We have optimized the platform by splitting functions into separate threads, such as the task scheduler thread, DCI decoder thread, status tracking thread, buffer handling thread, and DCI-Logging thread. These enhancements result in improved robustness, increased speed, and enhanced coding efficiency. The revamped parallel decoding platform also enables seamless scaling of the number of supported base stations, providing flexibility to accommodate various deployment scenarios.

Efficient Logging System: NG-Scope 2.0 introduced an efficient logging system that seamlessly integrates into the parallel decoding platform. This logging system records the decoded control messages, facilitating detailed analysis and troubleshooting. By capturing and organizing valuable information, the logging system enhances the overall analysis process and aids in identifying and resolving potential issues.

New Functionality: NG-Scope 2.0 introduces a new feature: the ability to decode the Channel State Information (CSI) of the wireless channel between base stations and mobile devices. Gain deeper insights into the quality and characteristics of your wireless communication.


## NG-Scope Version 1.0 Release Notes
 
We are thrilled to introduce NG-Scope, the initial release of our cutting-edge control channel decoder built on the srsRan library release 18. NG-Scope revolutionizes the control channel decoding capabilities of FDD base stations, supporting various bandwidths including 5MHz, 10MHz, 15MHz, and 20MHz.
 
Key Features:
 
Advanced Signal Decoding: NG-Scope excels at decoding essential signals such as PSS and SSS, as well as messages transmitted over PDCCH, PHICH, and PBCH.
 
Multi-Base Station Support: With NG-Scope, you can simultaneously decode the control channels of multiple base stations. This breakthrough enables efficient monitoring of carrier aggregation, a feature introduced in release 12.
 
Parallel DCI-Decoding Framework: To facilitate the decoding of multiple base stations on a single device, we have implemented a robust framework. This framework efficiently handles synchronization with the cell, demodulation of raw IQ data, and decoding of control messages.
 
We are excited to provide you with NG-Scope, a powerful tool that empowers you to unlock deeper insights into FDD base station communication. Stay tuned for future updates as we continue to enhance and expand NG-Scope's capabilities.
