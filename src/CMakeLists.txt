
set(MINIPRO_BIN		main.c
			minipro.c
			database.c
			liblcb/src/utils/ini.c
			liblcb/src/utils/sys.c
			liblcb/src/utils/buf_str.c)
add_executable(minipro ${MINIPRO_BIN})
set_target_properties(minipro PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(minipro ${CMAKE_REQUIRED_LIBRARIES} ${CMAKE_EXE_LINKER_FLAGS})
install(TARGETS minipro RUNTIME DESTINATION bin)

set(INFOIC_BIN		infoic.c)
add_executable(infoic ${INFOIC_BIN})
set_target_properties(infoic PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(infoic ${CMAKE_EXE_LINKER_FLAGS})
install(TARGETS infoic RUNTIME DESTINATION bin)
