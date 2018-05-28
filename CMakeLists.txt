#
# Cmake configuration file
#

############################# INITIAL SECTION ##########################
cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(minipro C)

set(PACKAGE_VERSION_MAJOR	1)
set(PACKAGE_VERSION_MINOR	2)
set(PACKAGE_VERSION_PATCH	0)

set(PACKAGE_NAME		"${PROJECT_NAME}")
set(PACKAGE_VERSION		"${PACKAGE_VERSION_MAJOR}.${PACKAGE_VERSION_MINOR}.${PACKAGE_VERSION_PATCH}")
set(PACKAGE_URL			"https://github.com/rozhuk-im/minipro")
set(PACKAGE_BUGREPORT		"https://github.com/rozhuk-im/minipro")
set(PACKAGE_STRING 		"${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_DESCRIPTION		"A free and open TL866XX programmer")
set(PACKAGE_TARNAME		"${PACKAGE_NAME}-${PACKAGE_VERSION}")

############################# OPTIONS SECTION ##########################

option(ENABLE_COVERAGE		"Build with code coverage options [default: OFF]"	OFF)
option(ENABLE_FULL_DEBUG	"Build with all possible debug [default: OFF]"		OFF)

# Now CMAKE_INSTALL_PREFIX is a base prefix for everything
IF(NOT SHARE_DIR)
	set(SHARE_DIR "${CMAKE_INSTALL_PREFIX}/share/minipro")
ENDIF()

############################# INCLUDE SECTION ##########################

INCLUDE(CheckFunctionExists)
INCLUDE(CheckCCompilerFlag)

find_package(PkgConfig REQUIRED)
pkg_search_module(LIBUSB1 REQUIRED libusb-1.0)
include_directories(SYSTEM ${LIBUSB1_INCLUDE_DIRS})
list(APPEND CMAKE_REQUIRED_LIBRARIES ${LIBUSB1_LIBRARIES})

############################# MACRO SECTION ############################
macro(try_c_flag prop flag)
	# Try flag once on the C compiler
	check_c_compiler_flag("-Werror ${flag}" C_FLAG_${prop})
	if (C_FLAG_${prop})
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
	endif()
endmacro()

macro(try_linker_flag prop flag)
	# Check with the C compiler
	set(CMAKE_REQUIRED_FLAGS ${flag})
	check_c_compiler_flag(${flag} LINKER_FLAG_${prop})
	set(CMAKE_REQUIRED_FLAGS "")
	if (LINKER_FLAG_${prop})
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${flag}")
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${flag}")
		set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${flag}")
	endif()
endmacro()

############################# CONFIG SECTION ###########################

# Prefer local include dirs to system ones.
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/"
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/liblcb/include"
		"${CMAKE_BINARY_DIR}/src")

set(TAR "tar")

# Platform specific configuration.
if (CMAKE_SYSTEM_NAME MATCHES "^.*BSD$|DragonFly")
	add_definitions(-D_BSD_SOURCE -DFREEBSD)
	message(STATUS "Configuring for BSD system")
	include_directories("/usr/local/include/")
	set(TAR "gtar")
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	add_definitions(-D_BSD_SOURCE -DDARWIN)
	message(STATUS "Configuring for Darwin")
	set(TAR "gnutar")
endif()


if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	add_definitions(-D_GNU_SOURCE -DLINUX -D__USE_GNU=1)
	message(STATUS "Configuring for Linux")
	if (BUILD_CPU_MODE STREQUAL "32")
		add_definitions(-D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE)
	endif()

	list(APPEND CMAKE_REQUIRED_LIBRARIES rt)
endif()


try_c_flag(PIPE "-pipe")

# Process with warn flags.
if (NOT CMAKE_C_OPT_FLAGS AND ENABLE_FULL_DEBUG)
	try_c_flag(W			"-W")
	try_c_flag(WALL			"-Wall")
	try_c_flag(WPOINTER		"-Wpointer-arith")
	try_c_flag(WPARAM		"-Wno-unused-parameter")
	try_c_flag(WFUNCTION		"-Wno-unused-function")
	try_c_flag(WUNUSED_VAR		"-Wno-unused-variable")
	try_c_flag(WPOINTER_SIGN	"-Wno-pointer-sign")
	try_c_flag(WSIGN_COMPARE	"-Wno-sign-compare")
	try_c_flag(WSTRICT_PROTOTYPES	"-Wstrict-prototypes")
	try_c_flag(PEDANTIC		"-pedantic")
	try_c_flag(WNO_UNUSED_CONST	"-Wno-unused-const-variable")
	try_c_flag(WNULL_DEREFERENCE	"-Wnull-dereference")
	try_c_flag(WDUPLICATED_COND	"-Wduplicated-cond")
	try_c_flag(WIMPLICIT_FALLTHROUGH "-Wimplicit-fallthrough")
endif()

if (NOT "${CMAKE_C_COMPILER_ID}" MATCHES SunPro)
	try_c_flag(STD11		"-std=c11")
	if (NOT C_FLAG_STD11)
		try_c_flag(STD99	"-std=c99")
	endif()
endif()


# Hardening flags
try_c_flag(FORTIFY_SOURCE2		"-D_FORTIFY_SOURCE=2")
try_c_flag(FSTACK_PROTECTOR_ALL		"-fstack-protector-all")
try_c_flag(FSANITIZE_SAFE_STACK		"-fsanitize=safe-stack")
try_c_flag(FSANITIZE_CFI		"-fsanitize=cfi")
try_c_flag(MRETPOLINE			"-mretpoline")
try_c_flag(MFUNCTION_RETURN		"-mfunction-return=thunk")
try_c_flag(MINDIRECT_BRANCH		"-mindirect-branch=thunk")
try_c_flag(FWRAPV			"-fwrapv")
try_c_flag(FPIE				"-fPIE")
try_linker_flag(PIE			"-pie")
try_linker_flag(RETPOLINEPLT		"-Wl,-z,retpolineplt")
try_linker_flag(ZRELRO			"-Wl,-z,relro")
try_linker_flag(ZNOW			"-Wl,-z,now")
try_linker_flag(ZNOEXECSTACK		"-Wl,-z,noexecstack")


# Optimization flags
if (ENABLE_COVERAGE)
	set(CMAKE_C_OPT_FLAGS "-g -O0 -fno-strict-aliasing")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
else()
	if (NOT CMAKE_C_OPT_FLAGS)
		if (ENABLE_FULL_DEBUG)
			set(CMAKE_C_OPT_FLAGS "-g -O0 -fstrict-aliasing")
		else()
			set(CMAKE_C_OPT_FLAGS "-g -O2 -fstrict-aliasing")
		endif()
	endif()
endif()


set(CMAKE_C_FLAGS "${CMAKE_C_OPT_FLAGS} ${CMAKE_C_FLAGS}")
# Silently strip whitespace
string(STRIP "${CMAKE_C_FLAGS}" CMAKE_C_FLAGS)
string(STRIP "${CMAKE_EXE_LINKER_FLAGS}" CMAKE_EXE_LINKER_FLAGS)
string(STRIP "${CMAKE_MODULE_LINKER_FLAGS}" CMAKE_MODULE_LINKER_FLAGS)
string(STRIP "${CMAKE_SHARED_LINKER_FLAGS}" CMAKE_SHARED_LINKER_FLAGS)


# Check platform specific includes.
#check_include_files(sys/types.h		HAVE_SYS_TYPES_H)

# Check platform API.
check_function_exists(memset_s			HAVE_MEMSET_S)
check_function_exists(explicit_bzero		HAVE_EXPLICIT_BZERO)
check_function_exists(memrchr			HAVE_MEMRCHR)
check_function_exists(memmem			HAVE_MEMMEM)
check_function_exists(strncasecmp		HAVE_STRNCASECMP)
check_function_exists(reallocarray		HAVE_REALLOCARRAY)

configure_file(config.h.cmake src/config.h)
add_definitions(-DHAVE_CONFIG_H)

################################ SUBDIRS SECTION #######################

add_subdirectory(src)

############################ TARGETS SECTION ###########################

add_custom_target(dist ${CMAKE_SOURCE_DIR}/dist.sh
	"${CMAKE_BINARY_DIR}/minipro-${MINIPRO_VERSION}.tar.xz" "${TAR}"
	COMMENT "Create source distribution"
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

##################### INSTALLATION #####################################

# Text files
install(FILES "minipro_db.ini" DESTINATION ${SHARE_DIR})
install(FILES "README.md" DESTINATION ${SHARE_DIR})
