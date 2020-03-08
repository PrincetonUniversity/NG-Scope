LINK_DIRECTORIES(../lib_dci/)
INCLUDE_DIRECTORIES(../lib_dci/)

add_executable(testDCI testDCI.cpp)
target_link_libraries(testDCI  dciLib srslte_phy srslte_common srslte_rf pthread config)

