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
namespace klog
{
	constexpr const char* LOG_PATH_ = "logs/test.log"; //默认日志存储路径
	constexpr std::size_t SINGLE_FILE_MAX_SIZE = 20 * 1024 * 1024;//单个日志文件最大大小(20M)
	constexpr std::size_t MAX_STORAGE_DAYS = 1;               //日志保存时间(天)

	class custom_level_formatter_flag;

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
		void flush_() override { m_file_helper.flush(); }

	private:
		///日志轮转 
		void rotate_();

		///清理过期日志文件 
		void cleanup_file_();

		std::size_t m_max_size;
		std::size_t m_max_storage_days;
		std::size_t m_current_size;
		spdlog::details::file_helper m_file_helper;
		std::experimental::filesystem::path m_log_basename;
		std::experimental::filesystem::path m_log_filename;
		std::experimental::filesystem::path m_log_parent_path;
		std::experimental::filesystem::path m_log_path;
	};

	class logger final
	{
	public:
		/// let logger like stream
		struct log_stream : public std::ostringstream
		{
		public:
			log_stream(const spdlog::source_loc& _loc, spdlog::level::level_enum _lvl) : loc(_loc), lvl(_lvl) { }

			~log_stream() { flush(); }

			void flush() { logger::get().log(loc, lvl, str().c_str()); }

		private:
			spdlog::source_loc loc;
			spdlog::level::level_enum lvl = spdlog::level::info;
		};

	public:
		static logger& get()
		{
			static logger logger;
			return logger;
		}

		///停止所有日志记录操作并清理内部资源
		void shutdown() { spdlog::shutdown(); }

		///spdlog输出
		template <typename... Args>
		void log(const spdlog::source_loc& loc, spdlog::level::level_enum lvl, const char* fmt, const Args&... args)
		{
			spdlog::log(loc, lvl, fmt, args...);
		}

		///传统printf输出
		void printf(const spdlog::source_loc& loc, spdlog::level::level_enum lvl, const char* fmt, ...)
		{
			auto fun = [](void* self, const char* fmt, va_list al) {
				auto thiz = static_cast<logger*>(self);
				char* buf = nullptr;
				int len = vasprintf(&buf, fmt, al);
				if (len != -1) {
					thiz->m_ss << std::string(buf, len);
					free(buf);
				}
			};

			va_list al;
			va_start(al, fmt);
			fun(this, fmt, al);
			va_end(al);
			log(loc, lvl, m_ss.str().c_str());
			m_ss.clear();
			m_ss.str("");
		}


		///fmt的printf输出（不支持格式化非void类型指针）
		template <typename... Args>
		void fmt_printf(const spdlog::source_loc& loc, spdlog::level::level_enum lvl, const char* fmt,
			const Args&... args)
		{
			spdlog::log(loc, lvl, fmt::sprintf(fmt, args...).c_str());
		}

		spdlog::level::level_enum level() { return log_level; }

		///设置输出级别
		void set_level(spdlog::level::level_enum lvl)
		{
			log_level = lvl;
			spdlog::set_level(lvl);
		}

		///设置刷新达到指定级别时自动刷新缓冲区
		void set_flush_on(spdlog::level::level_enum lvl) { spdlog::flush_on(lvl); }

	private:
		///初始化日志
		bool init(const std::string& log_path = LOG_PATH_);

	private:
		logger() { init(); }

		~logger() = default;
		logger(const logger&) = delete;
		void operator=(const logger&) = delete;

	private:
		std::atomic_bool is_inited = { false };
		spdlog::level::level_enum log_level = spdlog::level::trace;
		std::stringstream m_ss;
	};

	inline bool logger::init(const std::string& log_path)
	{
		namespace fs = std::experimental::filesystem;
		if (is_inited) return true;
		try {
			namespace fs = std::experimental::filesystem;
			fs::path log_file_path(log_path);
			fs::path log_filename = log_file_path.filename();

			spdlog::filename_t basename, ext;
			std::tie(basename, ext) = spdlog::details::file_helper::split_by_extension(log_filename.string());

			// initialize spdlog
			constexpr std::size_t log_buffer_size = 32 * 1024;// 32kb

			//spdlog::init_thread_pool(log_buffer_size, std::thread::hardware_concurrency());
			spdlog::init_thread_pool(log_buffer_size, 1);
			std::vector<spdlog::sink_ptr> sinks;

			// constexpr std::size_t max_file_size = 50 * 1024 * 1024; // 50mb
			//auto rotatingSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_file_path, 20 * 1024, 10);

			auto rotatingSink = std::make_shared<klog::custom_rotating_file_sink>(log_path, SINGLE_FILE_MAX_SIZE,
				MAX_STORAGE_DAYS);
			sinks.push_back(rotatingSink);

			//auto daily_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(log_path.string(), 23, 59); //日志滚动更新时间：每天23:59更新
			//sinks.push_back(daily_sink);

			// auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.string(), true);
			// sinks.push_back(file_sink);

#define xxx(sink_)	sink_->set_color(spdlog::level::trace, "\033[36m"); \
					sink_->set_color(spdlog::level::debug, "\033[1;34m"); \
					sink_->set_color(spdlog::level::info, "\033[1;32m"); \
					sink_->set_color(spdlog::level::warn, "\033[1;33m"); \
					sink_->set_color(spdlog::level::err, "\033[1;31m"); \
					sink_->set_color(spdlog::level::critical, "\033[1;35m");

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
#undef xxx
			spdlog::set_default_logger(std::make_shared<spdlog::logger>(basename, sinks.begin(), sinks.end()));

			auto formatter = std::make_unique<spdlog::pattern_formatter>();

			formatter->add_flag<custom_level_formatter_flag>('*').
				set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [%*] |%t| [%s:%# (%!)]: %v%$");

			spdlog::set_formatter(std::move(formatter));

			spdlog::flush_every(std::chrono::seconds(5));
			spdlog::flush_on(spdlog::level::warn);
			spdlog::set_level(log_level);
		}
		catch (std::exception_ptr e) {
			assert(false);
			return false;
		}
		is_inited = true;
		return true;
	}


	inline custom_rotating_file_sink::custom_rotating_file_sink(spdlog::filename_t log_path,
		std::size_t max_size,
		std::size_t max_storage_days, bool rotate_on_open,
		const spdlog::file_event_handlers& event_handlers)
		: m_log_path(log_path)
		, m_max_size(max_size)
		, m_max_storage_days(max_storage_days)
		, m_file_helper{ event_handlers }
	{
		if (max_size == 0) {
			spdlog::throw_spdlog_ex("rotating sink constructor: max_size arg cannot be zero");
		}

		if (max_storage_days > 365 * 2) {
			spdlog::throw_spdlog_ex("rotating sink constructor: max_storage_days arg cannot exceed 2 years");
		}

		m_log_parent_path = m_log_path.parent_path();
		m_log_filename = m_log_path.filename();

		spdlog::filename_t basename, ext;
		std::tie(basename, ext) = spdlog::details::file_helper::split_by_extension(m_log_filename.string());

		m_log_basename = basename;

		m_file_helper.open(calc_filename());
		m_current_size = m_file_helper.size();// expensive. called only once

		cleanup_file_();

		if (rotate_on_open && m_current_size > 0) {
			rotate_();
			m_current_size = 0;
		}
	}

	inline spdlog::filename_t custom_rotating_file_sink::calc_filename()
	{
		struct tm tm;
		time_t now = time(0);
		localtime_r(&now, &tm);

		return m_log_parent_path.empty()
			? spdlog::fmt_lib::format("{:%Y-%m-%d}/{}_{:%Y-%m-%d-%H:%M:%S}.log", tm, m_log_basename.string(), tm)
			: spdlog::fmt_lib::format("{}/{:%Y-%m-%d}/{}_{:%Y-%m-%d-%H:%M:%S}.log",
				m_log_parent_path.string(),
				tm,
				m_log_basename.string(),
				tm);/// logs/yyyy-mm-dd/test_yyyy-mm-dd-h-m-s.log
	}

	inline spdlog::filename_t custom_rotating_file_sink::filename()
	{
		std::lock_guard<std::mutex> lock(base_sink<std::mutex>::mutex_);
		return m_file_helper.filename();
	}

	inline void custom_rotating_file_sink::sink_it_(const spdlog::details::log_msg& msg)
	{
		spdlog::memory_buf_t formatted;
		base_sink<std::mutex>::formatter_->format(msg, formatted);
		auto new_size = m_current_size + formatted.size();

		if (new_size > m_max_size) {
			m_file_helper.flush();
			if (m_file_helper.size() > 0) {
				rotate_();
				new_size = formatted.size();
			}
		}
		m_file_helper.write(formatted);
		m_current_size = new_size;
	}

	inline void custom_rotating_file_sink::rotate_()
	{
		m_file_helper.close();

		cleanup_file_();

		spdlog::filename_t filename = calc_filename();

		m_file_helper.open(filename);
	}

	inline void custom_rotating_file_sink::cleanup_file_()
	{
		namespace fs = std::experimental::filesystem;

		const std::regex folder_regex(R"(\d{4}-\d{2}-\d{2})");
		//const std::chrono::hours max_age();

		for (auto& p : fs::directory_iterator(m_log_parent_path)) {
			if (fs::is_directory(p)) {
				const std::string folder_name = p.path().filename().string();
				if (std::regex_match(folder_name, folder_regex)) {
					const int year = std::stoi(folder_name.substr(0, 4));
					const int mon = std::stoi(folder_name.substr(5, 7));
					const int day = std::stoi(folder_name.substr(8, 10));

					std::tm date1_tm{ 0, 0, 0, day, mon - 1, year - 1900 };
					const std::time_t date_tt = std::mktime(&date1_tm);

					const std::chrono::system_clock::time_point time = std::chrono::system_clock::from_time_t(date_tt);

					const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

					const std::chrono::duration<double> duration = now - time;

					const double days = duration.count() / (24 * 60 * 60);// 将时间差转换为天数

					if (days > m_max_storage_days) {
						fs::remove_all(p);
						std::cout << "Clean up log files older than" << m_max_storage_days << " days" << std::endl;
					}
				}
			}
		}
	}


	class custom_level_formatter_flag : public spdlog::custom_flag_formatter
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
			return spdlog::details::make_unique<custom_level_formatter_flag>();
		}
	};
}// namespace klog


#define LOG_LEVEL_TRACE    0
#define LOG_LEVEL_DEBUG    1
#define LOG_LEVEL_INFO     2
#define LOG_LEVEL_WARN     3
#define LOG_LEVEL_ERROR    4
#define LOG_LEVEL_FATAL    5
#define LOG_LEVEL_CLOSE    6


#if (LOGGER_LEVEL <= LOG_LEVEL_TRACE)
#	define 	 log_trace(fmt,...) 		klog::logger::get().fmt_printf({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::trace, fmt, ##__VA_ARGS__)
#	define	 LOG_TRACE(fmt, ...) 		spdlog::log({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::trace, fmt, ##__VA_ARGS__)
#	define 	 logtrace(fmt,...) 		klog::logger::get().printf({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::trace, fmt, ##__VA_ARGS__)
#	define	 LOGTRACE() 			klog::logger::log_stream({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::trace)
#else
#	define	 log_trace(fmt,...)
#	define 	 LOG_TRACE(fmt, ...)
#	define 	 logtrace(fmt,...)
#	define	 LOGTRACE() klog::logger_none::get()
#endif

#if (LOGGER_LEVEL <= LOG_LEVEL_DEBUG)
#	define 	 log_debug(fmt,...) 		klog::logger::get().fmt_printf({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::debug, fmt, ##__VA_ARGS__)
#	define	 LOG_DEBUG(fmt, ...) 		spdlog::log({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::debug, fmt, ##__VA_ARGS__)
#	define 	 logdebug(fmt,...) 		klog::logger::get().printf({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::debug, fmt, ##__VA_ARGS__)
#	define	 LOGDEBUG() 			klog::logger::log_stream({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::debug)
#else
#	define	 log_debug(fmt, ...)
#	define 	 LOG_DEBUG(fmt,...)
#	define 	 logdebug(fmt,...)
#	define	 LOGDEBUG() klog::logger_none::get()
#endif

#if (LOGGER_LEVEL <= LOG_LEVEL_INFO)
#	define 	 log_info(fmt,...) 		klog::logger::get().fmt_printf({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::info, fmt, ##__VA_ARGS__)
#	define	 LOG_INFO(fmt, ...) 		spdlog::log({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::info, fmt, ##__VA_ARGS__)
#	define 	 loginfo(fmt,...) 		klog::logger::get().printf({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::info, fmt, ##__VA_ARGS__)
#	define	 LOGINFO() 			klog::logger::log_stream({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::info)
#else
#	define	 log_info(fmt, ...)
#	define 	 LOG_INFO(fmt,...)
#	define 	 loginfo(fmt,...)
#	define	 LOGINFO() klog::logger_none::get()
#endif

#if (LOGGER_LEVEL <= LOG_LEVEL_WARN)
#	define 	 log_warn(fmt,...) 		klog::logger::get().fmt_printf({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::warn, fmt, ##__VA_ARGS__)
#	define	 LOG_WARN(fmt, ...) 		spdlog::log({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::warn, fmt, ##__VA_ARGS__)
#	define 	 logwarn(fmt,...) 		klog::logger::get().printf({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::warn, fmt, ##__VA_ARGS__)
#	define	 LOGWARN() 			klog::logger::log_stream({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::warn)
#else
#	define	 log_warn(fmt, ...)
#	define 	 LOG_WARN(fmt,...)
#	define 	 logwarn(fmt,...)
#	define	 LOGWARN() klog::logger_none::get()
#endif

#if (LOGGER_LEVEL <= LOG_LEVEL_ERROR)
#	define 	 log_error(fmt,...) 		klog::logger::get().fmt_printf({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::err, fmt, ##__VA_ARGS__)
#	define	 LOG_ERROR(fmt, ...) 		spdlog::log({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::err, fmt, ##__VA_ARGS__)
#	define 	 logerror(fmt,...) 		klog::logger::get().printf({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::err, fmt, ##__VA_ARGS__)
#	define	 LOGERROR() 			klog::logger::log_stream({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::err)
#else
#	define	 log_error(fmt, ...)
#	define 	 LOG_ERROR(fmt,...)
#	define 	 logerror(fmt,...)
#	define	 LOGERROR() klog::logger_none::get()
#endif

#if (LOGGER_LEVEL <= LOG_LEVEL_FATAL)
#	define 	 log_fatal(fmt,...) 		klog::logger::get().fmt_printf({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::critical, fmt, ##__VA_ARGS__)
#	define	 LOG_FATAL(fmt, ...) 		spdlog::log({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::critical, fmt, ##__VA_ARGS__)
#	define 	 logfatal(fmt,...) 		klog::logger::get().printf({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::critical, fmt, ##__VA_ARGS__)
#	define	 LOGFATAL() 			klog::logger::log_stream({__FILE__, __LINE__, __FUNCTION__}, spdlog::level::critical)
#else
#	define	 log_fatal(fmt, ...)
#	define 	 LOG_FATAL(fmt,...)
#	define 	 logfatal(fmt,...)
#	define	 LOGFATAL() klog::logger_none::get()
#endif
