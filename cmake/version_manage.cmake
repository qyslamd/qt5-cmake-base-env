function(manage_version)
    set(oneValueArgs VERSION_FILE OUTPUT_PREFIX)
    cmake_parse_arguments(MV "" "${oneValueArgs}" "" ${ARGN})

    if(NOT MV_VERSION_FILE)
        set(MV_VERSION_FILE ${CMAKE_SOURCE_DIR}/src/app/version.txt)
    endif()

    if(NOT MV_OUTPUT_PREFIX)
        set(MV_OUTPUT_PREFIX "APP")
    endif()

    if(NOT EXISTS ${MV_VERSION_FILE})
        file(WRITE ${MV_VERSION_FILE}
            "major=1\nminor=0\npatch=0\ntweak=0\n")
    endif()

    file(READ ${MV_VERSION_FILE} VERSION_CONTENT)
    set(VERSION_REGEX "major=([0-9]+)[ \t\r\n]*minor=([0-9]+)[ \t\r\n]*patch=([0-9]+)[ \t\r\n]*tweak=([0-9]+)[ \t\r\n]*")
    if(NOT VERSION_CONTENT MATCHES ${VERSION_REGEX})
        message(FATAL_ERROR "Invalid version file format: ${MV_VERSION_FILE}")
    endif()

    set(MAJOR ${CMAKE_MATCH_1})
    set(MINOR ${CMAKE_MATCH_2})
    set(PATCH ${CMAKE_MATCH_3})
    set(TWEAK ${CMAKE_MATCH_4})

    math(EXPR NEW_TWEAK "${TWEAK} + 1")
    file(WRITE ${MV_VERSION_FILE}
        "major=${MAJOR}\nminor=${MINOR}\npatch=${PATCH}\ntweak=${NEW_TWEAK}\n")

    set(${MV_OUTPUT_PREFIX}_VERSION_MAJOR ${MAJOR} PARENT_SCOPE)
    set(${MV_OUTPUT_PREFIX}_VERSION_MINOR ${MINOR} PARENT_SCOPE)
    set(${MV_OUTPUT_PREFIX}_VERSION_PATCH ${PATCH} PARENT_SCOPE)
    set(${MV_OUTPUT_PREFIX}_VERSION_TWEAK ${NEW_TWEAK} PARENT_SCOPE)
    set(${MV_OUTPUT_PREFIX}_VERSION
        "${MAJOR}.${MINOR}.${PATCH}.${NEW_TWEAK}" PARENT_SCOPE)
endfunction()
