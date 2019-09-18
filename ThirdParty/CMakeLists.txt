cmake_minimum_required (VERSION 3.6)

add_subdirectory(TwBarLib)

file(RELATIVE_PATH SAMPLES_DIR_PATH "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/..")
install(FILES TwBarLib/License.txt DESTINATION "${SAMPLES_DIR_PATH}/Licenses/ThirdParty" RENAME TwBarLib-License.txt)
