add_rules("mode.debug", "mode.release")
set_languages("cxx14")
add_cxxflags("-Wall")
add_defines("SPDLOG_COMPILED_LIB")


target("test")
    set_kind("binary")
    add_includedirs("include")
    add_files("main.cpp","Log.cpp","src/*.cpp")
