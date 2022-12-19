# Authored by Graham Dianaty for Bitlogix Technologies. Based on code from Rene Hollander. ==========//
# Modified by Olivier Pelgrin 2022-12-08
# - Fixed get_variable to work with my version of CMake.js (6.3.2)
# - Do not install cmake-js
# - Removed unnecessary checks (for me) for npm and cmake-js
# - Only look for CMAKE_JS_INC variable content
# - Do not check for build type (it's irrelevant to CMAKE_JS_INC)
function(setup_cmakejs)
    function(GET_VARIABLE INPUT_STRING VARIABLE_TO_SELECT OUTPUT_VARIABLE)
        set(SEARCH_STRING "${VARIABLE_TO_SELECT}=")
        string(LENGTH "${SEARCH_STRING}" SEARCH_STRING_LENGTH)
        string(LENGTH "${INPUT_STRING}" INPUT_STRING_LENGTH)

        string(FIND "${INPUT_STRING}" "${SEARCH_STRING}" SEARCH_STRING_INDEX)

        math(EXPR SEARCH_STRING_INDEX "${SEARCH_STRING_INDEX}+${SEARCH_STRING_LENGTH}")

        string(SUBSTRING "${INPUT_STRING}" ${SEARCH_STRING_INDEX} ${INPUT_STRING_LENGTH} AFTER_SEARCH_STRING)
        string(FIND "${AFTER_SEARCH_STRING}" "'" QUOTE_INDEX)
        string(SUBSTRING "${AFTER_SEARCH_STRING}" "0" "${QUOTE_INDEX}" RESULT_STRING)
        set("${OUTPUT_VARIABLE}" "${RESULT_STRING}" PARENT_SCOPE)
    endfunction(GET_VARIABLE)

    execute_process(COMMAND npx cmake-js print-configure OUTPUT_VARIABLE CMAKE_JS_OUTPUT WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    message(VERBOSE ${CMAKE_JS_OUTPUT})

    get_variable("${CMAKE_JS_OUTPUT}" "CMAKE_JS_INC" CMAKE_JS_INC)
    set(CMAKE_JS_INC "${CMAKE_JS_INC}" PARENT_SCOPE)
    message(VERBOSE "CMAKE_JS_INC=${CMAKE_JS_INC}")

    message(STATUS "[CMakeJS] Set up variables.")
endfunction(setup_cmakejs)
