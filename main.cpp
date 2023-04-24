#include "myspdlog.hpp"


using namespace std;

int main(int argc, char *argv[])
{
	using namespace klog;
	if (!logger::get().init("logs/test/test.log")) {
		return 1;
	}

		logger::get().set_level(spdlog::level::trace);

		LOGTRACE() << "test" << 6666;
		logtrace("TEST");
		LOG_TRACE("test {}", 1);

		LOGDEBUG() << "test" << 6666;
		logdebug("TEST");
		LOG_DEBUG("test {}", 1);

		LOGINFO() << "test" << 6666;
		loginfo("TEST");
		LOG_INFO("test {}", 1);

		LOGWARN() << "test" << 6666;
		logwarn("TEST");
		LOG_WARN("test {}", 1);

		LOGERROR() << "test" << 6666;
		logerror("TEST");
		LOG_ERROR("test {}", 1);

		LOGFATAL() << "test" << 6666;
		logfatal("TEST");
		LOG_FATAL("test {}", 1);

	// call before spdlog static variables destroy
	logger::get().shutdown();

	getchar();
	return 0;
}
