set_project("demo", "this is a demo")       -- 设置项目名称和描述
set_xmakever("2.7.8")                       -- 设置xmake版本
add_rules("mode.debug", "mode.release")     -- xmake f --verbose -m debug/release

-- 添加C++编译选项
--set_symbols("debug")                      -- 默认构建模式 debug release
set_optimize("none")                      -- 优化级别  
set_languages("cxx14")                    -- 语言标准 
if is_plat("windows") then
    add_cxxflags("/Zc:__cplusplus","/utf-8")
end

if is_plat("macosx","linux") then
    add_cxxflags("-ggdb","-g","-fPIC","-Wall","-Wextra",
                "-Wno-unused-function","-Wno-unused-parameter","-Wno-unused-variable",
                "-Wno-error=extra","-Wno-error=missing-field-initializers","-Wno-error=type-limits")
    add_syslinks("pthread","stdc++fs")
end

add_includedirs("3rdpart/Spdlog/include")

set_targetdir("build")

target("demo")
    set_kind("binary")
    add_deps("spdlog")
    add_files("src/*.cpp")
    --add_linkdirs("lib")
    add_links("spdlog")


target("spdlog")
    set_kind("static")
    set_targetdir("build")
    add_files("3rdpart/Spdlog/src/*.cpp")
    add_defines("SPDLOG_COMPILED_LIB")
    


-----------Intall------------    
-- 设置安装目录
set_installdir("bin")

target("install")
    set_kind("phony")
    add_deps("demo")
    on_install(function (install)
        os.cp("build/demo", "bin/")
        os.cp("src/**.h", "bin/include/")
        os.cp("build/*.so", "bin/lib/")
    end)


