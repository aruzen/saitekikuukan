function(saitekikuukan_add_sample name library)
    add_executable(sample-${name} ${name}.cpp)
    target_link_libraries(sample-${name} PRIVATE ${library})
    add_test(NAME sample-${name} COMMAND sample-${name})
    set_tests_properties(sample-${name} PROPERTIES LABELS sample)
endfunction()

if(SAITEKIKUUKAN_BUILD_TESTS)
    foreach(name IN ITEMS
        particles
        fixed-capacity
        buffer-ids
        insertion-events
        chunked-particles
        custom-columns
        move-only
        strategy-selection)
        saitekikuukan_add_sample(${name} saitekikuukan::saitekikuukan)
    endforeach()
endif()

if(SAITEKIKUUKAN_BUILD_REFLECTION_TESTS)
    foreach(name IN ITEMS
        reflected-particles
        reflected-chunks
        reflected-buffer
        raw-columns)
        saitekikuukan_add_sample(${name} saitekikuukan::reflection)
    endforeach()
endif()
