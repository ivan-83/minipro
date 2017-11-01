
SET(MINIPRO_BIN		main.c
			minipro.c
			database.c
			liblcb/src/utils/ini.c
			liblcb/src/utils/sys.c
			liblcb/src/utils/buf_str.c)

ADD_EXECUTABLE(minipro ${MINIPRO_BIN})
SET_TARGET_PROPERTIES(minipro PROPERTIES LINKER_LANGUAGE C)
TARGET_LINK_LIBRARIES(minipro ${CMAKE_REQUIRED_LIBRARIES} ${CMAKE_EXE_LINKER_FLAGS})

INSTALL(TARGETS minipro RUNTIME DESTINATION bin)
