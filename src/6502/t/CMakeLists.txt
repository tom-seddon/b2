##########################################################################
##########################################################################

add_executable(6502_basic_tests 6502_basic_tests.cpp)
target_link_libraries(6502_basic_tests PRIVATE shared_lib 6502_lib)
b2_target_boilerplate(6502_basic_tests SANITIZERS)

add_test(
  NAME 6502/6502_basic_tests
  COMMAND $<TARGET_FILE:6502_basic_tests> ${CMAKE_CURRENT_LIST_DIR}/../../../etc/cmos_adc_sbc/)

##########################################################################
##########################################################################

add_executable(lorenz lorenz.cpp)
target_compile_definitions(lorenz PRIVATE -DLORENZ_FOLDER_NAME="${CMAKE_SOURCE_DIR}/etc/testsuite-2.15/ascii-bin/")
target_link_libraries(lorenz PRIVATE shared_lib 6502_lib)
b2_target_boilerplate(lorenz SANITIZERS)

add_test(
  NAME 6502/lorenz
  COMMAND $<TARGET_FILE:lorenz>)

##########################################################################
##########################################################################

add_executable(klaus klaus.cpp)
target_compile_definitions(klaus PRIVATE -DKLAUS_FOLDER_NAME="${CMAKE_SOURCE_DIR}/etc/6502_65C02_functional_tests/")
target_link_libraries(klaus PRIVATE shared_lib 6502_lib)
b2_target_boilerplate(klaus SANITIZERS)

add_test(
  NAME klaus
  COMMAND $<TARGET_FILE:klaus>)

##########################################################################
##########################################################################

add_executable(visual6502 visual6502.cpp)
target_compile_definitions(visual6502 PRIVATE -DVISUAL6502_PATH="${b2_SOURCE_DIR}/submodules/visual6502")
target_link_libraries(visual6502 PRIVATE shared_lib 6502_lib perfect6502_lib)
b2_target_boilerplate(visual6502 SANITIZERS)

add_test(
  NAME 6502/visual6502/list
  COMMAND $<TARGET_FILE:visual6502> --list-for-check_ctest_log)

# the mix runs up to 390 cycles.
foreach(I RANGE 3)
  add_test(
    NAME 6502/visual6502/mix/${I}xx
    COMMAND $<TARGET_FILE:visual6502> -T "mix.${I}*")
endforeach()

add_test(
 NAME 6502/visual6502/rest
 COMMAND $<TARGET_FILE:visual6502> -X "mix.*")


##########################################################################
##########################################################################

add_executable(tom_seddon_6502_tests tom_seddon_6502_tests.cpp)
target_compile_definitions(tom_seddon_6502_tests PRIVATE -DSRC_PATH="${b2_SOURCE_DIR}/submodules/6502-tests/releases/6502-tests-generic.20221022T231937.dat")
target_link_libraries(tom_seddon_6502_tests PRIVATE shared_lib 6502_lib)
b2_target_boilerplate(tom_seddon_6502_tests SANITIZERS)

add_test(
  NAME 6502/tom_seddon_6502_tests/list
  COMMAND $<TARGET_FILE:tom_seddon_6502_tests> --list-for-check_ctest_log)

function(add_tom_seddon_6502_test instr)
  add_test(
    NAME 6502/tom_seddon_6502_tests/${instr}
    COMMAND $<TARGET_FILE:tom_seddon_6502_tests> -T ${instr}.*)
endfunction()

add_tom_seddon_6502_test(adc)
add_tom_seddon_6502_test(sbc)
add_tom_seddon_6502_test(arr)
add_tom_seddon_6502_test(asr)
add_tom_seddon_6502_test(anc)
add_tom_seddon_6502_test(anc2)
add_tom_seddon_6502_test(dcp)
add_tom_seddon_6502_test(isb)
add_tom_seddon_6502_test(lax)
add_tom_seddon_6502_test(lds)
add_tom_seddon_6502_test(rla)
add_tom_seddon_6502_test(rra)
add_tom_seddon_6502_test(sax)
add_tom_seddon_6502_test(slo)
add_tom_seddon_6502_test(sre)
add_tom_seddon_6502_test(usbc)
