cmake_minimum_required(VERSION 2.8)

project(ProcessesLogger)	

# log4cplus
option(LOG4CPLUS_PATH "path to library log4cplus, default path is /opt/liblog4cplus-armhf"
       	OFF)
IF (NOT LOG4CPLUS_PATH)
   set(LOG4CPLUS_PATH "/opt/liblog4cplus-armhf")
endif(NOT LOG4CPLUS_PATH)

string(CONCAT LOG4CPLUS_INCL_P ${LOG4CPLUS_PATH} "/include")
string(CONCAT LOG4CPLUS_LIB_P ${LOG4CPLUS_PATH} "/lib")

set(SOURCE_EXE ProcessInfo.cpp
	../inih/cpp/INIReader.cpp
	../inih/cpp/INIReader.h
	../inih/ini.c
	../inih/ini.h
		ProcessInfo.cpp)

# Влючаем поддержку стандарта C++11
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories(${LOG4CPLUS_INCL_P})
include_directories("../inih")

link_directories(${LOG4CPLUS_LIB_P})

set(LIBS log4cplus)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")
add_executable(ProcessesLogger ${SOURCE_EXE})
target_link_libraries(ProcessesLogger ${LIBS})

