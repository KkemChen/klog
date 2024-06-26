#pragma once

#if defined(_WIN32) && _MSC_VER >= 1910 && __cplusplus < 201703L
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#endif

#if __cplusplus < 201703L || (defined(__GNUC__) && __GNUC__ < 8)
#include <experimental/filesystem>
namespace std {
namespace filesystem = experimental::filesystem;
}// namespace std
#else
#include <filesystem>
#endif

#include <cstdarg>
#include <fstream>
#include <sstream>
#include <memory>
#include <regex>
#include <iostream>

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/logger.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/bundled/printf.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/common.h>
#include <spdlog/pattern_formatter.h>

namespace kkem {
constexpr const char* LOG_PATH = "logs/demo.log";		   //默认日志存储路径
constexpr uint32_t SINGLE_FILE_MAX_SIZE = 20 * 1024 * 1024;//单个日志文件最大大小(20M)
constexpr uint32_t MAX_STORAGE_DAYS = 5;				   //日志保存时间(天)

enum LogMode {
	STDOUT = 1 << 0, //主日志控制台输出
	FILEOUT = 1 << 1,//主日志文件输出
	ASYNC = 1 << 2	 //异步日志模式
};

enum LogLevel {
	Trace = 0,
	Debug = 1,
	Info = 2,
	Warn = 3,
	Error = 4,
	Fatal = 5,
	Off = 6,
	N_Level
};

class CustomLevelFormatterFlag;
class CustomRotatingFileSink;

class Logger final {
public:
	/// let logger like stream
	struct LogStream : public std::ostringstream {
	public:
		LogStream(const spdlog::source_loc& loc, LogLevel lvl) : _loc(loc), _lvl(lvl) {}

		~LogStream() override { flush(); }

		void flush() { Logger::Get().log(_loc, _lvl, str().c_str()); }

	private:
		spdlog::source_loc _loc;
		LogLevel _lvl;
	};

	struct LogStream_ : public std::ostringstream {
	public:
		LogStream_(const std::string& logger, const spdlog::source_loc& loc,
				   LogLevel lvl) : _logger(logger), _loc(loc), _lvl(lvl) {}

		~LogStream_() override { flush_(); }

		void flush_() {
			Logger::Get().log_(_logger, _loc, _lvl, str().c_str());
		}

	private:
		std::string _logger;
		spdlog::source_loc _loc;
		LogLevel _lvl;
	};

public:
	static Logger& Get() {
		static Logger Logger;
		return Logger;
	}

	///停止所有日志记录操作并清理内部资源
	void shutdown() { spdlog::shutdown(); }

	///spdlog输出
	template<typename... Args>
	void log(const spdlog::source_loc& loc, LogLevel lvl, const char* fmt, const Args&... args);

	///传统printf输出
	void printf(const spdlog::source_loc& loc, LogLevel lvl, const char* fmt, ...);

	///fmt的printf输出（不支持格式化非void类型指针）
	template<typename... Args>
	void fmt_printf(const spdlog::source_loc& loc, LogLevel lvl, const char* fmt,
					const Args&... args);

	/*********Exlog**********/
	///spdlog输出
	template<typename... Args>
	void log_(const std::string& logger, const spdlog::source_loc& loc, LogLevel lvl,
			  const char* fmt, const Args&... args);

	///传统printf输出
	void printf_(const std::string& logger, const spdlog::source_loc& loc, LogLevel lvl,
				 const char* fmt, ...);

	///fmt的printf输出（不支持格式化非void类型指针）
	template<typename... Args>
	void fmt_printf_(const std::string& logger, const spdlog::source_loc& loc, LogLevel lvl,
					 const char* fmt, const Args&... args);

	///设置输出级别
	void set_level(LogLevel lvl);

	void set_level(const std::string& logger, LogLevel lvl);

	///设置自定义输出
	void add_on_output(const std::function<void(const std::string& msg, const int& level)>& cb);

	void add_on_output(const std::string& logger, const std::function<void(const std::string& msg, const int& level)>& cb);

	///设置刷新达到指定级别时自动刷新缓冲区
	void set_flush_on(LogLevel lvl) {
		spdlog::flush_on(static_cast<spdlog::level::level_enum>(lvl));
	}

	/**
    * \brief 初始化日志
    * \param logPath 日志路径：默认"logs/LinuxProject1.log"
    * \param mode STDOUT:控制台 FILE:文件 ASYNC:异步模式
    * \param threadCount 异步模式线程池线程数量
    * \param logBufferSize 异步模式下日志缓冲区大小
    * \return true:success  false:failed
    */
	bool init(const std::string& logPath = LOG_PATH, const uint32_t mode = STDOUT,
			  const uint32_t threadCount = 1, const uint32_t backtrackDepth = 128, const uint32_t logBufferSize = 32 * 1024);

	/**
    * \brief 添加额外日志（多用于临时调试）
    * \param logPath 日志路径：需要避免 额外日志名称 与 主日志名称 重复
    * \param mode STDOUT:控制台 FILE:文件 ASYNC:异步模式
    * \return true:success  false:failed
    */
	bool add_ExLog(const std::string& logPath, const int mode = STDOUT);

private:
	Logger() = default;
	~Logger() { shutdown(); }
	Logger(const Logger&) = delete;
	void operator=(const Logger&) = delete;

private:
	std::atomic_bool _isInited = {false};
	spdlog::level::level_enum _logLevel = spdlog::level::trace;
	static thread_local std::stringstream _ss;

	std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> _map_exLog;
};

/**
* \brief 自定义sink
*	1.按文件大小分片存储
*	2.文件命名：basename+datetime+.log
*	3.每次rotate时自动检查当前目录日志文件是否超过设定的时间，并自动清理
*/
class CustomRotatingFileSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
	/**
    * \brief
    * \param log_path 日志存储路径
    * \param max_size 单文件最大容量
    * \param max_storage_days 最大保存天数
    * \param rotate_on_open 默认true
    * \param event_handlers 默认
    */
	CustomRotatingFileSink(spdlog::filename_t log_path, std::size_t max_size,
						   std::size_t max_storage_days, bool rotate_on_open = true,
						   const spdlog::file_event_handlers& event_handlers = {});

	///添加自定义输出回调 (默认不开启互斥锁的情况，需要在实际输出日志前调用，否则可能不安全)
	void add_on_output(const std::function<void(const std::string& msg, const int& level)>& cb);

protected:
	///将日志消息写入到输出目标
	void sink_it_(const spdlog::details::log_msg& msg) override;

	///刷新日志缓冲区
	void flush_() override { _file_helper.flush(); }

private:
	spdlog::filename_t calc_filename();
	spdlog::filename_t filename();

	///日志轮转
	void rotate_();

	///清理过期日志文件
	void cleanup_file_();

	///是否到达每日轮转时间点
	bool is_daily_rotate_tp_();

	std::atomic<uint32_t> _max_size;
	std::atomic<uint32_t> _max_storage_days;
	std::size_t _current_size;
	std::atomic<int> _last_rotate_day;
	spdlog::details::file_helper _file_helper;
	std::filesystem::path _log_basename;
	std::filesystem::path _log_filename;
	std::filesystem::path _log_parent_path;
	std::filesystem::path _log_path;

	std::recursive_mutex _on_output_mx;
	std::vector<std::function<void(const std::string&, const int& level)>> _on_outputs;//自定义输出回调
};

class CustomLevelFormatterFlag : public spdlog::custom_flag_formatter {
public:
	void format(const spdlog::details::log_msg& _log_msg, const std::tm&,
				spdlog::memory_buf_t& dest) override;

	std::unique_ptr<custom_flag_formatter> clone() const override {
		return spdlog::details::make_unique<CustomLevelFormatterFlag>();
	}
};

///////////////////////////////////////////
template<typename... Args>
inline void Logger::log(const spdlog::source_loc& loc, LogLevel lvl, const char* fmt,
						const Args&... args) {
#if __cplusplus >= 202002L
	spdlog::log(loc, static_cast<spdlog::level::level_enum>(lvl), fmt::runtime(fmt), args...);
#else
	spdlog::log(loc, static_cast<spdlog::level::level_enum>(lvl), fmt, args...);
#endif
}

template<typename... Args>
inline void Logger::fmt_printf(const spdlog::source_loc& loc, LogLevel lvl, const char* fmt,
							   const Args&... args) {
	log(loc, lvl, fmt::sprintf(fmt, args...).c_str());
}

template<typename... Args>
inline void Logger::log_(const std::string& logger, const spdlog::source_loc& loc, LogLevel lvl,
						 const char* fmt, const Args&... args) {
	auto it = _map_exLog.find(logger);
	if (it != _map_exLog.end()) {
#if __cplusplus >= 202002L
		it->second->log(loc, static_cast<spdlog::level::level_enum>(lvl), fmt::runtime(fmt),
						args...);
#else
		it->second->log(loc, static_cast<spdlog::level::level_enum>(lvl), fmt, args...);
#endif
	} else {
		log(loc, lvl, fmt, args...);
	}
}

template<typename... Args>
inline void Logger::fmt_printf_(const std::string& logger, const spdlog::source_loc& loc,
								LogLevel lvl, const char* fmt, const Args&... args) {
	auto it = _map_exLog.find(logger);
	if (it != _map_exLog.end()) {
		it->second->log(loc, static_cast<spdlog::level::level_enum>(lvl),
						fmt::sprintf(fmt, args...).c_str());
	} else {
		log(loc, lvl, fmt::sprintf(fmt, args...).c_str());
	}
}
}// namespace kkem

#define log_trace(fmt, ...) kkem::Logger::Get().fmt_printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Trace, fmt, ##__VA_ARGS__)
#define LOG_TRACE(fmt, ...) kkem::Logger::Get().log({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Trace, fmt, ##__VA_ARGS__)
#define logtrace(fmt, ...) kkem::Logger::Get().printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Trace, fmt, ##__VA_ARGS__)
#define LOGTRACE() kkem::Logger::LogStream({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Trace)

#define log_trace_(logger, fmt, ...) kkem::Logger::Get().fmt_printf_(logger, {__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Trace, fmt, ##__VA_ARGS__)
#define LOG_TRACE_(logger, fmt, ...) kkem::Logger::Get().log_(logger, {__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Trace, fmt, ##__VA_ARGS__)
#define logtrace_(logger, fmt, ...) kkem::Logger::Get().printf_(logger, {__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Trace, fmt, ##__VA_ARGS__)
#define LOGTRACE_(logger) kkem::Logger::LogStream_(logger, {__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Trace)

#define log_debug(fmt, ...) kkem::Logger::Get().fmt_printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Debug, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) kkem::Logger::Get().log({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Debug, fmt, ##__VA_ARGS__)
#define logdebug(fmt, ...) kkem::Logger::Get().printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Debug, fmt, ##__VA_ARGS__)
#define LOGDEBUG() kkem::Logger::LogStream({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Debug)

#define log_debug_(logger, fmt, ...) kkem::Logger::Get().fmt_printf_(logger, {__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Debug, fmt, ##__VA_ARGS__)
#define LOG_DEBUG_(logger, fmt, ...) kkem::Logger::Get().log_(logger, {__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Debug, fmt, ##__VA_ARGS__)
#define logdebug_(logger, fmt, ...) kkem::Logger::Get().printf_(logger, {__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Debug, fmt, ##__VA_ARGS__)
#define LOGDEBUG_(logger) kkem::Logger::LogStream_(logger, {__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Debug)

#define log_info(fmt, ...) kkem::Logger::Get().fmt_printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Info, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) kkem::Logger::Get().log({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Info, fmt, ##__VA_ARGS__)
#define loginfo(fmt, ...) kkem::Logger::Get().printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Info, fmt, ##__VA_ARGS__)
#define LOGINFO() kkem::Logger::LogStream({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Info)

#define log_info_(logger, fmt, ...) kkem::Logger::Get().fmt_printf_(logger, {__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Info, fmt, ##__VA_ARGS__)
#define LOG_INFO_(logger, fmt, ...) kkem::Logger::Get().log_(logger, {__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Info, fmt, ##__VA_ARGS__)
#define loginfo_(logger, fmt, ...) kkem::Logger::Get().printf_(logger, {__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Info, fmt, ##__VA_ARGS__)
#define LOGINFO_(logger) kkem::Logger::LogStream_(logger, {__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Info)

#define log_warn(fmt, ...) kkem::Logger::Get().fmt_printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Warn, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) kkem::Logger::Get().log({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Warn, fmt, ##__VA_ARGS__)
#define logwarn(fmt, ...) kkem::Logger::Get().printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Warn, fmt, ##__VA_ARGS__)
#define LOGWARN() kkem::Logger::LogStream({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Warn)

#define log_warn_(logger, fmt, ...) kkem::Logger::Get().fmt_printf_(logger, {__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Warn, fmt, ##__VA_ARGS__)
#define LOG_WARN_(logger, fmt, ...) kkem::Logger::Get().log_(logger, {__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Warn, fmt, ##__VA_ARGS__)
#define logwarn_(logger, fmt, ...) kkem::Logger::Get().printf_(logger, {__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Warn, fmt, ##__VA_ARGS__)
#define LOGWARN_(logger) kkem::Logger::LogStream_(logger, {__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Warn)

#define log_err(fmt, ...) kkem::Logger::Get().fmt_printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Error, fmt, ##__VA_ARGS__)
#define LOG_ERR(fmt, ...) kkem::Logger::Get().log({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Error, fmt, ##__VA_ARGS__)
#define logerr(fmt, ...) kkem::Logger::Get().printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Error, fmt, ##__VA_ARGS__)
#define LOGERR() kkem::Logger::LogStream({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Error)

#define log_err_(logger, fmt, ...) kkem::Logger::Get().fmt_printf_(logger, {__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Error, fmt, ##__VA_ARGS__)
#define LOG_ERR_(logger, fmt, ...) kkem::Logger::Get().log_(logger, {__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Error, fmt, ##__VA_ARGS__)
#define logerr_(logger, fmt, ...) kkem::Logger::Get().printf_(logger, {__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Error, fmt, ##__VA_ARGS__)
#define LOGERR_(logger) kkem::Logger::LogStream_(logger, {__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Error)

#define log_fatal(fmt, ...) kkem::Logger::Get().fmt_printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Fatal, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) kkem::Logger::Get().log({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Fatal, fmt, ##__VA_ARGS__)
#define logfatal(fmt, ...) kkem::Logger::Get().printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Fatal, fmt, ##__VA_ARGS__)
#define LOGFATAL() kkem::Logger::LogStream({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Fatal)

#define log_fatal_(logger, fmt, ...) kkem::Logger::Get().fmt_printf_(logger, {__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Fatal, fmt, ##__VA_ARGS__)
#define LOG_FATAL_(logger, fmt, ...) kkem::Logger::Get().log_(logger, {__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Fatal, fmt, ##__VA_ARGS__)
#define logfatal_(logger, fmt, ...) kkem::Logger::Get().printf_(logger, {__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Fatal, fmt, ##__VA_ARGS__)
#define LOGFATAL_(logger) kkem::Logger::LogStream_(logger, {__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Fatal)
