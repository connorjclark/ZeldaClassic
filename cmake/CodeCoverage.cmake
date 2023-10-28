if(NOT MSVC AND NOT WANT_PGO)
	# For the coverage report.
	set(CMAKE_CXX_FLAGS_COVERAGE "-g -O0 --coverage")
endif()
