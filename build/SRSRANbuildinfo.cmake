
execute_process(
COMMAND git rev-parse --abbrev-ref HEAD
WORKING_DIRECTORY "/home/yaxiong/research/ngscope_related/srsRAN_4G-release_23_04"
OUTPUT_VARIABLE GIT_BRANCH
OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
COMMAND git log -1 --format=%h
WORKING_DIRECTORY "/home/yaxiong/research/ngscope_related/srsRAN_4G-release_23_04"
OUTPUT_VARIABLE GIT_COMMIT_HASH
OUTPUT_STRIP_TRAILING_WHITESPACE
)

message(STATUS "Generating build_info.h")
configure_file(
  /home/yaxiong/research/ngscope_related/srsRAN_4G-release_23_04/lib/include/srsran/build_info.h.in
  /home/yaxiong/research/ngscope_related/srsRAN_4G-release_23_04/build/lib/include/srsran/build_info.h
)
