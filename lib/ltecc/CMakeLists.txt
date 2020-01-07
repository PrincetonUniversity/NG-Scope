add_executable(cca_main cca_main.cpp dci_decode_multi_usrp.c read_cfg.c)
target_link_libraries(cca_main srslte_phy srslte_common srslte_rf pthread config sockLib)

