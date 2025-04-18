# libgeneral-writerx
set( SRC
	general-writer.cpp
	utf8-like-int-codec.c
)

GenerateStaticLibs( general-writer "${SRC}" )

# gw-dumper

add_executable( gw-dumper gw-dumper.cpp )
add_dependencies ( gw-dumper general-writer )
target_link_libraries ( gw-dumper
    general-writer
	${COMMON_LIBS_READ}
)

if( RUN_SANITIZER_TESTS )
	add_executable( gw-dumper-asan gw-dumper.cpp )
	add_dependencies ( gw-dumper-asan general-writer-asan )
	target_link_libraries ( gw-dumper-asan
		general-writer-asan
		${COMMON_LIBS_READ}
	)
	target_compile_options( gw-dumper-asan PRIVATE -fsanitize=address )
	target_link_options( gw-dumper-asan PRIVATE -fsanitize=address )

	add_executable( gw-dumper-tsan gw-dumper.cpp )
	add_dependencies ( gw-dumper-tsan general-writer-tsan )
	target_link_libraries ( gw-dumper-tsan
		general-writer-tsan
		${COMMON_LIBS_READ}
	)
	target_compile_options( gw-dumper-tsan PRIVATE -fsanitize=thread )
	target_link_options( gw-dumper-tsan PRIVATE -fsanitize=thread )
endif()

if ( NOT TOOLS_ONLY )
    add_subdirectory( test )
endif()
