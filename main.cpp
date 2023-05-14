#include "Log.hpp"

using namespace std;


int main(int argc, char *argv[])
{
	int ret = 22;
	kkem::Logger::Get().init("logs/stream.log",kkem::STDOUT | kkem::FILEOUT | kkem::ASYNC);
	kkem::Logger::Get().set_level(kkem::LOG_LEVEL::TRACE);


	kkem::Logger::Get().add_ExLog("logs/EX_stream.log", kkem::STDOUT | kkem::FILEOUT | kkem::ASYNC);
	kkem::Logger::Get().set_level_("EX_stream", kkem::LOG_LEVEL::OFF);


	LOGTRACE() << "test" << 6666;
	logtrace("TEST:%p,%d,%x",&ret,ret+2,&ret+1);
	log_trace("fmt test %p", (void*)&ret);
	LOG_TRACE("test {}", 1);

	//......

	LOGFATAL() << "test" << 6666;
	logfatal("TEST:%p,%d,%x", &ret, ret + 2, &ret + 1);
	LOG_FATAL("test {}", 1);

	/**********************/
	LOGTRACE_("EX_stream") << "test" << 6666;
	logtrace_("EX_stream","TEST:%p,%d,%x", &ret, ret + 2, &ret + 1);
	log_trace_("EX_stream","fmt test %p", (void*)&ret);
	LOG_TRACE_("EX_stream","test {}", 1);

	//......

	LOGFATAL_("EX_stream") << "test" << 6666;
	logfatal_("EX_stream","TEST:%p,%d,%x", &ret, ret + 2, &ret + 1);
	LOG_FATAL_("EX_stream","test {}", 1);


	getchar();
	return 0;
}
