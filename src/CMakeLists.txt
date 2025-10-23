add_library(emxu8 SHARED
		emxu8.cpp
		executor.cpp
		fetcher.cpp
		decoder.cpp
		interrupter.cpp
		disassembler.cpp
)

target_include_directories(emxu8 PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_compile_definitions(emxu8 PRIVATE BUILDING_EMXU8)
target_compile_features(emxu8 PUBLIC cxx_std_23)

set_target_properties(emxu8 PROPERTIES
		PREFIX ""
		RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
		LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
