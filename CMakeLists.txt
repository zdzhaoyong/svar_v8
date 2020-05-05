cmake_minimum_required(VERSION 2.8)
project(svar_v8)

include(PICMake)
set(CMAKE_CXX_STANDARD 11)

pi_add_target(svar SHARED src REQUIRED System)

set_target_properties(svar PROPERTIES PREFIX "")
set_target_properties(svar PROPERTIES SUFFIX ".node")

pi_report_target()

INCLUDE_DIRECTORIES(/data/zhaoyong/Desktop/svar_v8/node_modules/nan /usr/local/include/node)

# Now do make install
get_property(TARGETS2COMPILE GLOBAL PROPERTY TARGETS2COMPILE)
pi_install(TARGETS ${TARGETS2COMPILE})
