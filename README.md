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
		int ret = 1;
		
		LOGTRACE() << "test" << 6666;  ///流式输入
		logtrace("TEST:%p,%d,%x",&ret,ret+2,&ret+1);  ///print输入
		LOG_TRACE("test {}", 1);  ///fmt输入
		
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


  

