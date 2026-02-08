cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 读取 .env 文件并设置编译定义
if(EXISTS "${CMAKE_SOURCE_DIR}/.env")
    file(STRINGS "${CMAKE_SOURCE_DIR}/.env" ENV_VARS)
    foreach(VAR ${ENV_VARS})
        # 跳过注释和空行
        if(NOT VAR MATCHES "^#" AND NOT VAR STREQUAL "")
            string(REGEX MATCH "^[A-Za-z_][A-Za-z0-9_]*=" VAR_NAME "${VAR}")
            if(VAR_NAME)
                string(REPLACE "=" ";" VAR_LIST "${VAR}")
                list(GET VAR_LIST 0 VAR_KEY)
                string(REPLACE "${VAR_KEY}=" "" VAR_VALUE "${VAR}")
                message(STATUS "Found env var: ${VAR_KEY}=${VAR_VALUE}")
                # 使用引号包裹值，确保特殊字符正确处理
                add_definitions("-D${VAR_KEY}=\"${VAR_VALUE}\"")
            endif()
        endif()
    endforeach()
endif()

project(sg90_servo_project)
