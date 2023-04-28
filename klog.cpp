#include "klog.h"

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

void klog::custom_level_formatter_flag::format(const spdlog::details::log_msg& _log_msg,
                                               const std::tm&,
                                               spdlog::memory_buf_t& dest)
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

std::unique_ptr<spdlog::custom_flag_formatter> klog::custom_level_formatter_flag::clone() const
{
	return spdlog::details::make_unique<custom_level_formatter_flag>();
}

klog::custom_rotating_file_sink::custom_rotating_file_sink(spdlog::filename_t log_path,
                                                           std::size_t max_size,
                                                           std::size_t max_storage_days,
                                                           bool rotate_on_open,
                                                           const spdlog::file_event_handlers& event_handlers)
	: m_log_path(log_path)
	, m_max_size(max_size)
	, m_max_storage_days(max_storage_days)
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

spdlog::filename_t klog::custom_rotating_file_sink::calc_filename()
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

spdlog::filename_t klog::custom_rotating_file_sink::filename()
{
	std::lock_guard<std::mutex> lock(base_sink<std::mutex>::mutex_);
	return m_file_helper.filename();
}

void klog::custom_rotating_file_sink::sink_it_(const spdlog::details::log_msg& msg)
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

void klog::custom_rotating_file_sink::flush_()
{
	m_file_helper.flush();
}

void klog::custom_rotating_file_sink::rotate_()
{
	m_file_helper.close();

	cleanup_file_();

	spdlog::filename_t filename = calc_filename();

	m_file_helper.open(filename);
}

void klog::custom_rotating_file_sink::cleanup_file_()
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

				if (days > m_max_storage_days) {
					fs::remove_all(p);
					std::cout << "Clean up log files older than" << m_max_storage_days << " days" << std::endl;
				}
			}
		}
	}
}

klog::logger::log_stream::log_stream(const spdlog::source_loc& _loc, spdlog::level::level_enum _lvl)
	: loc(_loc)
	, lvl(_lvl) {}

klog::logger::log_stream::~log_stream()
{
	flush();
}

void klog::logger::log_stream::flush()
{
	logger::get().log(loc, lvl, str().c_str());
}

klog::logger& klog::logger::get()
{
	static logger logger;
	return logger;
}

bool klog::logger::init(const std::string& log_path)
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

		auto rotatingSink = std::make_shared<klog::custom_rotating_file_sink>(log_path, SINGLE_FILE_MAX_SIZE, MAX_STORAGE_DAYS);
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
#undef xx
		spdlog::set_default_logger(std::make_shared<spdlog::logger>(basename, sinks.begin(), sinks.end()));

		auto formatter = std::make_unique<spdlog::pattern_formatter>();

		formatter->add_flag<custom_level_formatter_flag>('*').
		           set_pattern("%^[%Y-%m-%d %H:%M:%S] [%*] |%t| [<%!> %s:%#]: %v%$");

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

klog::logger::logger()
{
	init();
}

void klog::logger::shutdown()
{
	spdlog::shutdown();
}

void klog::logger::printf(const spdlog::source_loc& loc, spdlog::level::level_enum lvl, const char* fmt, ...)
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

spdlog::level::level_enum klog::logger::level()
{
	return log_level;
}

void klog::logger::set_level(spdlog::level::level_enum lvl)
{
	log_level = lvl;
	spdlog::set_level(lvl);
}

void klog::logger::set_flush_on(spdlog::level::level_enum lvl)
{
	spdlog::flush_on(lvl);
}

template <typename... Args>
void klog::logger::log(const spdlog::source_loc& loc, spdlog::level::level_enum lvl, const char* fmt,
                       const Args&... args)
{
	spdlog::log(loc, lvl, fmt, args...);
}

template <typename... Args>
void klog::logger::fmt_printf(const spdlog::source_loc& loc, spdlog::level::level_enum lvl, const char* fmt,
                              const Args&... args)
{
	spdlog::log(loc, lvl, fmt::sprintf(fmt, args...).c_str());
}
