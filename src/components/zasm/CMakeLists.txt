add_library(zasm STATIC
	debug_data.cpp
	eval.cpp
	serialize.cpp
	table.cpp
)
target_link_libraries(zasm PUBLIC base)
