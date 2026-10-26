execute_process(COMMAND "${TOOL}" --run "${SOURCE}" -O0
  RESULT_VARIABLE status0 OUTPUT_VARIABLE out0 ERROR_VARIABLE err0 TIMEOUT 10)
execute_process(COMMAND "${TOOL}" --run "${SOURCE}" -O1
  RESULT_VARIABLE status1 OUTPUT_VARIABLE out1 ERROR_VARIABLE err1 TIMEOUT 10)
if(NOT "${status0}" STREQUAL "${EXPECT_STATUS}" OR
   NOT "${status1}" STREQUAL "${status0}" OR
   NOT "${out1}" STREQUAL "${out0}" OR NOT "${err1}" STREQUAL "${err0}")
  message(FATAL_ERROR "O0/O1 mismatch: ${status0}/${status1}\n${out0}/${out1}\n${err0}/${err1}")
endif()
