cmake_minimum_required(VERSION 3.22)

project(SamplerFoodBench VERSION 0.1.0 LANGUAGES C CXX)

option(SAMPLE_BENCH_BUILD_APP "Build the JUCE desktop prototype" ON)
option(SAMPLE_BENCH_FETCH_JUCE "Fetch JUCE with CMake when it is not installed" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(sample_bench_model STATIC
    src/model/SampleBenchModel.cpp
    src/model/SampleBenchModel.h
)
target_include_directories(sample_bench_model PUBLIC src)
target_compile_features(sample_bench_model PUBLIC cxx_std_20)

enable_testing()

add_executable(sample_bench_model_tests
    tests/SampleBenchModelTests.cpp
)
target_link_libraries(sample_bench_model_tests PRIVATE sample_bench_model)
add_test(NAME sample_bench_model_tests COMMAND sample_bench_model_tests)

add_executable(bench_palette_tests
    tests/BenchPaletteTests.cpp
)
target_include_directories(bench_palette_tests PRIVATE src)
target_compile_features(bench_palette_tests PRIVATE cxx_std_20)
add_test(NAME bench_palette_tests COMMAND bench_palette_tests)

add_executable(bench_render_model_tests
    tests/BenchRenderModelTests.cpp
)
target_link_libraries(bench_render_model_tests PRIVATE sample_bench_model)
add_test(NAME bench_render_model_tests COMMAND bench_render_model_tests)

if(SAMPLE_BENCH_BUILD_APP)
    find_package(JUCE CONFIG QUIET)

    if(NOT JUCE_FOUND AND SAMPLE_BENCH_FETCH_JUCE)
        include(FetchContent)
        FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.6
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(JUCE)
    endif()

    if(NOT TARGET juce::juce_gui_extra)
        message(FATAL_ERROR "JUCE was not found. Install JUCE or configure with -DSAMPLE_BENCH_BUILD_APP=OFF.")
    endif()

    juce_add_gui_app(SamplerFoodBench
        PRODUCT_NAME "Bench Sampler"
        COMPANY_NAME "Bench Sampler"
        BUNDLE_ID "com.samplerfoodbench.prototype"
    )

    target_sources(SamplerFoodBench PRIVATE
        src/app/Main.cpp
        src/app/MainComponent.cpp
        src/app/MainComponent.h
        src/app/PluginScanWindow.cpp
        src/app/PluginScanWindow.h
        src/app/WaveformOverview.cpp
        src/app/WaveformOverview.h
    )

    target_link_libraries(SamplerFoodBench PRIVATE
        sample_bench_model
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_gui_extra
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(SamplerFoodBench PRIVATE -Wno-nontrivial-memcall -Wno-deprecated-declarations)
    endif()

    target_compile_definitions(SamplerFoodBench PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_PLUGINHOST_VST3=1
    )
endif()
