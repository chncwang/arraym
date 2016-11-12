set(CMAKE_VERBOSE_MAKEFILE ON)

# project global options
option(WITH_EXTRA_RUNTIME_CHECKS "Performs extra checks at runtime" ON)
option(WITH_OMP "Enable multithreading of NLL using OpenMP" ON)
option(WITH_CUDA "Activate modules using CUDA. It requires the CUDA SDK and code examples to be installed" ON)
option(WITH_OPENBLAS "Activate modules using OpenBLAS. It requires the OpenBLAS install" ON)
option(WITH_CPPCHECK "Activate static code analysis with cppcheck" ON)
option(WITH_ZLIB "Enable compression using zlib" ON)
option(WITH_BOOSTSIMD "Enable Boost.SIMD for vectorization" ON)
option(WITH_DCMTK "Enables DICOM reading/writing" ON)
option(WITH_NUMPY "Enables numpy to and from conversions" ON)
option(WITH_COMPANY "Enables private components" ON)
option(WITH_VOXELIZER "Voxelizer engine" OFF)