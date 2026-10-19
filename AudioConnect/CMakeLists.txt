cmake_minimum_required(VERSION 3.22)

# Static MSVC runtime so the .exe / .vst3 run on machines without the VC++ redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(AudioConnect VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

# ---- Optional SDKs (you must download these yourself; licences apply) --------
#  -DASIO_SDK_PATH=C:/SDKs/asiosdk     -> enables ASIO drivers (Steinberg licence)
#  -DAAX_SDK_PATH=C:/SDKs/aax-sdk      -> adds an AAX build for Pro Tools (Avid dev account + PACE signing)
set(AC_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND AC_FORMATS AU)          # Logic Pro uses Audio Units (macOS only)
endif()
if(DEFINED AAX_SDK_PATH)
    juce_set_aax_sdk_path(${AAX_SDK_PATH})
    list(APPEND AC_FORMATS AAX)
endif()

juce_add_plugin(AudioConnect
    COMPANY_NAME               "MiKeys Dsz Studios"
    PRODUCT_NAME               "Audio Connect"
    PLUGIN_MANUFACTURER_CODE   Mkds
    PLUGIN_CODE                Acon
    FORMATS                    ${AC_FORMATS}
    IS_SYNTH                   FALSE
    NEEDS_MIDI_INPUT           FALSE
    NEEDS_MIDI_OUTPUT          FALSE
    VST3_CATEGORIES            Fx Tools
    AU_MAIN_TYPE               kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD    FALSE)

target_sources(AudioConnect PRIVATE
    Source/PluginProcessor.cpp
    Source/RouterEngine.cpp)

# ---- Poppins font: drop Poppins-Regular.ttf and Poppins-SemiBold.ttf into Assets/ ----
file(GLOB AC_POPPINS "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Poppins-*.ttf")
if(AC_POPPINS)
    juce_add_binary_data(AC_Fonts SOURCES ${AC_POPPINS})
    set_target_properties(AC_Fonts PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
    target_link_libraries(AudioConnect PRIVATE AC_Fonts)
    target_compile_definitions(AudioConnect PRIVATE AC_HAS_POPPINS=1)
else()
    message(WARNING "No Poppins TTFs found in Assets/ - UI will fall back to an installed 'Poppins' or the system font.")
endif()

target_compile_definitions(AudioConnect PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_WASAPI=1
    JUCE_DIRECTSOUND=1)

if(DEFINED ASIO_SDK_PATH)
    target_compile_definitions(AudioConnect PUBLIC JUCE_ASIO=1)
    target_include_directories(AudioConnect PRIVATE "${ASIO_SDK_PATH}/common")
endif()

target_link_libraries(AudioConnect
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_devices
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
