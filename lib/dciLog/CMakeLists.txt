add_executable(dciLogger dciLogger.c dci_decode_multi_usrp.c read_cfg.c)
target_link_libraries(dciLogger srslte_phy srslte_common srslte_rf pthread config)

