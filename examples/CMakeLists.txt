# Examples

add_nontest_target(weave_examples_executors executors/main.cpp)
add_nontest_target(weave_examples_futures futures/main.cpp)
add_nontest_target(weave_examples_fibers fibers/main.cpp)
add_nontest_target(weave_examples_timers timers/main.cpp)
add_nontest_target(weave_examples_cancel cancel/main.cpp)

add_custom_target(weave_examples ALL DEPENDS 
                  weave_examples_executors
                  weave_examples_futures
                  weave_examples_fibers
                  weave_examples_timers
                  weave_examples_cancel)