# Futures 

add_test_target(weave_nd_futures_unit_tests futures/just_works/unit.cpp)
add_test_target(weave_nd_futures_stress_tests futures/just_works/stress.cpp)

# Cancellation

add_test_target(weave_nd_cancel_unit_tests cancel/just_works/unit.cpp)