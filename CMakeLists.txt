cmake_minimum_required(VERSION 3.16)
project(LLM-O-Meter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/src/ui)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Svg)

set(SOURCES
	src/stdafx.cpp
	src/main.cpp
	src/CredentialManager.cpp
	src/UsageApiClient.cpp
	src/AntiGravityProcessScanner.cpp
	src/AntiGravityClient.cpp
	src/TrayManager.cpp
	src/UsagePopupWidget.cpp
	src/SettingsDialog.cpp
	src/UsageRateTracker.cpp
)

set(HEADERS
	src/stdafx.h
	src/UsageData.h
	src/AntiGravityData.h
	src/FormatUtils.h
	src/CredentialManager.h
	src/UsageApiClient.h
	src/AntiGravityProcessScanner.h
	src/AntiGravityClient.h
	src/TrayManager.h
	src/UsagePopupWidget.h
	src/SettingsDialog.h
	src/UsageRateTracker.h
)

set(UI_FILES
	src/ui/UsagePopupWidget.ui
	src/ui/SettingsDialog.ui
)

set(RESOURCES
	src/resources/resources.qrc
)
if(WIN32)
	list(APPEND RESOURCES src/resources/app.rc)
endif()

if(WIN32)
	add_executable(${PROJECT_NAME} WIN32
		${SOURCES} ${HEADERS} ${UI_FILES} ${RESOURCES})
elseif(APPLE)
	add_executable(${PROJECT_NAME} MACOSX_BUNDLE
		${SOURCES} ${HEADERS} ${UI_FILES} ${RESOURCES})
	set_target_properties(${PROJECT_NAME} PROPERTIES
		MACOSX_BUNDLE_GUI_IDENTIFIER com.llmometer.app
		MACOSX_BUNDLE_BUNDLE_NAME "LLM-O-Meter"
	)
else()
	add_executable(${PROJECT_NAME}
		${SOURCES} ${HEADERS} ${UI_FILES} ${RESOURCES})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Svg)
if(WIN32)
	target_link_libraries(${PROJECT_NAME} PRIVATE wbemuuid iphlpapi ws2_32)
elseif(APPLE)
	target_link_libraries(${PROJECT_NAME} PRIVATE "-framework Security")
endif()
target_precompile_headers(${PROJECT_NAME} PRIVATE src/stdafx.h)
