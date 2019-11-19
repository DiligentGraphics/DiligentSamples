cmake_minimum_required (VERSION 3.6)

file(RELATIVE_PATH THIRD_PARTY_REL_PATH "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
install(FILES nuklear/Readme.md DESTINATION "Licenses/${THIRD_PARTY_REL_PATH}" RENAME Nuklear-Readme.md)
