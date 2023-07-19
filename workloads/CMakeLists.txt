project_log("Workloads")

add_nontest_target(weave_workloads_mutex mutex.cpp)
add_nontest_target(weave_workloads_mutex_unstable mutex_unstable.cpp)

add_nontest_target(weave_workloads_yield yield.cpp)

add_nontest_target(weave_workloads_yield_pooling1 yield_bad_pooling.cpp)
add_nontest_target(weave_workloads_yield_pooling2 yield_good_pooling.cpp)

add_nontest_target(weave_workloads_channels channels.cpp)

add_nontest_target(weave_workloads_bursts bursts.cpp)

add_nontest_target(weave_workloads_futures futures.cpp)

add_nontest_target(weave_workloads_racy racy.cpp)

add_custom_target(weave_worksloads ALL 
                  DEPENDS
                  weave_workloads_mutex
                  weave_workloads_mutex_unstable
                  weave_workloads_yield
                  weave_workloads_yield_pooling1
                  weave_workloads_yield_pooling2
                  weave_workloads_channels
                  weave_workloads_bursts
                  weave_workloads_futures
                  weave_workloads_racy)

