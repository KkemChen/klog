add_rules("mode.debug", "mode.release")
set_languages("cxx14")

target("test")
    set_kind("binary")
    add_includedirs("include")
    add_files("src/*.cpp")