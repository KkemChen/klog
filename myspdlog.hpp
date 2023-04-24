#pragma once

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


/// spdlog wrap class
namespace klog
{
constexpr const char* log_path_ = "logs/test.log";  //日志存储路径
class custom_level_formatter_flag;

class my_color_formatter : public spdlog::formatter {
public:
    std::string format(const spdlog::details::log_msg& msg){
        // 根据日志级别选择颜色
        const char* color_code = "";
        switch (msg.level) {
            case spdlog::level::trace:
                color_code = "\033[1;37m";  // 白色
                break;
            case spdlog::level::debug:
                color_code = "\033[1;36m";  // 青色
                break;
            case spdlog::level::info:
                color_code = "\033[1;32m";  // 绿色
                break;
            case spdlog::level::warn:
                color_code = "\033[1;33m";  // 黄色
                break;
            case spdlog::level::err:
                color_code = "\033[1;31m";  // 红色
                break;
            case spdlog::level::critical:
                color_code = "\033[1;35m";  // 紫色
                break;
            case spdlog::level::off:
                break;
        }

        // 构造带有颜色的日志消息字符串
        std::ostringstream oss;
        if (color_code[0] != '\0') {
            // 如果颜色代码不为空，则添加颜色代码和重置代码
            oss << color_code << "["  << "] " << msg.payload.data() << "\033[0m";
        } else {
            // 否则，只添加日志级别和消息文本
            oss << "[" << "] " << msg.payload.data();
        }

        return oss.str();
    }
};

/**
 * \brief 自定义sink
 *		  1.按文件大小分片存储
 *		  2.文件命名：basename+datetime+.log
 *		  3.每次rotate时自动检查当前目录日志文件是否超过设定的时间，并自动清理
 */
class custom_rotating_file_sink final : public spdlog::sinks::base_sink<std::mutex>
{
public:
	/**
	 * \brief 
	 * \param base_filename 基础日志文件路径
	 * \param max_size 单文件最大容量
	 * \param max_storage_days 最大保存天数
	 * \param rotate_on_open 默认true
	 * \param event_handlers 默认
	 */
	custom_rotating_file_sink(spdlog::filename_t log_path,
	                          std::size_t max_size,
	                          std::size_t max_storage_days,
	                          bool rotate_on_open = true, const spdlog::file_event_handlers& event_handlers = {});

	/**
	 * \brief 
	 * \param filename 拼接文件名
	 * \param index 时间戳
	 * \return 
	 */
	spdlog::filename_t calc_filename();
	spdlog::filename_t filename();

protected:
	/**
	 * \brief 将日志消息写入到输出目标
	 * \param msg 日志消息
	 */
	void sink_it_(const spdlog::details::log_msg& msg) override;

	/**
	 * \brief 刷新日志缓冲区
	 */
	void flush_() override;

private:
	/**
	 * \brief 日志轮转
	 */
	void rotate_();

	/**
	 * \brief 清理过期日志文件
	 */
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
		log_stream(const spdlog::source_loc& _loc, spdlog::level::level_enum _lvl): loc(_loc)
		  , lvl(_lvl) { }

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

	//初始化日志
	bool init(const std::string& log_path = log_path_);

	void shutdown() { spdlog::shutdown(); }

	template <typename... Args>
	void log(const spdlog::source_loc& loc, spdlog::level::level_enum lvl, const char* fmt, const Args&... args)
	{
		spdlog::log(loc, lvl, fmt, args...);
	}

	template <typename... Args>
	void printf(const spdlog::source_loc& loc, spdlog::level::level_enum lvl, const char* fmt, const Args&... args)
	{
		spdlog::log(loc, lvl, fmt::sprintf(fmt, args...).c_str());
	}

	spdlog::level::level_enum level() { return log_level; }

	void set_level(spdlog::level::level_enum lvl)
	{
		log_level = lvl;
		spdlog::set_level(lvl);
	}

	void set_flush_on(spdlog::level::level_enum lvl) { spdlog::flush_on(lvl); }

private:
	logger() = default;
	~logger() = default;

	logger(const logger&) = delete;
	void operator=(const logger&) = delete;

private:
	std::atomic_bool is_inited = {false};
	spdlog::level::level_enum log_level = spdlog::level::trace;
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
		constexpr std::size_t log_buffer_size = 32 * 1024; // 32kb

		//spdlog::init_thread_pool(log_buffer_size, std::thread::hardware_concurrency());
		spdlog::init_thread_pool(log_buffer_size, 1);
		std::vector<spdlog::sink_ptr> sinks;

		// constexpr std::size_t max_file_size = 50 * 1024 * 1024; // 50mb
		//auto rotatingSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_file_path, 20 * 1024, 10);

		auto rotatingSink = std::make_shared<klog::custom_rotating_file_sink>(log_path, 20 * 1024 * 1024, 1);
		sinks.push_back(rotatingSink);

		//auto daily_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(log_path.string(), 23, 59); //日志滚动更新时间：每天23:59更新
		//sinks.push_back(daily_sink);

		// auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.string(), true);
		// sinks.push_back(file_sink);

#if defined(_DEBUG) && defined(WIN32) && !defined(NO_CONSOLE_LOG)
			auto ms_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
			sinks.push_back(ms_sink);
#endif //  _DEBUG

#if !defined(WIN32) && !defined(NO_CONSOLE_LOG)
			auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
			sinks.push_back(console_sink);
#endif
		spdlog::set_default_logger(std::make_shared<spdlog::logger>(basename, sinks.begin(), sinks.end()));

		auto formatter = std::make_unique<spdlog::pattern_formatter>();

		formatter->add_flag<custom_level_formatter_flag>('*').
		           set_pattern("%Y-%m-%d %H:%M:%S  %^[%*]%$  |%t|  [%s:%# (%!)]: %v");
		
		spdlog::set_formatter(std::move(formatter));

		spdlog::flush_every(std::chrono::seconds(5));
		spdlog::flush_on(spdlog::level::warn);
		spdlog::set_level(log_level);
	} catch (std::exception_ptr e) {
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
	, max_storage_days(max_storage_days)
	, m_file_helper{event_handlers}
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
	m_current_size = m_file_helper.size(); // expensive. called only once
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
		                                 tm); /// logs/yyyy-mm-dd/test_yyyy-mm-dd-h-m-s.log
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

inline void custom_rotating_file_sink::flush_()
{
	m_file_helper.flush();
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

				std::tm date1_tm{0, 0, 0, day, mon - 1, year - 1900};
				const std::time_t date_tt = std::mktime(&date1_tm);

				const std::chrono::system_clock::time_point time = std::chrono::system_clock::from_time_t(date_tt);

				const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

				const std::chrono::duration<double> duration = now - time;

				const double days = duration.count() / (24 * 60 * 60); // 将时间差转换为天数

				int ret = 0;

				if (days > max_storage_days) {
					fs::remove_all(p);
					std::cout << "Clean up log files older than" << max_storage_days << " days" << std::endl;
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
