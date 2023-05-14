﻿#include "Log.h"

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


#define xxx(sink_)	sink_->set_color(\
					static_cast<spdlog::level::level_enum>(LOG_LEVEL::TRACE), "\033[36m");\
					sink_->set_color(\
					static_cast<spdlog::level::level_enum>(LOG_LEVEL::DEBUG), "\033[1;34m");\
					sink_->set_color(\
					static_cast<spdlog::level::level_enum>(LOG_LEVEL::INFO), "\033[1;32m");\
					sink_->set_color(\
					static_cast<spdlog::level::level_enum>(LOG_LEVEL::WARN), "\033[1;33m");\
					sink_->set_color(\
					static_cast<spdlog::level::level_enum>(LOG_LEVEL::ERROR), "\033[1;31m");\
					sink_->set_color(\
					static_cast<spdlog::level::level_enum>(LOG_LEVEL::FATAL), "\033[1;35m");


void kkem::CustomLevelFormatterFlag::format(const spdlog::details::log_msg& _log_msg, const std::tm&,
	spdlog::memory_buf_t& dest) {
	switch (_log_msg.level) {
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

void kkem::Logger::printf(const spdlog::source_loc& loc, kkem::LOG_LEVEL lvl, const char* fmt, ...) {
	auto fun = [](void* self, const char* fmt, va_list al) {
		auto thiz = static_cast<Logger*>(self);
		char* buf = nullptr;
		int len = vasprintf(&buf, fmt, al);
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

void kkem::Logger::printf_(const std::string& logger, const spdlog::source_loc& loc, kkem::LOG_LEVEL lvl,
	const char* fmt, ...) {
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

void kkem::Logger::set_level(kkem::LOG_LEVEL lvl) {
	_logLevel = static_cast<spdlog::level::level_enum>(lvl);
	spdlog::set_level(_logLevel);
}

void kkem::Logger::set_level_(const std::string& logger, kkem::LOG_LEVEL lvl) {
	auto it = _map_exLog.find(logger);
	if (it != _map_exLog.end()) {
		it->second->set_level(static_cast<spdlog::level::level_enum>(lvl));
	}
}

bool kkem::Logger::init(const std::string& logPath, const int mode, const std::size_t threadCount,
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
	}
	catch (std::exception_ptr e) {
		assert(false);
		return false;
	}
	_isInited = true;
	return true;
}

bool kkem::Logger::add_ExLog(const std::string& logPath, int mode)
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

		std::shared_ptr < spdlog::logger > exLog;
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
	}
	catch (std::exception_ptr e) {
		assert(false);
		return false;
	}
	return true;
}

kkem::CustomRotatingFileSink::CustomRotatingFileSink(spdlog::filename_t log_path,
                                                            std::size_t max_size,
                                                            std::size_t max_storage_days, bool rotate_on_open,
                                                            const spdlog::file_event_handlers& event_handlers)
	: _log_path(log_path)
	, _max_size(max_size)
	, _max_storage_days(max_storage_days)
	, _file_helper{ event_handlers }
{
	if (max_size == 0) {
		spdlog::throw_spdlog_ex("rotating sink constructor: max_size arg cannot be zero");
	}

	if (max_storage_days > 365 * 2) {
		spdlog::throw_spdlog_ex("rotating sink constructor: max_storage_days arg cannot exceed 2 years");
	}

	_log_parent_path = _log_path.parent_path();
	_log_filename = _log_path.filename();

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

spdlog::filename_t kkem::CustomRotatingFileSink::calc_filename()
{
	auto now = std::chrono::system_clock::now();
	std::time_t time = std::chrono::system_clock::to_time_t(now);
	std::tm tm = *std::localtime(&time);

	return _log_parent_path.empty()
		? spdlog::fmt_lib::format("{:%Y-%m-%d}/{}_{:%Y-%m-%d-%H:%M:%S}.log", tm, _log_basename.string(), tm)
		: spdlog::fmt_lib::format("{}/{:%Y-%m-%d}/{}_{:%Y-%m-%d-%H:%M:%S}.log",
			_log_parent_path.string(),
			tm,
			_log_basename.string(),
			tm);/// logs/yyyy-mm-dd/test_yyyy-mm-dd-h-m-s.log
}

spdlog::filename_t kkem::CustomRotatingFileSink::filename()
{
	std::lock_guard<std::mutex> lock(base_sink<std::mutex>::mutex_);
	return _file_helper.filename();
}

void kkem::CustomRotatingFileSink::sink_it_(const spdlog::details::log_msg& msg)
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

void kkem::CustomRotatingFileSink::rotate_()
{
	_file_helper.close();

	cleanup_file_();

	spdlog::filename_t filename = calc_filename();

	_file_helper.open(filename);
}

void kkem::CustomRotatingFileSink::cleanup_file_()
{
	namespace fs = std::experimental::filesystem;

	const std::regex folder_regex(R"(\d{4}-\d{2}-\d{2})");
	//const std::chrono::hours max_age();

	for (auto& p : fs::directory_iterator(_log_parent_path)) {
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

				if (days > _max_storage_days) {
					fs::remove_all(p);
					std::cout << "Clean up log files older than" << _max_storage_days << " days" << std::endl;
				}
			}
		}
	}
}

#undef xxx