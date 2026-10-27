# PATCH_COMMAND helper: applies PATCH (a git diff) to the current directory unless it is already in.
# FetchContent runs the patch step again on a reconfigure, or after a CI cache restored the patched
# source, and a second plain `git apply` fails there.
#   cmake -DGIT=<git> -DPATCH=<file> -P apply_once.cmake
execute_process(COMMAND "${GIT}" apply --ignore-whitespace --reverse --check "${PATCH}"
                RESULT_VARIABLE already_applied OUTPUT_QUIET ERROR_QUIET)
if(already_applied EQUAL 0)
	message(STATUS "Already applied: ${PATCH}")
	return()
endif()
execute_process(COMMAND "${GIT}" apply --ignore-whitespace "${PATCH}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
	message(FATAL_ERROR "Could not apply ${PATCH}")
endif()
