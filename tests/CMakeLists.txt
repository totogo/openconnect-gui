find_package(Qt6 REQUIRED COMPONENTS Test Widgets)

if(APPLE)
    enable_language(OBJCXX)

    # Pure-logic test for the stale SystemConfiguration clean-up (no root needed).
    add_executable(tst_stalevpnstate
        tst_stalevpnstate.cpp
        ${CMAKE_SOURCE_DIR}/src/macos/StaleVpnState.cpp
        ${CMAKE_SOURCE_DIR}/src/logger.cpp
    )
    target_include_directories(tst_stalevpnstate PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(tst_stalevpnstate PRIVATE Qt6::Test Qt6::Core spdlog::spdlog)
    add_test(NAME stalevpnstate COMMAND tst_stalevpnstate)

    # Regression test for the macOS 27 tray-icon crash. Drives Qt's real cocoa
    # platform plugin, so it needs a window server session (not headless CI).
    add_executable(tst_trayiconworkaround
        tst_trayiconworkaround.mm
        ${CMAKE_SOURCE_DIR}/src/macos/TrayIconWorkaround.mm
        ${CMAKE_SOURCE_DIR}/src/logger.cpp
    )
    target_include_directories(tst_trayiconworkaround PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(tst_trayiconworkaround PRIVATE Qt6::Widgets spdlog::spdlog "-framework AppKit")
    add_test(NAME trayiconworkaround COMMAND tst_trayiconworkaround)
    set_tests_properties(trayiconworkaround PROPERTIES LABELS gui)
endif()
