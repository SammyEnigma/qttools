# Generated from src.pro.

# special case begin
# Need to stop building these apps by default because they would fail
# in device_and_simulator builds.
if(IOS)
   set(_qt_additional_tools_to_exclude qtplugininfo)
endif()
# special case end

qt_exclude_tool_directories_from_default_target(
    distancefieldgenerator
    pixeltool
    ${_qt_additional_tools_to_exclude} # special case
)

# special case begin
# Evaluate features to decide what to build.
# The config files will be written in the src/global module.
qt_feature_evaluate_features("${CMAKE_CURRENT_SOURCE_DIR}/../configure.cmake")
# special case end

add_subdirectory(global) # special case add as first directory
add_subdirectory(linguist)
# add_subdirectory(global) # special case remove
if(QT_FEATURE_png AND QT_FEATURE_pushbutton AND QT_FEATURE_toolbutton AND TARGET Qt::Widgets)
    add_subdirectory(designer)
    add_subdirectory(pixeltool)
endif()
if(QT_FEATURE_png AND QT_FEATURE_pushbutton AND QT_FEATURE_toolbutton AND TARGET Qt::Widgets AND (FEATURE_sql_sqlite OR QT_BUILD_SHARED_LIBS)) # special case
    add_subdirectory(assistant)
endif()
if(QT_FEATURE_png AND QT_FEATURE_thread AND QT_FEATURE_toolbutton AND TARGET Qt::Quick AND TARGET Qt::Widgets)
    add_subdirectory(distancefieldgenerator)
endif()
if(QT_FEATURE_commandlineparser)
    add_subdirectory(qtattributionsscanner)
endif()
if(QT_FEATURE_commandlineparser AND QT_FEATURE_library AND (android_app OR NOT ANDROID))
    add_subdirectory(qtplugininfo)
endif()
if(QT_FEATURE_clang AND QT_FEATURE_commandlineparser AND QT_FEATURE_thread)
    add_subdirectory(qdoc)
endif()
if(MACOS)
    add_subdirectory(macdeployqt)
endif()
if(TARGET Qt::DBus)
    add_subdirectory(qdbus)
endif()
if(QT_FEATURE_windeployqt) # special case
    add_subdirectory(windeployqt)
endif()
if(QT_FEATURE_commandlineparser AND TARGET Qt::Gui AND NOT ANDROID AND NOT QNX AND NOT UIKIT AND NOT WASM)
    add_subdirectory(qtdiag)
endif()
