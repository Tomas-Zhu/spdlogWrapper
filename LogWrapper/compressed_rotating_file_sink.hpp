#pragma once

#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/file_helper.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/details/synchronous_factory.h>

#include <spdlog/details/os.h>

#include <chrono>
#include <mutex>
#include <string>

namespace spdlog {
namespace sinks {

//
// Rotating file sink based on size
//
template <typename Mutex>
class compressed_rotating_file_sink final : public base_sink<Mutex> {
 public:
  compressed_rotating_file_sink(filename_t base_filename, std::size_t max_size, std::size_t max_files, std::size_t max_compressed_files, bool rotate_on_open = false,
        const file_event_handlers &event_handlers = {});
  static filename_t calc_filename(const filename_t& filename, std::size_t index);
  filename_t filename();

 protected:
  void sink_it_(const details::log_msg& msg) override;
  void flush_() override;

 private:
  // Rotate files:
  // log.txt -> log.1.txt
  // log.1.txt -> log.2.txt
  // log.2.txt -> log.3.txt
  // log.3.txt -> delete
  void rotate_();
#if defined(_WIN32) && defined(SPDLOG_WCHAR_FILENAMES)
  void compressW_();
#else
  void compressA_();
#endif

  // delete the target if exists, and rename the src file  to target
  // return true on success, false otherwise.
  bool rename_file_(const filename_t& src_filename, const filename_t& target_filename);

  filename_t base_filename_;
  std::size_t max_size_;
  std::size_t max_files_;
  std::size_t max_compressed_files_;
  std::size_t current_size_;
  details::file_helper file_helper_;
  filename_t dir_;
  filename_t basename_;
  filename_t file_ext_;
};

using compressed_rotating_file_sink_mt = compressed_rotating_file_sink<std::mutex>;
using compressed_rotating_file_sink_st = compressed_rotating_file_sink<details::null_mutex>;

template <typename Mutex>
SPDLOG_INLINE compressed_rotating_file_sink<Mutex>::compressed_rotating_file_sink(filename_t base_filename, std::size_t max_size, std::size_t max_files, std::size_t max_compressed_files,
    bool rotate_on_open, const file_event_handlers &event_handlers)
    : base_filename_(std::move(base_filename))
    , max_size_(max_size)
    , max_files_(max_files)
    , max_compressed_files_(max_compressed_files)
    , file_helper_(event_handlers)
{
    if (max_size == 0)
    {
        throw_spdlog_ex("rotating sink constructor: max_size arg cannot be zero");
    }

    if (max_files > 200000)
    {
        throw_spdlog_ex("rotating sink constructor: max_files arg cannot exceed 200000");
    }
    file_helper_.open(calc_filename(base_filename_, 0));
    current_size_ = file_helper_.size();  // expensive. called only once

	filename_t path;
	std::tie(path, file_ext_) = details::file_helper::split_by_extension(base_filename_);
	dir_ = spdlog::details::os::dir_name(path);
	std::size_t dir_index = path.rfind('/');
	if (dir_index != filename_t::npos)
	{
		basename_ = path.substr(dir_index + 1);
	}
	dir_index = path.rfind('\\');
	if (dir_index != filename_t::npos)
	{
		basename_ = path.substr(dir_index + 1);
	}

	if (rotate_on_open && current_size_ > 0)
	{
		rotate_();
#if defined(_WIN32) && defined(SPDLOG_WCHAR_FILENAMES)
		compressW_();
#else
		compressA_();
#endif
		current_size_ = 0;
	}
}

// calc filename according to index and file extension if exists.
// e.g. calc_filename("logs/mylog.txt, 3) => "logs/mylog.3.txt".
template <typename Mutex>
SPDLOG_INLINE filename_t compressed_rotating_file_sink<Mutex>::calc_filename(const filename_t& filename, std::size_t index)
{
    if (index == 0u)
    {
        return filename;
    }

    filename_t basename, ext;
    std::tie(basename, ext) = details::file_helper::split_by_extension(filename);
    return fmt_lib::format(SPDLOG_FILENAME_T("{}.{}{}"), basename, index, ext);
}

template<typename Mutex>
SPDLOG_INLINE filename_t compressed_rotating_file_sink<Mutex>::filename()
{
	std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
	return file_helper_.filename();
}

template <typename Mutex>
SPDLOG_INLINE void compressed_rotating_file_sink<Mutex>::sink_it_(const details::log_msg& msg) {
    memory_buf_t formatted;
    base_sink<Mutex>::formatter_->format(msg, formatted);
    auto new_size = current_size_ + formatted.size();

    // rotate if the new estimated file size exceeds max size.
    // rotate only if the real size > 0 to better deal with full disk (see issue #2261).
    // we only check the real size when new_size > max_size_ because it is relatively expensive.
    if (new_size > max_size_)
    {
        file_helper_.flush();
        if (file_helper_.size() > 0)
        {
            rotate_();
#if defined(_WIN32) && defined(SPDLOG_WCHAR_FILENAMES)
			compressW_();
#else
			compressA_();
#endif
            new_size = formatted.size();
        }
    }
    file_helper_.write(formatted);
    current_size_ = new_size;
}

template <typename Mutex>
SPDLOG_INLINE void compressed_rotating_file_sink<Mutex>::flush_() 
{
    file_helper_.flush();
}

// Rotate files:
// log.txt -> log.1.txt
// log.1.txt -> log.2.txt
// log.2.txt -> log.3.txt
// log.3.txt -> delete
template <typename Mutex>
SPDLOG_INLINE void compressed_rotating_file_sink<Mutex>::rotate_()
{
    using details::os::filename_to_str;
    using details::os::path_exists;

    file_helper_.close();
    for (auto i = max_files_; i > 0; --i)
    {
        filename_t src = calc_filename(base_filename_, i - 1);
        if (!path_exists(src))
        {
            continue;
        }
        filename_t target = calc_filename(base_filename_, i);

        if (!rename_file_(src, target))
        {
            // if failed try again after a small delay.
            // this is a workaround to a windows issue, where very high rotation
            // rates can cause the rename to fail with permission denied (because of antivirus?).
            details::os::sleep_for_millis(100);
            if (!rename_file_(src, target))
            {
                file_helper_.reopen(true); // truncate the log file anyway to prevent it to grow beyond its limit!
                current_size_ = 0;
                throw_spdlog_ex("rotating_file_sink: failed renaming " + filename_to_str(src) + " to " + filename_to_str(target), errno);
            }
        }
    }
    file_helper_.reopen(true);
}

// delete the target if exists, and rename the src file  to target
// return true on success, false otherwise.
template<typename Mutex>
SPDLOG_INLINE bool compressed_rotating_file_sink<Mutex>::rename_file_(const filename_t &src_filename, const filename_t &target_filename)
{
	// try to delete the target file in case it already exists.
	(void)details::os::remove(target_filename);
	return details::os::rename(src_filename, target_filename) == 0;
}

#if defined(_WIN32) && defined(SPDLOG_WCHAR_FILENAMES)
template <typename Mutex>
SPDLOG_INLINE void compressed_rotating_file_sink<Mutex>::compressW_()
{
	if (max_compressed_files_ == 0)
	{
		return;
	}

	const std::wstring compress_ext = L"zip";

	filename_t basename;
	if (dir_.empty())
	{
		basename = L"./" + basename_;
	}
	else
	{
		basename = dir_ + L"/" + basename_;
	}

	std::size_t max_value = max_compressed_files_;
	std::size_t min_value = 1;

	std::wstring regex_newest_file = basename + L"." + std::to_wstring(max_value) + file_ext_ + L"." + compress_ext;
	std::wstring regex_oldest_file = basename + L"." + std::to_wstring(min_value) + file_ext_ + L"." + compress_ext;

	std::wstring compress_target_file;

	try
	{
		if (details::os::path_exists(dir_))
		{
			if (details::os::path_exists(regex_newest_file))
			{
				for (std::size_t index = min_value; index < max_value; ++index)
				{
					std::wstring target_file = basename + L"." + std::to_wstring(index) + file_ext_ + L"." + compress_ext;
					std::wstring src_file = basename + L"." + std::to_wstring(index + 1) + file_ext_ + L"." + compress_ext;
					rename_file_(src_file, target_file);
				}

				compress_target_file = regex_newest_file;
			}
			else
			{
				for (std::size_t index = min_value; index <= max_value; ++index)
				{
					std::wstring target = basename + L"." + std::to_wstring(index) + file_ext_ + L"." + compress_ext;
					if (!details::os::path_exists(target))
					{
						compress_target_file = target;
						break;
					}
				}
			}
		}
	}
	catch (const std::exception& e)
	{
		SPDLOG_THROW(spdlog_ex(e.what()));
	}

	filename_t file_to_compress;
	for (std::size_t index = min_value; index <= max_value; ++index)
	{
		std::wstring src = basename + L"." + std::to_wstring(index) + file_ext_;
		if (details::os::path_exists(src))
		{
			file_to_compress = src;
			break;
		}
	}
	if (details::os::path_exists(file_to_compress))
	{
		memory_buf_t buf;
		details::os::wstr_to_utf8buf(wstring_view_t(file_to_compress.data(), file_to_compress.size()), buf);
		std::string file_to_compressA(buf.data(), buf.size());
		buf.clear();
		details::os::wstr_to_utf8buf(wstring_view_t(compress_target_file.data(), compress_target_file.size()), buf);
		std::string compress_target_fileA(buf.data(), buf.size());
		buf.clear();

		// compress file and remove file after success
		if (false)
		{
			details::os::remove(file_to_compress);
		}
	}
}
#else
template <typename Mutex>
SPDLOG_INLINE void compressed_rotating_file_sink<Mutex>::compressA_()
{
	if (max_compressed_files_ == 0)
	{
		return;
	}
	
	const std::string compress_ext = "zip";

	filename_t basename;
	if (dir_.empty())
	{
		basename = "./" + basename_;
	}
	else
	{
		basename = dir_ + "/" + basename_;
	}
	
	std::size_t max_value = max_compressed_files_;
	std::size_t min_value = 1;

	std::string regex_newest_file = basename + "." + std::to_string(max_value) + file_ext_ + "." + compress_ext;
	std::string regex_oldest_file = basename + "." + std::to_string(min_value) + file_ext_ + "." + compress_ext;

	std::string compress_target_file;

	try
	{
		if (details::os::path_exists(dir_))
		{
			if (details::os::path_exists(regex_newest_file))
			{
				for (std::size_t index = min_value; index < max_value; ++index)
				{
					std::string target_file = basename + "." + std::to_string(index) + file_ext_ + "." + compress_ext;
					std::string src_file = basename + "." + std::to_string(index + 1) + file_ext_ + "." + compress_ext;
					rename_file_(src_file, target_file);
				}

				compress_target_file = regex_newest_file;
			}
			else
			{
				for (std::size_t index = min_value; index <= max_value; ++index)
				{
					std::string target = basename + "." + std::to_string(index) + file_ext_ + "." + compress_ext;
					if (!details::os::path_exists(target))
					{
						compress_target_file = target;
						break;
					}
				}
			}
		}
	}
	catch (const std::exception& e)
	{
		SPDLOG_THROW(spdlog_ex(e.what()));
	}

	filename_t file_to_compress;
	for (std::size_t index = min_value; index <= max_value; ++index)
	{
		std::string src = basename + L"." + std::to_string(index) + file_ext_;
		if (details::os::path_exists(src))
		{
			file_to_compress = src;
			break;
		}
	}
	if (details::os::path_exists(file_to_compress))
	{
		// compress file and remove file after success
		if (false)
		{
			details::os::remove(file_to_compress);
		}
	}
}
#endif
}  // namespace sinks
}  // namespace spdlog