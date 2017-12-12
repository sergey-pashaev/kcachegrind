add_executable(cgdiff main.cpp)

target_link_libraries(cgdiff core Qt5::Core)

# do not install example code...
# install(TARGETS cgdiff ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} )
