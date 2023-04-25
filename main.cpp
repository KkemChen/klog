#include "klog.hpp"


using namespace std;

int main(int argc, char *argv[])
{
	int ret = 1;
	LOGTRACE() << "test" << 6666;
	logtrace("TEST:%p,%d,%x",&ret,ret+2,&ret+1);
	LOG_TRACE("test {}", 1);

	LOGDEBUG() << "test" << 6666;
	logdebug("TEST:%p,%d,%x", &ret, ret + 2, &ret + 1);
	LOG_DEBUG("test {}", 1);

	LOGINFO() << "test" << 6666;
	loginfo("TEST:%p,%d,%x", &ret, ret + 2, &ret + 1);
	LOG_INFO("test {}", 1);

	LOGWARN() << "test" << 6666;
	logwarn("TEST:%p,%d,%x", &ret, ret + 2, &ret + 1);
	LOG_WARN("test {}", 1);

	LOGERROR() << "test" << 6666;
	logerror("TEST:%p,%d,%x", &ret, ret + 2, &ret + 1);
	LOG_ERROR("test {}", 1);

	LOGFATAL() << "test" << 6666;
	logfatal("TEST:%p,%d,%x", &ret, ret + 2, &ret + 1);
	LOG_FATAL("test {}", 1);

	getchar();
	return 0;
}
