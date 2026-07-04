execute_process(
    COMMAND "${OPTIONS_PRICER}" price
        --kind european --type call
        --spot 100 --strike 100 --rate 0.05 --vol 0.2 --maturity 1
        --paths 1000 --seed 42
    RESULT_VARIABLE price_result
    OUTPUT_VARIABLE price_output
    ERROR_VARIABLE price_error
)
if(NOT price_result EQUAL 0)
    message(FATAL_ERROR "price command failed: ${price_error}")
endif()
foreach(field
    "black_scholes:"
    "monte_carlo:"
    "standard_error:"
    "confidence_interval_95:"
    "paths:"
)
    string(FIND "${price_output}" "${field}" field_position)
    if(field_position EQUAL -1)
        message(FATAL_ERROR "price output is missing ${field}")
    endif()
endforeach()

execute_process(
    COMMAND "${OPTIONS_PRICER}" bench --paths 1000 --threads 1 --seed 42
    RESULT_VARIABLE benchmark_result
    OUTPUT_VARIABLE benchmark_output
    ERROR_VARIABLE benchmark_error
)
if(NOT benchmark_result EQUAL 0)
    message(FATAL_ERROR "benchmark command failed: ${benchmark_error}")
endif()
foreach(field "elapsed_ms" "paths_per_sec" "plain" "antithetic")
    string(FIND "${benchmark_output}" "${field}" field_position)
    if(field_position EQUAL -1)
        message(FATAL_ERROR "benchmark output is missing ${field}")
    endif()
endforeach()

execute_process(
    COMMAND "${OPTIONS_PRICER}" bench --paths 999 --threads 1
    RESULT_VARIABLE invalid_result
    OUTPUT_QUIET
    ERROR_QUIET
)
if(invalid_result EQUAL 0)
    message(FATAL_ERROR "benchmark accepted an odd antithetic path count")
endif()
