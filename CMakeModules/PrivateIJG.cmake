include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(lwe_ijg
    URL https://ijg.org/files/jpegsrc.v10.tar.gz
    URL_HASH SHA256=8b9eaa13242690ebd03e1728ab1edf97a81a78ed6e83624d493655f31ac95ab5)
FetchContent_MakeAvailable(lwe_ijg)

# IJG ships this configuration template for ANSI compilers. Build only the
# decoder and bind its symbols locally so host libjpeg consumers keep their ABI.
configure_file("${lwe_ijg_SOURCE_DIR}/jconfig.txt" "${lwe_ijg_BINARY_DIR}/jconfig.h" COPYONLY)
set(LWE_IJG_DECODE_SOURCES
    jaricom.c jcomapi.c jdapimin.c jdapistd.c jdarith.c jdatasrc.c jdcoefct.c
    jdcolor.c jddctmgr.c jdhuff.c jdinput.c jdmainct.c jdmarker.c jdmaster.c
    jdmerge.c jdpostct.c jdsample.c jerror.c jidctflt.c jidctfst.c jidctint.c
    jmemmgr.c jmemnobs.c jquant1.c jquant2.c jutils.c)
list(TRANSFORM LWE_IJG_DECODE_SOURCES PREPEND "${lwe_ijg_SOURCE_DIR}/")
add_library(lwe-private-jpeg STATIC
    ${LWE_IJG_DECODE_SOURCES}
    src/WallpaperEngine/Data/Parsers/TextureJpegDecoder.c)
target_include_directories(lwe-private-jpeg PRIVATE "${lwe_ijg_SOURCE_DIR}" "${lwe_ijg_BINARY_DIR}")
set_target_properties(lwe-private-jpeg PROPERTIES
    POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden C_STANDARD 99 C_STANDARD_REQUIRED ON)

install(FILES "${lwe_ijg_SOURCE_DIR}/README" "${CMAKE_CURRENT_LIST_DIR}/../licenses/ijg/NOTICE"
    DESTINATION licenses/ijg)
