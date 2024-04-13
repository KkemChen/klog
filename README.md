# klog
基于[spdlog ](https://github.com/gabime/spdlog)封装，支持fmt\printf\流式输入

> 代码使用xmake进行编译（仅作为示例），如需集成到其他项目，只需自行链接spdlog库即可。


### xmake构建

* `xmake -v `

### 使用说明

* 包含`Log.h`，编译时链接到该库

* 示例：

  ```cpp
  #include "Log.h"
  
  int main()
  {
		int ret = 22;
		//主日志初始化
		kkem::Logger::Get().init("logs/stream.log",kkem::STDOUT | kkem::FILEOUT | kkem::ASYNC);
		kkem::Logger::Get().set_level(kkem::LOG_LEVEL::TRACE);
		
		//添加额外日志
		kkem::Logger::Get().add_ExLog("logs/EX_stream.log", kkem::STDOUT | kkem::FILEOUT | kkem::ASYNC);
		kkem::Logger::Get().set_level("EX_stream", kkem::LOG_LEVEL::OFF);
		
		LOGTRACE() << "test" << 6666;	///流式输入
		logtrace("TEST:%p,%d,%x",&ret,ret+2,&ret+1);	///printf式输入
		log_trace("fmt test %p", (void*)&ret);	///fmt输入(类printf式，指针类型需转为void*)
		LOG_TRACE("test {}", 1);	///fmt式输入
		
			  // ...
			  // code...
			  // ...

		/**********指定日志输出************/
		LOGTRACE_("EX_stream") << "test" << 6666;
		logtrace_("EX_stream","TEST:%p,%d,%x", &ret, ret + 2, &ret + 1);
		log_trace_("EX_stream","fmt test %p", (void*)&ret);
		LOG_TRACE_("EX_stream","test {}", 1);
			  
			  // ...
			  // code...
			  // ...

		getchar();
		return 0;
  }
  ```

 
 * 输出：  
   ![Snipaste_2023-04-25_22-31-10](https://user-images.githubusercontent.com/44298896/234310422-7dd1e523-22a9-47fe-adbb-3455ef3dd7b5.png)

  
  ---


  

