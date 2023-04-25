#pragma once
#include <cstdarg>
#include <fstream>
#include <sstream>
#include <memory>
#include <regex>
#include <iostream>
#include <experimental/filesystem>  //C++ 11兼容   C++17可直接用<filsesystem>

#include <spdlog/spdlog.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>

#if WIN32
#ifndef NOMINMAX
#	undef min
#	undef max
#endif
#endif


/// spdlog wrap class
namespace klog
{
constexpr const char* LOG_PATH_ = "logs/test.log";						//默认日志存储路径
constexpr const std::size_t SINGLE_FILE_MAX_SIZE = 20 * 1024 * 1024;	//单个日志文件最大大小(20M)
constexpr const std::size_t MAX_STORAGE_DAYS = 1;						//日志保存时间(天)

///自定义level flag
class custom_level_formatter_flag : public spdlog::custom_flag_formatter
{
public:
	void format(const spdlog::details::log_msg& _log_msg, const std::tm&, spdlog::memory_buf_t& dest) override;

	std::unique_ptr<custom_flag_formatter> clone() const override;
};


/**
 * \brief 自定义sink
 *	1.按文件大小分片存储
 *	2.文件命名：basename+datetime+.log
 *	3.每次rotate时自动检查当前目录日志文件是否超过设定的时间，并自动清理
 */
class custom_rotating_file_sink final : public spdlog::sinks::base_sink<std::mutex>
{
public:
	/**
	 * \brief 
	 * \param log_path 日志存储路径
	 * \param max_size 单文件最大容量
	 * \param max_storage_days 最大保存天数
	 * \param rotate_on_open 默认true
	 * \param event_handlers 默认
	 */
	custom_rotating_file_sink(spdlog::filename_t log_path,
	                          std::size_t max_size,
	                          std::size_t max_storage_days,
	                          bool rotate_on_open = true, const spdlog::file_event_handlers& event_handlers = {});

	spdlog::filename_t calc_filename();
	spdlog::filename_t filename();

protected:
	///将日志消息写入到输出目标 
	void sink_it_(const spdlog::details::log_msg& msg) override;

	///刷新日志缓冲区 
	void flush_() override;

private:
	///日志轮转 
	void rotate_();

	///清理过期日志文件 
	void cleanup_file_();

	//spdlog::filename_t base_filename_;
	std::size_t m_max_size;
	std::size_t max_storage_days;
	std::size_t m_current_size;
	spdlog::details::file_helper m_file_helper;
	std::experimental::filesystem::path m_log_basename;
	std::experimental::filesystem::path m_log_filename;
	std::experimental::filesystem::path m_log_parent_path;
	std::experimental::filesystem::path m_log_path;
};

class logger final
{
	friend class custom_rotating_file_sink;

public:
	/// let logger like stream
	struct log_stream : public std::ostringstream
	{
	public:
		log_stream(const spdlog::source_loc& _loc, spdlog::level::level_enum _lvl);

		~log_stream();

		void flush();

	private:
		spdlog::source_loc loc;
		spdlog::level::level_enum lvl = spdlog::level::info;
	};

public:
	static logger& get();

	///停止所有日志记录操作并清理内部资源
	void shutdown();

	///spdlog输出
	template <typename... Args>
	void log(const spdlog::source_loc& loc, spdlog::level::level_enum lvl, const char* fmt, const Args&... args);
	///传统printf输出
	void printf(const spdlog::source_loc& loc, spdlog::level::level_enum lvl, const char* fmt, ...);

	///fmt的printf输出（不支持格式化非void类型指针）
	template <typename... Args>
	void fmt_printf(const spdlog::source_loc& loc, spdlog::level::level_enum lvl, const char* fmt, const Args&... args);
	spdlog::level::level_enum level();

	///设置输出级别
	void set_level(spdlog::level::level_enum lvl);

	///设置刷新达到指定级别时自动刷新缓冲区
	void set_flush_on(spdlog::level::level_enum lvl);

private:
	///初始化日志
	bool init(const std::string& log_path = LOG_PATH_);

private:
	logger();
	~logger() = default;
	logger(const logger&) = delete;
	void operator=(const logger&) = delete;

private:
	std::atomic_bool is_inited = {false};
	spdlog::level::level_enum log_level = spdlog::level::trace;
	std::stringstream m_ss;
};
} // namespace klog


#define LOG_LEVEL_TRACE    0
#define LOG_LEVEL_DEBUG    1
#define LOG_LEVEL_INFO     2
#define LOG_LEVEL_WARN     3
#define LOG_LEVEL_ERROR    4
#define LOG_LEVEL_FATAL    5
#define LOG_LEVEL_CLOSE    6


#if (LOGGER_LEVEL <= LOG_LEVEL_TRACE)
#	define	 LOG_TRACE(fmt, ...) 		spdlog::log({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::trace, fmt, ##__VA_ARGS__);
#	define 	 logtrace(fmt,...) 		klog::logger::get().printf({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::trace, fmt, ##__VA_ARGS__);
#	define	 LOGTRACE() 			klog::logger::log_stream({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::trace)
#else
#	define	 LOG_TRACE(fmt, ...)
#	define 	 PRINT_TRACE(fmt,...)
#	define	 STREAM_TRACE() klog::logger_none::get()
#endif

#if (LOGGER_LEVEL <= LOG_LEVEL_DEBUG)
#	define	 LOG_DEBUG(fmt, ...) 		spdlog::log({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::debug, fmt, ##__VA_ARGS__);
#	define 	 logdebug(fmt,...) 		klog::logger::get().printf({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::debug, fmt, ##__VA_ARGS__);
#	define	 LOGDEBUG() 			klog::logger::log_stream({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::debug)
#else
#	define	 LOG_DEBUG(fmt, ...)
#	define 	 PRINT_DEBUG(fmt,...)
#	define	 STREAM_DEBUG() klog::logger_none::get()
#endif

#if (LOGGER_LEVEL <= LOG_LEVEL_INFO)
#	define	 LOG_INFO(fmt, ...) 		spdlog::log({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::info, fmt, ##__VA_ARGS__);
#	define 	 loginfo(fmt,...) 		klog::logger::get().printf({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::info, fmt, ##__VA_ARGS__);
#	define	 LOGINFO() 			klog::logger::log_stream({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::info)
#else
#	define	 LOG_INFO(fmt, ...)
#	define 	 PRINT_INFO(fmt,...)
#	define	 STREAM_INFO() klog::logger_none::get()
#endif

#if (LOGGER_LEVEL <= LOG_LEVEL_WARN)
#	define	 LOG_WARN(fmt, ...) 		spdlog::log({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::warn, fmt, ##__VA_ARGS__);
#	define 	 logwarn(fmt,...) 		klog::logger::get().printf({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::warn, fmt, ##__VA_ARGS__);
#	define	 LOGWARN() 			klog::logger::log_stream({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::warn)
#else
#	define	 LOG_WARN(fmt, ...)
#	define 	 PRINT_WARN(fmt,...)
#	define	 STREAM_WARN() klog::logger_none::get()
#endif

#if (LOGGER_LEVEL <= LOG_LEVEL_ERROR)
#	define	 LOG_ERROR(fmt, ...) 		spdlog::log({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::err, fmt, ##__VA_ARGS__);
#	define 	 logerror(fmt,...) 		klog::logger::get().printf({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::err, fmt, ##__VA_ARGS__);
#	define	 LOGERROR() 			klog::logger::log_stream({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::err)
#else
#	define	 LOG_ERROR(fmt, ...)
#	define 	 PRINT_ERROR(fmt,...)
#	define	 STREAM_ERROR() klog::logger_none::get()
#endif

#if (LOGGER_LEVEL <= LOG_LEVEL_FATAL)
#	define	 LOG_FATAL(fmt, ...) 		spdlog::log({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::critical, fmt, ##__VA_ARGS__);
#	define 	 logfatal(fmt,...) 		klog::logger::get().printf({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::critical, fmt, ##__VA_ARGS__);
#	define	 LOGFATAL() 			klog::logger::log_stream({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::critical)
#else
#	define	 LOG_FATAL(fmt, ...)
#	define 	 PRINT_FATAL(fmt,...)
#	define	 STREAM_FATAL() klog::logger_none::get()
#endif
