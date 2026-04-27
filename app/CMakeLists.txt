find_package(wxWidgets REQUIRED COMPONENTS core base html)
message(STATUS "wxWidgets include dirs: ${wxWidgets_INCLUDE_DIRS}")
message(STATUS "wxWidgets libraries: ${wxWidgets_LIBRARIES}")
message(STATUS "wxWidgets definitions: ${wxWidgets_DEFINITIONS}")
include(${wxWidgets_USE_FILE})

set(FRONTEND_SOURCES main.cpp
		mcu/mcu.cpp
		peripheral/screen.cpp
		gui/about.cpp
		gui/base/about.cpp
		gui/base/debugger.cpp
		gui/debugger.cpp
		gui/brkpoint.cpp
		gui/base/brkpoint.cpp
		gui/base/brkpoint_panel.cpp
		peripheral/clockgen.cpp
		peripheral/tbc.cpp
		peripheral/standby.cpp
		peripheral/timer.cpp
		gui/base/hexview.cpp
		gui/hexed.cpp
		peripheral/keyboard.cpp)

if(WIN32)
	list(APPEND FRONTEND_SOURCES msw/app.rc)
	set(CRASHHANDLER_LIBS dbghelp)
elseif(UNIX OR APPLE)
	set(CRASHHANDLER_LIBS dl)
endif()

add_executable(emxu8_frontend ${FRONTEND_SOURCES})
target_compile_features(emxu8_frontend PUBLIC cxx_std_23)

target_link_libraries(emxu8_frontend PRIVATE
		emxu8
		SDL3
		SDL3_image
		${wxWidgets_LIBRARIES}
		${CRASHHANDLER_LIBS}
)
