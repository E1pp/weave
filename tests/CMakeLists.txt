# Tests

# Wait group
add_test_target(weave_wg_unit_tests threads/wait_group/unit.cpp)
add_test_target(weave_wg_stress_tests threads/wait_group/stress.cpp)
add_test_target(weave_wg_storage_tests threads/wait_group/storage.cpp)
add_test_target(weave_wg_futex_tests threads/wait_group/futex.cpp)

# Lockfree 

# Stack
add_test_target(weave_stack_unit_tests threads/lockfree/stack/unit.cpp)
add_test_target(weave_stack_stress_tests threads/lockfree/stack/stress.cpp)

# Queue
add_test_target(weave_queue_unit_tests threads/lockfree/queue/unit.cpp)
add_test_target(weave_queue_stress_tests threads/lockfree/queue/stress.cpp)

# Executors

# Thread Pool

add_test_target(weave_tp_unit_tests executors/thread_pool/just_works/unit.cpp)
add_test_target(weave_tp_stress_tests executors/thread_pool/just_works/stress.cpp)

# WaitIdle
add_test_target(weave_tp_wait_idle_unit_tests executors/thread_pool/wait_idle/unit.cpp)
add_test_target(weave_tp_wait_idle_stress_tests executors/thread_pool/wait_idle/stress.cpp)

# Parking + Balancing
add_test_target(weave_weave_tp_balancing_stress_tests executors/thread_pool/balancing/stress.cpp)

# Manual
add_test_target(weave_manual_unit_tests executors/manual/unit.cpp)

# Strand
add_test_target(weave_strand_unit_tests executors/strand/unit.cpp)
add_test_target(weave_strand_stress_tests executors/strand/stress.cpp)
add_test_target(weave_strand_mo_tests executors/strand/mo.cpp)
add_test_target(weave_strand_lifetime_tests executors/strand/lifetime.cpp)

# Futures
add_test_target(weave_futures_unit_tests futures/unit.cpp)
add_test_target(weave_futures_stress_tests futures/stress.cpp)

# Coro 
add_test_target(weave_coro_unit_tests coro/unit.cpp)

# Fibers

# Sched
add_test_target(weave_fibers_sched_unit_tests fibers/sched/unit.cpp)
add_test_target(weave_fibers_sched_stress_tests fibers/sched/stress.cpp)

# Sync

# Event
add_test_target(weave_fibers_event_unit_tests fibers/sync/event/unit.cpp)
add_test_target(weave_fibers_event_stress_tests fibers/sync/event/stress.cpp)
add_test_target(weave_fibers_event_storage_tests fibers/sync/event/storage.cpp)

# Mutex
add_test_target(weave_fibers_mutex_unit_tests fibers/sync/mutex/unit.cpp)
add_test_target(fibers_mutex_stress_tests fibers/sync/mutex/stress.cpp)
add_test_target(fibers_mutex_symm_transfer_tests fibers/sync/mutex/symm_transfer.cpp)

# WaitGroup
add_test_target(weave_fibers_wg_unit_tests fibers/sync/wait_group/unit.cpp)
add_test_target(weave_fibers_wg_stress_tests fibers/sync/wait_group/stress.cpp)
add_test_target(weave_fibers_wg_storage_tests fibers/sync/wait_group/storage.cpp)

# Channel 
add_test_target(weave_fibers_chan_unit_tests fibers/sync/channel/unit.cpp)
add_test_target(weave_fibers_chan_stress_tests fibers/sync/channel/stress.cpp)

# Select
add_test_target(weave_fibers_select_unit_tests fibers/sync/select/unit.cpp)
add_test_target(weave_fibers_select_stress_tests fibers/sync/select/stress.cpp)

# Await
add_test_target(weave_fibers_await_unit_tests await/unit.cpp)

# Fibers executors

# Manual

add_test_target(weave_fibers_manual_unit_tests executors/fibers/manual/unit.cpp)

# ThreadPool

add_test_target(weave_fibers_tp_unit_tests executors/fibers/thread_pool/unit.cpp)
add_test_target(weave_fibers_tp_stress_tests executors/fibers/thread_pool/stress.cpp)

# Cancellation

add_test_target(weave_cancel_unit_tests cancel/unit.cpp)
add_test_target(weave_cancel_stress_tests cancel/stress.cpp)

# Timers 

# Standalone

add_test_target(weave_timers_standalone_unit_tests timers/standalone/unit.cpp)
add_test_target(weave_timers_standalone_stress_tests timers/standalone/stress.cpp)

#####################################################################################

# Unit tests

add_custom_target(weave_unit_tests ALL 
                  DEPENDS
                  weave_wg_unit_tests
                  weave_stack_unit_tests
                  weave_queue_unit_tests
                  weave_tp_unit_tests
                  weave_tp_wait_idle_unit_tests
                  weave_manual_unit_tests
                  weave_strand_unit_tests
                  weave_futures_unit_tests
                  weave_coro_unit_tests
                  weave_fibers_sched_unit_tests
                  weave_fibers_event_unit_tests
                  weave_fibers_mutex_unit_tests
                  weave_fibers_wg_unit_tests
                  weave_fibers_chan_unit_tests
                  weave_fibers_select_unit_tests
                  weave_fibers_await_unit_tests
                  weave_fibers_manual_unit_tests
                  weave_fibers_tp_unit_tests
                  weave_cancel_unit_tests
                  weave_timers_standalone_unit_tests
                  )

add_custom_target(weave_stress_tests ALL
                  DEPENDS
                  weave_wg_stress_tests
                  weave_wg_storage_tests
                  weave_wg_futex_tests
                  weave_stack_stress_tests
                  weave_queue_stress_tests
                  weave_tp_stress_tests
                  weave_tp_wait_idle_stress_tests
                  weave_weave_tp_balancing_stress_tests
                  weave_strand_stress_tests
                  weave_strand_mo_tests
                  weave_strand_lifetime_tests
                  weave_futures_stress_tests
                  weave_fibers_sched_stress_tests
                  weave_fibers_event_stress_tests
                  weave_fibers_event_storage_tests
                  fibers_mutex_stress_tests
                  fibers_mutex_symm_transfer_tests
                  weave_fibers_wg_stress_tests
                  weave_fibers_wg_storage_tests
                  weave_fibers_chan_stress_tests
                  weave_fibers_select_stress_tests
                  weave_fibers_tp_stress_tests
                  weave_cancel_stress_tests
                  weave_timers_standalone_stress_tests
                  )

add_custom_target(weave_tests ALL DEPENDS weave_unit_tests weave_stress_tests)





