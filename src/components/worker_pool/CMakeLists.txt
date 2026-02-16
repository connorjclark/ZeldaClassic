add_library(worker_pool STATIC
	worker_pool.cpp
)
target_link_libraries(worker_pool PUBLIC base allegro_with_legacy)
