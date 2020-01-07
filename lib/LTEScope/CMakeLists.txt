add_subdirectory(lib_src)
include_directories(lib_src)

add_executable(dci_multi multi_usrp.c dci_decode_multi_usrp.c read_cfg.c)
target_link_libraries(dci_multi srslte_phy srslte_common srslte_rf pthread config)

add_executable(remote_serv_test remote_serv_test.cpp read_cfg.c)
target_link_libraries(remote_serv_test srslte_phy srslte_common srslte_rf pthread config sockLib)

add_executable(test_caTrigger test_caTrigger.cpp dci_decode_multi_usrp.c read_cfg.c)
target_link_libraries(test_caTrigger srslte_phy srslte_common srslte_rf pthread config sockLib)

add_executable(cca_main_noRF cca_main_noRF.cpp dci_decode_multi_usrp.c read_cfg.c)
target_link_libraries(cca_main_noRF srslte_phy srslte_common srslte_rf pthread config sockLib)

add_executable(cca_main_remote cca_main_remote.cpp dci_decode_multi_usrp.c read_cfg.c)
target_link_libraries(cca_main_remote srslte_phy srslte_common srslte_rf pthread config sockLib)

add_executable(cca_main_server cca_main_server.cpp dci_decode_multi_usrp.c read_cfg.c)
target_link_libraries(cca_main_server srslte_phy srslte_common srslte_rf pthread config sockLib)

add_executable(cca_main cca_main.cpp dci_decode_multi_usrp.c read_cfg.c)
target_link_libraries(cca_main srslte_phy srslte_common srslte_rf pthread config sockLib)

add_executable(test_cca test_cca.cpp dci_decode_multi_usrp.c read_cfg.c)
target_link_libraries(test_cca srslte_phy srslte_common srslte_rf pthread config sockLib)

add_executable(ca_trigger_test ca_trigger_test.cpp dci_decode_multi_usrp.c read_cfg.c)
target_link_libraries(ca_trigger_test srslte_phy srslte_common srslte_rf pthread config sockLib)

add_executable(cca_test cca_test.cpp dci_decode_multi_usrp.c read_cfg.c)
target_link_libraries(cca_test srslte_phy srslte_common srslte_rf pthread config sockLib)

add_executable(dci_multi_log multi_usrp_log.c dci_decode_multi_usrp.c read_cfg.c)
target_link_libraries(dci_multi_log srslte_phy srslte_common srslte_rf pthread config)

add_executable(dci_logUE dci_log_UE.c dci_decode_multi_usrp.c read_cfg.c)
target_link_libraries(dci_logUE srslte_phy srslte_common srslte_rf pthread config)

