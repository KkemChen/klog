set_project("demo", "this is a demo")
set_xmakever("2.7.8")
set_symbols("debug")
add_cxxflags("-std=c++14")
add_cxxflags("-Wall","-DSPDLOG_COMPILED_LIB")

if is_plat("linux") then
    add_ldflags("-pthread")
elseif is_plat("windows") then
    add_ldflags("-mwindows")
end

target("demo")
    set_kind("binary")
    add_deps("spdlog")
    set_targetdir("./build")
    add_files("./*.cpp")
    add_includedirs("include")
    add_links("spdlog","stdc++fs")
    
target("spdlog")
    set_kind("shared")
    set_targetdir("./build")
    add_files("./src/*.cpp")
    add_includedirs("include")


--target("install")
    --set_kind("phony")
    --add_deps("demo","spdlog")
    --on_install(function (target)
        --os.cp("build/demo", "bin/")
        --os.cp("include/*", "bin/include/")
        --os.cp("lib/*", "bin/lib/")
    --end)

