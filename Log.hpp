#pragma once

#include <cstdarg>
#include <fstream>
#include <sstream>
#include <memory>
#include <regex>
#include <iostream>
#include <experimental/filesystem>  //C++ 11兼容   C++17可直接用<filsesystem>


#if WIN32
#ifndef NOMINMAX
#	undef min
#	undef max
#endif
#endif


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


/// spdlog wrap class
namespace kkem
{
	constexpr const char* LOG_PATH             = "logs/test.log"; //默认日志存储路径
	constexpr std::size_t SINGLE_FILE_MAX_SIZE = 20 * 1024 * 1024;//单个日志文件最大大小(20M)
	constexpr std::size_t MAX_STORAGE_DAYS     = 1;               //日志保存时间(天)

	enum LogMode
	{
		STDOUT = 1 << 0,	//主日志控制台输出
		FILEOUT = 1 << 1,	//主日志文件输出
		ASYNC = 1 << 2		//异步日志模式
	};

	enum LogLevel
	{
		Trace = 0,
		Debug = 1,
		Info = 2,
		Warn = 3,
		Error = 4,
		Fatal = 5,
		Off = 6,
		N_Level
	};

#define xxx(sink_)	sink_->set_color(\
					static_cast<spdlog::level::level_enum>(LogLevel::Trace), "\033[36m");\
					sink_->set_color(\
					static_cast<spdlog::level::level_enum>(LogLevel::Debug), "\033[1;34m");\
					sink_->set_color(\
					static_cast<spdlog::level::level_enum>(LogLevel::Info), "\033[1;32m");\
					sink_->set_color(\
					static_cast<spdlog::level::level_enum>(LogLevel::Warn), "\033[1;33m");\
					sink_->set_color(\
					static_cast<spdlog::level::level_enum>(LogLevel::Error), "\033[1;31m");\
					sink_->set_color(\
					static_cast<spdlog::level::level_enum>(LogLevel::Fatal), "\033[1;35m");


	class CustomLevelFormatterFlag;
	class CustomRotatingFileSink;

	class Logger final
	{
	public:
		/// let logger like stream
		struct LogStream : public std::ostringstream
		{
		public:
			LogStream(const spdlog::source_loc& loc, kkem::LogLevel lvl) : _loc(loc), _lvl(lvl) { }

			~LogStream() { flush(); }

			void flush() { Logger::Get().log(_loc, _lvl, str().c_str()); }

		private:
			spdlog::source_loc _loc;
			kkem::LogLevel _lvl;
		};

		struct LogStream_ : public std::ostringstream
		{
		public:
			LogStream_(const std::string logger, const spdlog::source_loc& loc, kkem::LogLevel lvl) : _logger(logger),
				_loc(loc), _lvl(lvl) { }

			~LogStream_() { flush_(); }

			void flush_() { Logger::Get().log_(_logger, _loc, _lvl, str().c_str()); }

		private:
			std::string _logger;
			spdlog::source_loc _loc;
			kkem::LogLevel _lvl;
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
		void log(const spdlog::source_loc& loc, kkem::LogLevel lvl, const char* fmt, const Args&... args);

		///传统printf输出
		void printf(const spdlog::source_loc& loc, kkem::LogLevel lvl, const char* fmt, ...);

		///fmt的printf输出（不支持格式化非void类型指针）
		template <typename... Args>
		void fmt_printf(const spdlog::source_loc& loc, kkem::LogLevel lvl, const char* fmt,
		                const Args&... args);

		/*********Exlog**********/
		///spdlog输出
		template <typename... Args>
		void log_(const std::string& logger, const spdlog::source_loc& loc, kkem::LogLevel lvl, const char* fmt,
		          const Args&... args);

		///传统printf输出
		void printf_(const std::string& logger, const spdlog::source_loc& loc, kkem::LogLevel lvl, const char* fmt,
		             ...);

		///fmt的printf输出（不支持格式化非void类型指针）
		template <typename... Args>
		void fmt_printf_(const std::string& logger, const spdlog::source_loc& loc, kkem::LogLevel lvl, const char* fmt,
		                 const Args&... args);

		///设置输出级别
		void set_level(kkem::LogLevel lvl)
		{
			_logLevel = static_cast<spdlog::level::level_enum>(lvl);
			spdlog::set_level(_logLevel);
		}

		void set_level_(const std::string& logger, kkem::LogLevel lvl)
		{
			auto it = _map_exLog.find(logger);
			if (it != _map_exLog.end()) {
				it->second->set_level(static_cast<spdlog::level::level_enum>(lvl));
			}
		}

		///设置刷新达到指定级别时自动刷新缓冲区
		void set_flush_on(kkem::LogLevel lvl) { spdlog::flush_on(static_cast<spdlog::level::level_enum>(lvl)); }

		/**
		 * \brief 初始化日志
		 * \param logPath 日志路径：默认"logs/test.log"
		 * \param mode STDOUT:控制台 FILE:文件 ASYNC:异步模式 
		 * \param threadCount 异步模式线程池线程数量
		 * \param logBufferSize 异步模式下日志缓冲区大小
		 * \return true:success  false:failed
		 */
		bool init(const std::string& logPath = LOG_PATH, int mode = STDOUT, std::size_t threadCount = 1,
		          std::size_t logBufferSize  = 32 * 1024);

		/**
		 * \brief 添加额外日志（多用于临时调试）
		 * \param logPath 日志路径：需要避免 额外日志名称 与 主日志名称 重复
		 * \param mode STDOUT:控制台 FILE:文件 ASYNC:异步模式
		 * \return true:success  false:failed
		 */
		bool add_ExLog(const std::string& logPath, int mode = STDOUT);

	private:
		Logger()                      = default;
		~Logger()                     = default;
		Logger(const Logger&)         = delete;
		void operator=(const Logger&) = delete;

	private:
		std::atomic_bool _isInited          = {false};
		spdlog::level::level_enum _logLevel = spdlog::level::trace;
		std::stringstream _ss;

		std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> _map_exLog;
	};

	template <typename... Args>
	inline void Logger::log(const spdlog::source_loc& loc, kkem::LogLevel lvl, const char* fmt, const Args&... args)
	{
		spdlog::log(loc, static_cast<spdlog::level::level_enum>(lvl), fmt, args...);
	}

	inline void kkem::Logger::printf(const spdlog::source_loc& loc, kkem::LogLevel lvl, const char* fmt, ...)
	{
		auto fun = [](void* self, const char* fmt, va_list al) {
			auto thiz = static_cast<Logger*>(self);
			char* buf = nullptr;
			int len   = vasprintf(&buf, fmt, al);
			if (len != -1) {
				thiz->_ss << std::string(buf, len);
				free(buf);
			}
		};

		va_list al;
		va_start(al, fmt);
		fun(this, fmt, al);
		va_end(al);
		log(loc, lvl, _ss.str().c_str());
		_ss.clear();
		_ss.str("");
	}

	template <typename... Args>
	inline void Logger::fmt_printf(const spdlog::source_loc& loc, kkem::LogLevel lvl, const char* fmt,
	                               const Args&... args)
	{
		log(loc, lvl, fmt::sprintf(fmt, args...).c_str());
	}

	template <typename... Args>
	inline void Logger::log_(const std::string& logger, const spdlog::source_loc& loc, kkem::LogLevel lvl,
	                         const char* fmt,
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
	inline void Logger::fmt_printf_(const std::string& logger, const spdlog::source_loc& loc, kkem::LogLevel lvl,
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

	inline void kkem::Logger::printf_(const std::string& logger, const spdlog::source_loc& loc, kkem::LogLevel lvl,
	                                  const char* fmt, ...)
	{
		auto fun = [](void* self, const char* fmt, va_list al) {
			auto thiz = static_cast<Logger*>(self);
			char* buf = nullptr;
			int len   = vasprintf(&buf, fmt, al);
			if (len != -1) {
				thiz->_ss << std::string(buf, len);
				free(buf);
			}
		};

		va_list al;
		va_start(al, fmt);
		fun(this, fmt, al);
		va_end(al);
		log_(logger, loc, lvl, _ss.str().c_str());
		_ss.clear();
		_ss.str("");
	}


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

	/***************/
	inline bool Logger::init(const std::string& logPath, const int mode, const std::size_t threadCount,
	                         const std::size_t logBufferSize)
	{
		if (_isInited) return true;
		try {
			namespace fs = std::experimental::filesystem;
			fs::path log_file_path(logPath);
			fs::path log_filename = log_file_path.filename();

			spdlog::filename_t basename, ext;
			std::tie(basename, ext) = spdlog::details::file_helper::split_by_extension(log_filename.string());

			//spdlog::init_thread_pool(log_buffer_size, std::thread::hardware_concurrency());
			spdlog::init_thread_pool(logBufferSize, threadCount);
			std::vector<spdlog::sink_ptr> sinks;

			// constexpr std::size_t max_file_size = 50 * 1024 * 1024; // 50mb
			//auto rotatingSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_file_path, 20 * 1024, 10);

			//auto daily_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(log_path.string(), 23, 59); //日志滚动更新时间：每天23:59更新
			//sinks.push_back(daily_sink);

			// auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.string(), true);
			// sinks.push_back(file_sink);

			//控制台输出
			if (mode & STDOUT) {
#if defined(_DEBUG) && defined(WIN32) && !defined(NO_CONSOLE_LOG)
				auto ms_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
				xxx(ms_sink)
					sinks.push_back(ms_sink);
#endif //  _DEBUG

#if !defined(WIN32) && !defined(NO_CONSOLE_LOG)
				auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
				xxx(console_sink)
					sinks.push_back(console_sink);
#endif
			}

			//文件输出
			if (mode & FILEOUT) {
				auto rotatingSink = std::make_shared<kkem::CustomRotatingFileSink>(logPath, SINGLE_FILE_MAX_SIZE,
					MAX_STORAGE_DAYS);
				sinks.push_back(rotatingSink);
			}

			//异步
			if (mode & ASYNC) {
				spdlog::set_default_logger(std::make_shared<spdlog::async_logger>(basename, sinks.begin(), sinks.end(),
					                           spdlog::thread_pool(), spdlog::async_overflow_policy::block));
			}
			else {
				spdlog::set_default_logger(std::make_shared<spdlog::logger>(basename, sinks.begin(), sinks.end()));
			}

			auto formatter = std::make_unique<spdlog::pattern_formatter>();

			formatter->add_flag<CustomLevelFormatterFlag>('*').
			           set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [%*] |%t| [%s:%# (%!)]: %v%$");

			spdlog::set_formatter(std::move(formatter));

			spdlog::flush_every(std::chrono::seconds(5));
			spdlog::flush_on(spdlog::level::info);
			spdlog::set_level(_logLevel);
		} catch (std::exception_ptr e) {
			assert(false);
			return false;
		}
		_isInited = true;
		return true;
	}

	inline bool Logger::add_ExLog(const std::string& logPath, int mode)
	{
		try {
			namespace fs = std::experimental::filesystem;
			fs::path log_file_path(logPath);
			fs::path log_filename = log_file_path.filename();

			spdlog::filename_t basename, ext;
			std::tie(basename, ext) = spdlog::details::file_helper::split_by_extension(log_filename.string());

			std::vector<spdlog::sink_ptr> sinks;

			//控制台输出
			if (mode & STDOUT) {
#if defined(_DEBUG) && defined(WIN32) && !defined(NO_CONSOLE_LOG)
				auto ms_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
				xxx(ms_sink)
					sinks.push_back(ms_sink);
#endif //  _DEBUG

#if !defined(WIN32) && !defined(NO_CONSOLE_LOG)
				auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
				xxx(console_sink)
					sinks.push_back(console_sink);
#endif
			}
			//文件输出
			if (mode & FILEOUT) {
				auto rotatingSink = std::make_shared<kkem::CustomRotatingFileSink>(logPath, SINGLE_FILE_MAX_SIZE,
					MAX_STORAGE_DAYS);
				sinks.push_back(rotatingSink);
			}

			std::shared_ptr<spdlog::logger> exLog;
			if (mode & ASYNC) {
				exLog = std::make_shared<spdlog::async_logger>(basename, sinks.begin(), sinks.end(),
				                                               spdlog::thread_pool(),
				                                               spdlog::async_overflow_policy::block);
			}
			else {
				exLog = std::make_shared<spdlog::logger>(basename, sinks.begin(), sinks.end());
			}

			auto formatter = std::make_unique<spdlog::pattern_formatter>();

			formatter->add_flag<CustomLevelFormatterFlag>('*').
			           set_pattern("%^[%n][%Y-%m-%d %H:%M:%S.%e] [%*] |%t| [%s:%# (%!)]: %v%$");

			exLog->set_formatter(std::move(formatter));
			exLog->flush_on(spdlog::level::trace);
			exLog->set_level(spdlog::level::trace);
			_map_exLog.emplace(basename, exLog);
		} catch (std::exception_ptr e) {
			assert(false);
			return false;
		}
		return true;
	}


	inline CustomRotatingFileSink::CustomRotatingFileSink(spdlog::filename_t log_path,
	                                                      std::size_t max_size,
	                                                      std::size_t max_storage_days, bool rotate_on_open,
	                                                      const spdlog::file_event_handlers& event_handlers)
		: _log_path(log_path)
		, _max_size(max_size)
		, _max_storage_days(max_storage_days)
		, _file_helper{event_handlers}
	{
		if (max_size == 0) {
			spdlog::throw_spdlog_ex("rotating sink constructor: max_size arg cannot be zero");
		}

		if (max_storage_days > 365 * 2) {
			spdlog::throw_spdlog_ex("rotating sink constructor: max_storage_days arg cannot exceed 2 years");
		}

		_log_parent_path = _log_path.parent_path();
		_log_filename    = _log_path.filename();

		spdlog::filename_t basename, ext;
		std::tie(basename, ext) = spdlog::details::file_helper::split_by_extension(_log_filename.string());

		_log_basename = basename;

		_file_helper.open(calc_filename());
		_current_size = _file_helper.size();// expensive. called only once

		cleanup_file_();

		if (rotate_on_open && _current_size > 0) {
			rotate_();
			_current_size = 0;
		}
	}

	inline spdlog::filename_t CustomRotatingFileSink::calc_filename()
	{
		auto now         = std::chrono::system_clock::now();
		std::time_t time = std::chrono::system_clock::to_time_t(now);
		std::tm tm       = *std::localtime(&time);

		return _log_parent_path.empty()
			       ? spdlog::fmt_lib::format("{:%Y-%m-%d}/{}_{:%Y-%m-%d-%H:%M:%S}.log", tm, _log_basename.string(), tm)
			       : spdlog::fmt_lib::format("{}/{:%Y-%m-%d}/{}_{:%Y-%m-%d-%H:%M:%S}.log",
			                                 _log_parent_path.string(),
			                                 tm,
			                                 _log_basename.string(),
			                                 tm);/// logs/yyyy-mm-dd/test_yyyy-mm-dd-h-m-s.log
	}

	inline spdlog::filename_t CustomRotatingFileSink::filename()
	{
		std::lock_guard<std::mutex> lock(base_sink<std::mutex>::mutex_);
		return _file_helper.filename();
	}

	inline void CustomRotatingFileSink::sink_it_(const spdlog::details::log_msg& msg)
	{
		spdlog::memory_buf_t formatted;
		base_sink<std::mutex>::formatter_->format(msg, formatted);
		auto new_size = _current_size + formatted.size();

		if (new_size > _max_size) {
			_file_helper.flush();
			if (_file_helper.size() > 0) {
				rotate_();
				new_size = formatted.size();
			}
		}
		_file_helper.write(formatted);
		_current_size = new_size;
	}

	inline void CustomRotatingFileSink::rotate_()
	{
		_file_helper.close();

		cleanup_file_();

		spdlog::filename_t filename = calc_filename();

		_file_helper.open(filename);
	}

	inline void CustomRotatingFileSink::cleanup_file_()
	{
		namespace fs = std::experimental::filesystem;

		const std::regex folder_regex(R"(\d{4}-\d{2}-\d{2})");
		//const std::chrono::hours max_age();

		for (auto& p : fs::directory_iterator(_log_parent_path)) {
			if (fs::is_directory(p)) {
				const std::string folder_name = p.path().filename().string();
				if (std::regex_match(folder_name, folder_regex)) {
					const int year = std::stoi(folder_name.substr(0, 4));
					const int mon  = std::stoi(folder_name.substr(5, 7));
					const int day  = std::stoi(folder_name.substr(8, 10));

					std::tm date1_tm{0, 0, 0, day, mon - 1, year - 1900};
					const std::time_t date_tt = std::mktime(&date1_tm);

					const std::chrono::system_clock::time_point time = std::chrono::system_clock::from_time_t(date_tt);

					const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

					const std::chrono::duration<double> duration = now - time;

					const double days = duration.count() / (24 * 60 * 60);// 将时间差转换为天数

					if (days > _max_storage_days) {
						fs::remove_all(p);
						std::cout << "Clean up log files older than" << _max_storage_days << " days" << std::endl;
					}
				}
			}
		}
	}


	class CustomLevelFormatterFlag : public spdlog::custom_flag_formatter
	{
	public:
		void format(const spdlog::details::log_msg& _log_msg, const std::tm&, spdlog::memory_buf_t& dest) override
		{
			switch (_log_msg.level) {
#undef DEBUG
#undef ERROR
#define xx(level,msg) case level: {\
				static std::string msg = #msg; \
					dest.append(msg.data(), msg.data() + msg.size()); \
					break; }

			xx(spdlog::level::trace, TRACE)
			xx(spdlog::level::debug, DEBUG)
			xx(spdlog::level::info, INFO)
			xx(spdlog::level::warn, WARN)
			xx(spdlog::level::err, ERROR)
			xx(spdlog::level::critical, FATAL)
#undef xx
			default: break;
			}
		}

		std::unique_ptr<custom_flag_formatter> clone() const override
		{
			return spdlog::details::make_unique<CustomLevelFormatterFlag>();
		}
	};
#undef xxx
}// namespace kkem


#	define 	 log_trace(fmt,...) 		kkem::Logger::Get().fmt_printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Trace, fmt, ##__VA_ARGS__)
#	define	 LOG_TRACE(fmt, ...) 		kkem::Logger::Get().log({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Trace, fmt, ##__VA_ARGS__)
#	define 	 logtrace(fmt,...) 		kkem::Logger::Get().printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Trace, fmt, ##__VA_ARGS__)
#	define	 LOGTRACE() 			kkem::Logger::LogStream({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Trace)

#	define 	 log_trace_(logger,fmt,...) 		kkem::Logger::Get().fmt_printf_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Trace, fmt, ##__VA_ARGS__)
#	define	 LOG_TRACE_(logger,fmt, ...) 		kkem::Logger::Get().log_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Trace, fmt, ##__VA_ARGS__)
#	define 	 logtrace_(logger,fmt,...) 		kkem::Logger::Get().printf_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Trace, fmt, ##__VA_ARGS__)
#	define	 LOGTRACE_(logger) 			kkem::Logger::LogStream_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Trace)


#	define 	 log_debug(fmt,...) 		kkem::Logger::Get().fmt_printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Debug, fmt, ##__VA_ARGS__)
#	define	 LOG_DEBUG(fmt, ...) 		kkem::Logger::Get().log({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Debug, fmt, ##__VA_ARGS__)
#	define 	 logdebug(fmt,...) 		kkem::Logger::Get().printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Debug, fmt, ##__VA_ARGS__)
#	define	 LOGDEBUG() 			kkem::Logger::LogStream({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Debug)

#	define 	 log_debug_(logger,fmt,...) 		kkem::Logger::Get().fmt_printf_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Debug, fmt, ##__VA_ARGS__)
#	define	 LOG_DEBUG_(logger,fmt, ...) 		kkem::Logger::Get().log_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Debug, fmt, ##__VA_ARGS__)
#	define 	 logdebug_(logger,fmt,...) 		kkem::Logger::Get().printf_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Debug, fmt, ##__VA_ARGS__)
#	define	 LOGDEBUG_(logger) 			kkem::Logger::LogStream_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Debug)


#	define 	 log_info(fmt,...) 		kkem::Logger::Get().fmt_printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Info, fmt, ##__VA_ARGS__)
#	define	 LOG_INFO(fmt, ...) 		kkem::Logger::Get().log({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Info, fmt, ##__VA_ARGS__)
#	define 	 loginfo(fmt,...) 		kkem::Logger::Get().printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Info, fmt, ##__VA_ARGS__)
#	define	 LOGINFO() 			kkem::Logger::LogStream({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Info)

#	define 	 log_info_(logger,fmt,...) 		kkem::Logger::Get().fmt_printf_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Info, fmt, ##__VA_ARGS__)
#	define	 LOG_INFO_(logger,fmt, ...) 		kkem::Logger::Get().log_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Info, fmt, ##__VA_ARGS__)
#	define 	 loginfo_(logger,fmt,...) 		kkem::Logger::Get().printf_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Info, fmt, ##__VA_ARGS__)
#	define	 LOGINFO_(logger) 			kkem::Logger::LogStream_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Info)


#	define 	 log_warn(fmt,...) 		kkem::Logger::Get().fmt_printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Warn, fmt, ##__VA_ARGS__)
#	define	 LOG_WARN(fmt, ...) 		kkem::Logger::Get().log({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Warn, fmt, ##__VA_ARGS__)
#	define 	 logwarn(fmt,...) 		kkem::Logger::Get().printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Warn, fmt, ##__VA_ARGS__)
#	define	 LOGWARN() 			kkem::Logger::LogStream({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Warn)

#	define 	 log_warn_(logger,fmt,...) 		kkem::Logger::Get().fmt_printf_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Warn, fmt, ##__VA_ARGS__)
#	define	 LOG_WARN_(logger,fmt, ...) 		kkem::Logger::Get().log_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Warn, fmt, ##__VA_ARGS__)
#	define 	 logwarn_(logger,fmt,...) 		kkem::Logger::Get().printf_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Warn, fmt, ##__VA_ARGS__)
#	define	 LOGWARN_(logger) 			kkem::Logger::LogStream_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Warn)


#	define 	 log_err(fmt,...) 		kkem::Logger::Get().fmt_printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Error, fmt, ##__VA_ARGS__)
#	define	 LOG_ERR(fmt, ...) 		kkem::Logger::Get().log({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Error, fmt, ##__VA_ARGS__)
#	define 	 logerr(fmt,...) 		kkem::Logger::Get().printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Error, fmt, ##__VA_ARGS__)
#	define	 LOGERR() 			kkem::Logger::LogStream({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Error)

#	define 	 log_err_(logger,fmt,...) 		kkem::Logger::Get().fmt_printf_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Error, fmt, ##__VA_ARGS__)
#	define	 LOG_ERR_(logger,fmt, ...) 		kkem::Logger::Get().log_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Error, fmt, ##__VA_ARGS__)
#	define 	 logerr_(logger,fmt,...) 		kkem::Logger::Get().printf_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Error, fmt, ##__VA_ARGS__)
#	define	 LOGERR_(logger) 			kkem::Logger::LogStream_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Error)


#	define 	 log_fatal(fmt,...) 		kkem::Logger::Get().fmt_printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Fatal, fmt, ##__VA_ARGS__)
#	define	 LOG_FATAL(fmt, ...) 		kkem::Logger::Get().log({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Fatal, fmt, ##__VA_ARGS__)
#	define 	 logfatal(fmt,...) 		kkem::Logger::Get().printf({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Fatal, fmt, ##__VA_ARGS__)
#	define	 LOGFATAL() 			kkem::Logger::LogStream({__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Fatal)

#	define 	 log_fatal_(logger,fmt,...) 		kkem::Logger::Get().fmt_printf_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Fatal, fmt, ##__VA_ARGS__)
#	define	 LOG_FATAL_(logger,fmt, ...) 		kkem::Logger::Get().log_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Fatal, fmt, ##__VA_ARGS__)
#	define 	 logfatal_(logger,fmt,...) 		kkem::Logger::Get().printf_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Fatal, fmt, ##__VA_ARGS__)
#	define	 LOGFATAL_(logger) 			kkem::Logger::LogStream_(logger,{__FILE__, __LINE__, __FUNCTION__}, kkem::LogLevel::Fatal)
