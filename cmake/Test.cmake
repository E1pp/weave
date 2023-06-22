function(add_test_target test_name file)
    add_executable(${test_name} ${file})
    target_link_libraries(${test_name} weave)
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

function(add_nontest_target tname file)
    add_executable(${tname} ${file})
    target_link_libraries(${tname} weave)
endfunction()