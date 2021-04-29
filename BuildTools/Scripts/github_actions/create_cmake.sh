cd $1
echo "cmake_minimum_required(VERSION 3.6)" > CMakeLists.txt
echo "Project(DiligentSamples_Test)" >> CMakeLists.txt
echo "add_subdirectory(DiligentCore)" >> CMakeLists.txt
echo "add_subdirectory(DiligentTools)" >> CMakeLists.txt
echo "add_subdirectory(DiligentFX)" >> CMakeLists.txt
echo "add_subdirectory(DiligentSamples)" >> CMakeLists.txt
