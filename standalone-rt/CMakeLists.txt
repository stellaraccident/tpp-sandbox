add_mlir_library(standalone_c_runner_utils
  SHARED
  XsmmRunnerUtils.cpp

  LINK_LIBS PUBLIC
  xsmm

  #TODO: pthread must be included here for proper linking on "some" systems
  # figure this out ..
  pthread
)
set_property(TARGET standalone_c_runner_utils PROPERTY CXX_STANDARD 11)
target_compile_definitions(standalone_c_runner_utils PRIVATE mlir_c_runner_utils_EXPORTS)
