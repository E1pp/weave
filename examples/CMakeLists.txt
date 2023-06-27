# Examples

add_nontest_target(examples_executors executors/main.cpp)
add_nontest_target(examples_futures futures/main.cpp)
add_nontest_target(examples_fibers fibers/main.cpp)

add_custom_target(weave_examples ALL DEPENDS 
                  examples_executors
                  examples_futures
                  examples_fibers)