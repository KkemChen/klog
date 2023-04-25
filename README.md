# klog
基于spdlog封装，支持fmt\printf\流式输入



### xmake构建

* `xmake -v `

### 使用说明

* 直接包含`klog.hpp`

* 示例：

  ```cpp
  #include "klog.hpp"
  
  int main()
  {
		using namespace klog;
		logger::get().init("logs/test/test.log")

		logger::get().set_level(spdlog::level::trace);
		int ret = 0;

		LOGINFO() << "test" << 6666;  ///流式输入
		loginfo("TEST%d",ret);   ///print输入
		LOG_INFO("test {}", 1);  ///fmt输入

			  // ...
			  // code...
			  // ...

		logger::get().shutdown();

		getchar();
		return 0;
  }
  ```

 
 * 输出：  
   ![Snipaste_2023-04-25_22-31-10](https://user-images.githubusercontent.com/44298896/234310422-7dd1e523-22a9-47fe-adbb-3455ef3dd7b5.png)
  
  ---


  

