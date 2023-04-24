# klog
基于spdlog封装，支持fmt\printf\流式输入



### xmake构建

* `xmake -v `

### 使用说明

* 直接包含`klog.hpp`

* 示例：

  ```cpp
  #include "klog.hpp"
  
  int main(int argc, char *argv[])
  {
  	using namespace klog;
  	if (!logger::get().init("logs/test/test.log")) {
  		return 1;
  	}
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
   ![image](https://user-images.githubusercontent.com/44298896/233994979-f6499d74-5eac-49ec-90b3-490c9a278ce4.png)
  
  ---


  

