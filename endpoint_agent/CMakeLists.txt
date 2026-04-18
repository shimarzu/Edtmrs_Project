cmake_minimum_required(VERSION 3.16)
project(edtmrs_agent CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows only
if(NOT WIN32)
    message(FATAL_ERROR "EDTMRS Agent is Windows-only (requires WinAPI/WinHTTP/SetupAPI)")
endif()

add_executable(edtmrs_agent
    main.cpp
    device_monitor.cpp
    http_client.cpp
)

# Required Windows libraries
target_link_libraries(edtmrs_agent
    winhttp
    setupapi
    cfgmgr32
    ws2_32
)

# Subsystem: console for debugging, change to windows for silent operation
set_target_properties(edtmrs_agent PROPERTIES
    WIN32_EXECUTABLE FALSE   # FALSE = console window visible (for testing)
    # WIN32_EXECUTABLE TRUE  # TRUE  = silent background process
)

# Optimization for release
target_compile_options(edtmrs_agent PRIVATE
    $<$<CONFIG:Release>:/O2 /GL>
)
