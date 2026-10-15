#include "vblf.h"

#include <utf8.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <new>
#include <string>
#include <system_error>

/** @brief Private owner of one binary file stream. */
struct vblf_handle
{
    std::fstream file;
    vblf_open_mode_t mode = 0;
};

namespace
{

} // namespace

/**
 * @brief Open a binary file and transfer handle ownership on success.
 * @note This migration step manages file resources only.
 */
VBLF_API vblf_create_file(const char *path_utf8, vblf_open_mode_t mode, vblf_handle_t **out_handle)
{
    if (out_handle == nullptr)
    {
        return VBLF_ERROR_INVALID_ARGUMENT;
    }

    *out_handle = nullptr;
    if (path_utf8 == nullptr || *path_utf8 == '\0' || !utf8::is_valid(path_utf8))
    {
        return VBLF_ERROR_INVALID_ARGUMENT;
    }
    if (mode != VBLF_OPEN_READ && mode != VBLF_OPEN_WRITE)
    {
        return VBLF_ERROR_INVALID_MODE;
    }

    try
    {
#if defined(__cpp_char8_t)
        const std::filesystem::path path(
            std::u8string(path_utf8, path_utf8 + std::char_traits<char>::length(path_utf8)));
#else
        const std::filesystem::path path = std::filesystem::u8path(path_utf8);
#endif

        if (mode == VBLF_OPEN_READ && !std::filesystem::is_regular_file(path))
        {
            return VBLF_ERROR_IO;
        }

        auto handle = std::make_unique<vblf_handle>();
        handle->mode = mode;
        const auto access = mode == VBLF_OPEN_READ ? std::ios::in : std::ios::out | std::ios::trunc;
        handle->file.open(path, std::ios::binary | access);
        if (!handle->file.is_open() || !handle->file.good())
        {
            return VBLF_ERROR_IO;
        }

        *out_handle = handle.release();
        return VBLF_OK;
    }
    catch (const std::bad_alloc &)
    {
        return VBLF_ERROR_OUT_OF_MEMORY;
    }
    catch (const std::system_error &)
    {
        return VBLF_ERROR_IO;
    }
    catch (...)
    {
        return VBLF_ERROR_INTERNAL;
    }
}

/** @brief Flush pending output, close the file and release the handle. */
VBLF_API vblf_close_handle(vblf_handle_t *handle)
{
    if (handle == nullptr)
    {
        return VBLF_OK;
    }

    const std::unique_ptr<vblf_handle> owner(handle);
    try
    {
        const bool had_io_error = handle->file.bad();
        // A clean end-of-file must not turn a successful close into an error.
        handle->file.clear();

        if (handle->mode == VBLF_OPEN_WRITE)
        {
            handle->file.flush();
        }
        const bool flush_failed = handle->file.fail();

        handle->file.clear();
        handle->file.close();
        return had_io_error || flush_failed || handle->file.fail() ? VBLF_ERROR_IO : VBLF_OK;
    }
    catch (const std::bad_alloc &)
    {
        return VBLF_ERROR_OUT_OF_MEMORY;
    }
    catch (const std::system_error &)
    {
        return VBLF_ERROR_IO;
    }
    catch (...)
    {
        return VBLF_ERROR_INTERNAL;
    }
}
