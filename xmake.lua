set_project("demo", "this is a demo")
set_xmakever("2.7.8")
set_symbols("debug")
add_cxxflags("-std=c++14")
add_cxxflags("-Wall")


target("demo")
    set_kind("binary")
    set_targetdir("./build")
    add_files("./*.cpp")
    add_linkdirs("lib")
    add_links("pthread", "stdc++fs")
    

target("install")
    set_kind("phony")
    add_deps("demo")
    on_install(function (target)
        os.cp("build/demo", "bin/")
        os.cp("include/*", "bin/include/")
        os.cp("lib/*", "bin/lib/")
    end)

