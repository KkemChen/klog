﻿#pragma once
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
#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/bundled/printf.h>
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
namespace kkem
{
	constexpr const char* LOG_PATH             = "logs/test.log"; //默认日志存储路径
	constexpr std::size_t SINGLE_FILE_MAX_SIZE = 20 * 1024 * 1024;//单个日志文件最大大小(20M)
	constexpr std::size_t MAX_STORAGE_DAYS     = 1;               //日志保存时间(天)

	enum LOG_MODE
	{
		STDOUT = 1 << 0,	//主日志控制台输出
		FILEOUT = 1 << 1,	//主日志文件输出
		ASYNC = 1 << 2		//异步日志模式
	};

	enum LOG_LEVEL
	{
		TRACE = 0,
		DEBUG = 1,
		INFO = 2,
		WARN = 3,
		ERROR = 4,
		FATAL = 5,
		OFF = 6,
		N_LEVEL
	};

	class CustomLevelFormatterFlag;
	class CustomRotatingFileSink;

	class Logger final
	{
	public:
		/// let logger like stream
		struct LogStream : public std::ostringstream
		{
		public:
			LogStream(const spdlog::source_loc& loc, kkem::LOG_LEVEL lvl) : _loc(loc), _lvl(lvl) { }

			~LogStream() { flush(); }

			void flush() { Logger::Get().log(_loc, _lvl, str().c_str()); }

		private:
			spdlog::source_loc _loc;
			kkem::LOG_LEVEL _lvl;
		};

		struct LogStream_ : public std::ostringstream
		{
		public:
			LogStream_(const std::string logger, const spdlog::source_loc& loc, kkem::LOG_LEVEL lvl) : _logger(logger),
				_loc(loc), _lvl(lvl) { }

			~LogStream_() { flush_(); }

			void flush_() { Logger::Get().log_(_logger, _loc, _lvl, str().c_str()); }

		private:
			std::string _logger;
			spdlog::source_loc _loc;
			kkem::LOG_LEVEL _lvl;
		};

	public:
		static Logger& Get()
		{
			static Logger Logger;
			return Logger;
		}

		///停止所有日志记录操作并清理内部资源
		void shutdown() { spdlog::shutdown(); }

		///spdlog输出
		template <typename... Args>
		void log(const spdlog::source_loc& loc, kkem::LOG_LEVEL lvl, const char* fmt, const Args&... args);

		///传统printf输出
		void printf(const spdlog::source_loc& loc, kkem::LOG_LEVEL lvl, const char* fmt, ...);

		///fmt的printf输出（不支持格式化非void类型指针）
		template <typename... Args>
		void fmt_printf(const spdlog::source_loc& loc, kkem::LOG_LEVEL lvl, const char* fmt,
		                const Args&... args);

		/*********Exlog**********/
		///spdlog输出
		template <typename... Args>
		void log_(const std::string& logger, const spdlog::source_loc& loc, kkem::LOG_LEVEL lvl, const char* fmt,
		          const Args&... args);

		///传统printf输出
		void printf_(const std::string& logger, const spdlog::source_loc& loc, kkem::LOG_LEVEL lvl, const char* fmt,
		             ...);

		///fmt的printf输出（不支持格式化非void类型指针）
		template <typename... Args>
		void fmt_printf_(const std::string& logger, const spdlog::source_loc& loc, kkem::LOG_LEVEL lvl, const char* fmt,
		                 const Args&... args);

		///设置输出级别
		void set_level(kkem::LOG_LEVEL lvl);
		void set_level_(const std::string& logger, kkem::LOG_LEVEL lvl);

		///设置刷新达到指定级别时自动刷新缓冲区
		void set_flush_on(kkem::LOG_LEVEL lvl) { spdlog::flush_on(static_cast<spdlog::level::level_enum>(lvl)); }

		/**
		 * \brief 初始化日志
		 * \param logPath 日志路径：默认"logs/test.log"
		 * \param mode STDOUT:控制台 FILEOUT:文件 ASYNC:异步模式
		 * \param threadCount 异步模式线程池线程数量
		 * \param logBufferSize 异步模式下日志缓冲区大小
		 * \return true:success  false:failed
		 */
		bool init(const std::string& logPath = LOG_PATH, int mode = STDOUT, std::size_t threadCount = 1,
		          std::size_t logBufferSize  = 32 * 1024);

		/**
		 * \brief 添加额外日志（多用于临时调试）
		 * \param logPath 日志路径：需要避免 额外日志名称 与 主日志名称 重复
		 * \param mode STDOUT:控制台 FILEOUT:文件 ASYNC:异步模式
		 * \return true:success  false:failed
		 */
		bool add_ExLog(const std::string& logPath, int mode = STDOUT);

	private:
		Logger() = default;
		~Logger() = default;
		Logger(const Logger&) = delete;
		void operator=(const Logger&) = delete;

	private:
		std::atomic_bool _isInited = {false};
		spdlog::level::level_enum _logLevel = spdlog::level::trace;
		std::stringstream _ss;

		std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> _map_exLog;
	};

	///自定义FormatterFlag
	class CustomLevelFormatterFlag : public spdlog::custom_flag_formatter
	{
	public:
		void format(const spdlog::details::log_msg& _log_msg, const std::tm&, spdlog::memory_buf_t& dest) override;

		std::unique_ptr<custom_flag_formatter> clone() const override
		{
			return spdlog::details::make_unique<CustomLevelFormatterFlag>();
		}
	};


	/**
	 * \brief 自定义sink
	 *	1.按文件大小分片存储
	 *	2.文件命名：basename+datetime+.log
	 *	3.每次rotate时自动检查当前目录日志文件是否超过设定的时间，并自动清理
	 */
	class CustomRotatingFileSink final : public spdlog::sinks::base_sink<std::mutex>
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
		CustomRotatingFileSink(spdlog::filename_t log_path,
		                       std::size_t max_size,
		                       std::size_t max_storage_days,
		                       bool rotate_on_open = true, const spdlog::file_event_handlers& event_handlers = {});

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

		std::atomic<uint32_t> _max_size;
		std::atomic<uint32_t> _max_storage_days;
		std::size_t _current_size;
		spdlog::details::file_helper _file_helper;
		std::experimental::filesystem::path _log_basename;
		std::experimental::filesystem::path _log_filename;
		std::experimental::filesystem::path _log_parent_path;
		std::experimental::filesystem::path _log_path;
	};


	template <typename... Args>
	void Logger::log(const spdlog::source_loc& loc, kkem::LOG_LEVEL lvl, const char* fmt, const Args&... args)
	{
		spdlog::log(loc, static_cast<spdlog::level::level_enum>(lvl), fmt, args...);
	}

	template <typename... Args>
	void Logger::fmt_printf(const spdlog::source_loc& loc, kkem::LOG_LEVEL lvl, const char* fmt, const Args&... args)
	{
		log(loc, lvl, fmt::sprintf(fmt, args...).c_str());
	}

	template <typename... Args>
	void Logger::log_(const std::string& logger, const spdlog::source_loc& loc, kkem::LOG_LEVEL lvl, const char* fmt,
	                  const Args&... args)
	{
		auto it = _map_exLog.find(logger);
		if (it != _map_exLog.end()) {
			it->second->log(loc, static_cast<spdlog::level::level_enum>(lvl), fmt, args...);
		}
		else {
			log(loc, lvl, fmt, args...);
		}
	}

	template <typename... Args>
	void Logger::fmt_printf_(const std::string& logger, const spdlog::source_loc& loc, kkem::LOG_LEVEL lvl,
	                         const char* fmt, const Args&... args)
	{
		auto it = _map_exLog.find(logger);
		if (it != _map_exLog.end()) {
			it->second->log(loc, static_cast<spdlog::level::level_enum>(lvl), fmt::sprintf(fmt, args...).c_str());
		}
		else {
			log(loc, lvl, fmt::sprintf(fmt, args...).c_str());
		}
	}
}// namespace kkem


#	define 	 log_trace(fmt,...) 		kkem::Logger::Get().fmt_printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::TRACE, fmt, ##__VA_ARGS__)
#	define	 LOG_TRACE(fmt, ...) 		kkem::Logger::Get().log({__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::TRACE, fmt, ##__VA_ARGS__)
#	define 	 logtrace(fmt,...) 		kkem::Logger::Get().printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::TRACE, fmt, ##__VA_ARGS__)
#	define	 LOGTRACE() 			kkem::Logger::LogStream({__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::TRACE)

#	define 	 log_trace_(logger,fmt,...) 		kkem::Logger::Get().fmt_printf_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::TRACE, fmt, ##__VA_ARGS__)
#	define	 LOG_TRACE_(logger,fmt, ...) 		kkem::Logger::Get().log_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::TRACE, fmt, ##__VA_ARGS__)
#	define 	 logtrace_(logger,fmt,...) 		kkem::Logger::Get().printf_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::TRACE, fmt, ##__VA_ARGS__)
#	define	 LOGTRACE_(logger) 			kkem::Logger::LogStream_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::TRACE)


#	define 	 log_debug(fmt,...) 		kkem::Logger::Get().fmt_printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::DEBUG, fmt, ##__VA_ARGS__)
#	define	 LOG_DEBUG(fmt, ...) 		kkem::Logger::Get().log({__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::DEBUG, fmt, ##__VA_ARGS__)
#	define 	 logdebug(fmt,...) 		kkem::Logger::Get().printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::DEBUG, fmt, ##__VA_ARGS__)
#	define	 LOGDEBUG() 			kkem::Logger::LogStream({__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::DEBUG)

#	define 	 log_debug_(logger,fmt,...) 		kkem::Logger::Get().fmt_printf_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::DEBUG, fmt, ##__VA_ARGS__)
#	define	 LOG_DEBUG_(logger,fmt, ...) 		kkem::Logger::Get().log_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::DEBUG, fmt, ##__VA_ARGS__)
#	define 	 logdebug_(logger,fmt,...) 		kkem::Logger::Get().printf_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::DEBUG, fmt, ##__VA_ARGS__)
#	define	 LOGDEBUG_(logger) 			kkem::Logger::LogStream_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::DEBUG)


#	define 	 log_info(fmt,...) 		kkem::Logger::Get().fmt_printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::INFO, fmt, ##__VA_ARGS__)
#	define	 LOG_INFO(fmt, ...) 		kkem::Logger::Get().log({__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::INFO, fmt, ##__VA_ARGS__)
#	define 	 loginfo(fmt,...) 		kkem::Logger::Get().printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::INFO, fmt, ##__VA_ARGS__)
#	define	 LOGINFO() 			kkem::Logger::LogStream({__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::INFO)

#	define 	 log_info_(logger,fmt,...) 		kkem::Logger::Get().fmt_printf_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::INFO, fmt, ##__VA_ARGS__)
#	define	 LOG_INFO_(logger,fmt, ...) 		kkem::Logger::Get().log_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::INFO, fmt, ##__VA_ARGS__)
#	define 	 loginfo_(logger,fmt,...) 		kkem::Logger::Get().printf_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::INFO, fmt, ##__VA_ARGS__)
#	define	 LOGINFO_(logger) 			kkem::Logger::LogStream_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::INFO)


#	define 	 log_warn(fmt,...) 		kkem::Logger::Get().fmt_printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::WARN, fmt, ##__VA_ARGS__)
#	define	 LOG_WARN(fmt, ...) 		kkem::Logger::Get().log({__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::WARN, fmt, ##__VA_ARGS__)
#	define 	 logwarn(fmt,...) 		kkem::Logger::Get().printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::WARN, fmt, ##__VA_ARGS__)
#	define	 LOGWARN() 			kkem::Logger::LogStream({__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::WARN)

#	define 	 log_warn_(logger,fmt,...) 		kkem::Logger::Get().fmt_printf_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::WARN, fmt, ##__VA_ARGS__)
#	define	 LOG_WARN_(logger,fmt, ...) 		kkem::Logger::Get().log_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::WARN, fmt, ##__VA_ARGS__)
#	define 	 logwarn_(logger,fmt,...) 		kkem::Logger::Get().printf_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::WARN, fmt, ##__VA_ARGS__)
#	define	 LOGWARN_(logger) 			kkem::Logger::LogStream_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::WARN)


#	define 	 log_err(fmt,...) 		kkem::Logger::Get().fmt_printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::ERROR, fmt, ##__VA_ARGS__)
#	define	 LOG_ERR(fmt, ...) 		kkem::Logger::Get().log({__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::ERROR, fmt, ##__VA_ARGS__)
#	define 	 logerr(fmt,...) 		kkem::Logger::Get().printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::ERROR, fmt, ##__VA_ARGS__)
#	define	 LOGERR() 			kkem::Logger::LogStream({__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::ERROR)

#	define 	 log_err_(logger,fmt,...) 		kkem::Logger::Get().fmt_printf_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::ERROR, fmt, ##__VA_ARGS__)
#	define	 LOG_ERR_(logger,fmt, ...) 		kkem::Logger::Get().log_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::ERROR, fmt, ##__VA_ARGS__)
#	define 	 logerr_(logger,fmt,...) 		kkem::Logger::Get().printf_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::ERROR, fmt, ##__VA_ARGS__)
#	define	 LOGERR_(logger) 			kkem::Logger::LogStream_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::ERROR)


#	define 	 log_fatal(fmt,...) 		kkem::Logger::Get().fmt_printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::FATAL, fmt, ##__VA_ARGS__)
#	define	 LOG_FATAL(fmt, ...) 		kkem::Logger::Get().log({__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::FATAL, fmt, ##__VA_ARGS__)
#	define 	 logfatal(fmt,...) 		kkem::Logger::Get().printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::FATAL, fmt, ##__VA_ARGS__)
#	define	 LOGFATAL() 			kkem::Logger::LogStream({__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::FATAL)

#	define 	 log_fatal_(logger,fmt,...) 		kkem::Logger::Get().fmt_printf_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::FATAL, fmt, ##__VA_ARGS__)
#	define	 LOG_FATAL_(logger,fmt, ...) 		kkem::Logger::Get().log_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::FATAL, fmt, ##__VA_ARGS__)
#	define 	 logfatal_(logger,fmt,...) 		kkem::Logger::Get().printf_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::FATAL, fmt, ##__VA_ARGS__)
#	define	 LOGFATAL_(logger) 			kkem::Logger::LogStream_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LOG_LEVEL::FATAL)