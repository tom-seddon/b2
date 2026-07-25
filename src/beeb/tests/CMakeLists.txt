##########################################################################
##########################################################################

add_executable(test_6522 test_6522.cpp)
b2_target_boilerplate(test_6522 SANITIZERS)
target_link_libraries(test_6522 PRIVATE shared_lib 6502_lib beeb_lib)
add_test(
  NAME beeb/test_6522
  COMMAND $<TARGET_FILE:test_6522>)

##########################################################################
##########################################################################

add_executable(test_OutputDataBuffer test_OutputDataBuffer.cpp)
b2_target_boilerplate(test_OutputDataBuffer SANITIZERS)
target_link_libraries(test_OutputDataBuffer PRIVATE shared_lib beeb_lib)
add_test(
  NAME beeb/test_OutputDataBuffer
  COMMAND $<TARGET_FILE:test_OutputDataBuffer>)

##########################################################################
##########################################################################

if(MSVC)
  add_executable(test_relacy_OutputDataBuffer test_relacy_OutputDataBuffer.cpp)
  b2_target_boilerplate(test_relacy_OutputDataBuffer)
  target_link_libraries(test_relacy_OutputDataBuffer PRIVATE shared_lib beeb_lib relacy_lib)
  add_test(
    NAME beeb/test_relacy_OutputDataBuffer
    COMMAND $<TARGET_FILE:test_relacy_OutputDataBuffer>)
endif()

##########################################################################
##########################################################################

add_executable(test_beeb
  test_beeb.cpp test_beeb.inl)
target_compile_definitions(test_beeb PRIVATE
  -Db2_SOURCE_DIR="${b2_SOURCE_DIR}"
  -DBBC_TESTS_OUTPUT_FOLDER="${CMAKE_BINARY_DIR}/b2_tests_output.$<CONFIG>")
target_link_libraries(test_beeb PRIVATE shared_lib beeb_lib stb_image_lib)
b2_target_boilerplate(test_beeb SANITIZERS)

function(add_test_beeb_wildcard_category category)
  set(name "beeb/test_beeb ${category} ")
  add_test(NAME ${name}
    COMMAND $<TARGET_FILE:test_beeb> -T "${category}.*")
  add_test_boilerplate(${name})
endfunction()

function(add_test_beeb_literal_category test_name)
  set(name "beeb/test_beeb ${test_name} ")
  add_test(NAME ${name}
    COMMAND $<TARGET_FILE:test_beeb> -t "${test_name}")
  add_test_boilerplate(${name})
endfunction()

add_test(NAME beeb/test_beeb_list_tests
  COMMAND $<TARGET_FILE:test_beeb> --list-for-check_ctest_log)

add_test_beeb_wildcard_category(standard.VIA)
add_test_beeb_literal_category(standard.VTIMERS)
add_test_beeb_literal_category(standard.TIMINGS)
add_test_beeb_literal_category(standard.VTIMEOU)
add_test_beeb_literal_category(standard.VPOLL)
add_test_beeb_literal_category(standard.NOP1)

add_test_beeb_literal_category(tube.xtu_prst)
add_test_beeb_literal_category(tube.itu_prst)
add_test_beeb_literal_category(tube.xtu_r124)
add_test_beeb_literal_category(tube.xtu_r3)

add_test_beeb_literal_category(kevin_edwards.Alien8)
add_test_beeb_literal_category(kevin_edwards.Nightsh)
add_test_beeb_literal_category(kevin_edwards.Jetman)

add_test_beeb_literal_category(teletext.ENGTEST)
add_test_beeb_literal_category(teletext.RED)
add_test_beeb_literal_category(teletext.TELETST)
add_test_beeb_literal_category(teletext.TEST)
add_test_beeb_literal_category(teletext.FLASH1)

add_test_beeb_wildcard_category(dp111)

add_test_beeb_literal_category(video_ula.detect_nula)
add_test_beeb_wildcard_category(video_nula.detect_nula)

foreach(M RANGE 8)
  foreach(I RANGE 15)
    # I did "%02d" in C++, because it makes the file names easier to
    # read, and it's all tied together, so file name and test name are
    # the same.
    #
    # I didn't think about how annoying this might end up in CMake.
    set(I_STR ${I})
    if(I LESS 10)
      string(PREPEND I_STR 0)
    endif()
    add_test_beeb_literal_category(video_nula.scroll1.M${M}.O${I_STR})
    add_test_beeb_literal_category(video_nula.blank1.M${M}.N${I_STR})
  endforeach()
endforeach()

foreach(C RANGE 1)
  foreach(F RANGE 1)
    foreach(M RANGE 3)
      add_test_beeb_literal_category(video_ula.ulamode.C${C}.F${F}.M${M})
      add_test_beeb_wildcard_category(video_nula.ulamode.C${C}.F${F}.M${M})
    endforeach()
  endforeach()
endforeach()

foreach(BP RANGE 7)
  foreach(ACCESS r w)
    add_test_beeb_literal_category(debug.bp.b.BP${BP}.${ACCESS})
    add_test_beeb_literal_category(debug.bp.bplus.BP${BP}.${ACCESS})
    foreach(SYS RANGE 7)
      add_test_beeb_literal_category(debug.bp.master128.BP${BP}.SYS${SYS}.${ACCESS})
    endforeach()
  endforeach()
endforeach()

add_test_beeb_wildcard_category(printer)

add_test_beeb_literal_category(disk.floppy.b.sd.acorndfs)

foreach(DENSITY sd dd)
  foreach(TYPE watford.ddb2 watford.ddb3 opus challenger)
    add_test_beeb_literal_category(disk.floppy.b.${DENSITY}.${TYPE})
  endforeach()
endforeach()

add_test_beeb_literal_category(disk.floppy.bplus)

add_test_beeb_literal_category(disk.mmfs.b)
add_test_beeb_literal_category(disk.mmfs.bplus)
add_test_beeb_literal_category(disk.mmfs.electron)

foreach(ACCCON RANGE 3)
  foreach(MOS mos320 mos350)
    add_test_beeb_literal_category(disk.mmfs.master.${ACCCON}.${MOS})
    foreach(TYPE dfs adfs)
      add_test_beeb_literal_category(disk.floppy.master.${ACCCON}.${MOS}.${TYPE})
    endforeach()
  endforeach()

  foreach(MOS mos500 mos510 mosI510C mos511i)
    add_test_beeb_literal_category(disk.mmfs.compact.${ACCCON}.${MOS})
    add_test_beeb_literal_category(disk.floppy.compact.${ACCCON}.${MOS}.adfs)
  endforeach()
endforeach()

add_test_beeb_wildcard_category(disk.hard.master)

foreach(MODE 0 3 6)
  add_test_beeb_literal_category(disk.floppy.electron.adfs.${MODE})
  add_test_beeb_literal_category(disk.hard.electron.${MODE})
endforeach()

foreach(CPUTYPE nmos cmos)
  add_test_beeb_literal_category(beeb_6502_timings.timings.${CPUTYPE})
endforeach()

foreach(GRAB RANGE 0 46)
  add_test_beeb_literal_category(mode7dem.${GRAB})
endforeach()

foreach(TYPE b bplus electron master compact)
  foreach(WHICH stop nostop)
    add_test_beeb_literal_category(tests.${WHICH}.${TYPE})
  endforeach()
endforeach()

add_test_beeb_literal_category(electron.rom_unlock)
