cmake_minimum_required(VERSION 3.0.0)
project(stdx VERSION 0.1.0)

set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Werror -ggdb3 -fno-stack-protector -pthread"
)


add_library(stdx STATIC)
#add_library(stdx SHARED)

#target_link_libraries(stdx -static-libgcc -static-libstdc++)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED true)

target_include_directories(stdx PUBLIC ${PROJECT_SOURCE_DIR}/include)


aux_source_directory(${PROJECT_SOURCE_DIR}/src  src)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/tcp  src_tcp)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/thread src_thread)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/timer src_timer)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/ioevent src_event)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/log src_log)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/ini src_ini)

target_sources(stdx PUBLIC ${src} ${src_tcp} ${src_thread} ${src_timer} ${src_event} ${src_log} ${src_ini})


#testing below
enable_testing()

#testing task_thread_pool
add_executable(test_task_thread_pool ${PROJECT_SOURCE_DIR}/testing/thread/test_task_thread_pool.cpp)
target_link_libraries(test_task_thread_pool stdx)

target_include_directories(test_task_thread_pool PUBLIC ${PROJECT_SOURCE_DIR}/include)


add_test(NAME test_task_thread_pool_multi COMMAND test_task_thread_pool muti_task WORKING_DIRECTORY ${PROJECT_BINARY_DIR} )
add_test(NAME test_task_thread_pool_return COMMAND test_task_thread_pool async_return WORKING_DIRECTORY ${PROJECT_BINARY_DIR} )
add_test(NAME test_task_thread_pool_wait COMMAND test_task_thread_pool wait WORKING_DIRECTORY ${PROJECT_BINARY_DIR} )
add_test(NAME test_task_thread_pool_usage COMMAND test_task_thread_pool usage WORKING_DIRECTORY ${PROJECT_BINARY_DIR} )

#testing tcp_lister
add_executable(test_tcp_listener ${PROJECT_SOURCE_DIR}/testing/tcp/test_tcp_listener.cpp)

target_include_directories(test_tcp_listener PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_tcp_listener stdx)

#testing timer
add_executable(test_timer ${PROJECT_SOURCE_DIR}/testing/timer/test_timer.cpp)
target_include_directories(test_timer PUBLIC ${PROJECT_SOURCE_DIR}/include/)
target_link_libraries(test_timer stdx)

add_test(NAME test_timer_add1 COMMAND test_timer add1 WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
add_test(NAME test_timer_del COMMAND test_timer del WORKING_DIRECTORY ${PROJECT_BINARY_DIR})

#testing packet
add_executable(test_packet ${PROJECT_SOURCE_DIR}/testing/tcp/test_packet.cpp)
target_include_directories(test_packet PUBLIC ${PROJECT_SOURCE_DIR}/include/)
target_link_libraries(test_packet stdx)

add_test(NAME test_packet_serialize COMMAND test_packet s WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
add_test(NAME test_packet_serialize1 COMMAND test_packet s1 WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
add_test(NAME test_packet_serialize2 COMMAND test_packet s2 WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
add_test(NAME test_packet_unserialize COMMAND test_packet u WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
add_test(NAME test_packet_unserialize1 COMMAND test_packet u1 WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
add_test(NAME test_packet_unserialize2 COMMAND test_packet u2 WORKING_DIRECTORY ${PROJECT_BINARY_DIR})

#testing log
add_executable(test_log ${PROJECT_SOURCE_DIR}/testing/log/test_log.cpp)
target_include_directories(test_log PUBLIC ${PROJECT_SOURCE_DIR}/include/)
target_link_libraries(test_log stdx)

add_test(NAME test_log_file COMMAND test_log file WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
add_test(NAME test_log_nofile COMMAND test_log nofile WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
add_test(NAME test_log_info COMMAND test_log info WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
add_test(NAME test_log_debug COMMAND test_log debug WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
add_test(NAME test_log_warn COMMAND test_log warn WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
add_test(NAME test_log_error COMMAND test_log error WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
add_test(NAME test_log_format COMMAND test_log fmt WORKING_DIRECTORY ${PROJECT_BINARY_DIR})

#test ini
add_executable(test_ini ${PROJECT_SOURCE_DIR}/testing/ini/test_ini.cpp)
target_include_directories(test_ini PUBLIC ${PROJECT_SOURCE_DIR}/include/)
target_link_libraries(test_ini stdx)

add_test(NAME test_ini_str COMMAND test_ini str WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
add_test(NAME test_ini_file COMMAND test_ini file WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
add_test(NAME test_ini_trim COMMAND test_ini trim WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
add_test(NAME test_ini_lower COMMAND test_ini lower WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
add_test(NAME test_ini_save COMMAND test_ini save WORKING_DIRECTORY ${PROJECT_BINARY_DIR})