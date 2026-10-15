/*
 * MIT License
 *
 * Copyright (c) 2024-present Artur Drogunow <artur.drogunow@zf.com>
 * Copyright (c) 2026 baudforge
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "vblf.h"

#include <utf8.h>

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <new>
#include <string>
#include <system_error>

/** @brief Calendar fields stored in a BLF file header, without time conversion. */
struct vblf_system_time
{
    uint16_t year = 0;
    uint16_t month = 0;
    uint16_t day_of_week = 0;
    uint16_t day = 0;
    uint16_t hour = 0;
    uint16_t minute = 0;
    uint16_t second = 0;
    uint16_t milliseconds = 0;
};

/**
 * @brief Decoded file-header metadata; its C++ layout is not a disk layout.
 * @note Field meanings follow vblf.general.FileStatistics at revision
 * e509ae9c64c834851418e74e9bd89a32efe41d3c. Reserved bytes are not retained.
 */
struct vblf_file_header
{
    uint32_t statistics_size = 0;
    uint32_t api_number = 0;
    vblf_app_id_t application_id = 0;
    vblf_compression_t compression_level = 0;
    uint8_t application_major = 0;
    uint8_t application_minor = 0;
    uint64_t file_size = 0;
    uint64_t uncompressed_file_size = 0;
    uint32_t object_count = 0;
    uint32_t application_build = 0;
    vblf_system_time measurement_start_time;
    vblf_system_time last_object_time;
    uint64_t restore_points_offset = 0;
};

/** @brief Private owner of one binary file stream and its decoded file header. */
struct vblf_handle
{
    std::fstream file;
    vblf_open_mode_t mode = 0;
    vblf_file_header header;
};

namespace
{

/** @brief Decode an unsigned little-endian value without alignment assumptions. */
uint16_t read_u16_le(const uint8_t *data) noexcept
{
    return static_cast<uint16_t>(static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8));
}

/** @brief Decode an unsigned little-endian value without alignment assumptions. */
uint32_t read_u32_le(const uint8_t *data) noexcept
{
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

/** @brief Decode an unsigned little-endian value without alignment assumptions. */
uint64_t read_u64_le(const uint8_t *data) noexcept
{
    return static_cast<uint64_t>(read_u32_le(data)) | (static_cast<uint64_t>(read_u32_le(data + 4)) << 32);
}

/** @brief Preserve the calendar fields as recorded, including unset fields. */
vblf_system_time read_system_time(const uint8_t *data) noexcept
{
    return {
        read_u16_le(data),
        read_u16_le(data + 2),
        read_u16_le(data + 4),
        read_u16_le(data + 6),
        read_u16_le(data + 8),
        read_u16_le(data + 10),
        read_u16_le(data + 12),
        read_u16_le(data + 14)};
}

/**
 * @brief Decode the fixed file-header fields and position the stream at the object area.
 * @details The first 72 bytes contain the required fields. A complete 8-byte
 * restore-point offset is decoded when present. All remaining header bytes are
 * skipped, including the reserved tail of the usual 144-byte file header.
 * @note Recorded file sizes, counts and timestamps are metadata. This operation
 * validates the header boundary, not the objects that follow it.
 */
vblf_result_t read_file_header(std::fstream &file, vblf_file_header &out_header)
{
    constexpr uint32_t prefix_size = 8;
    constexpr uint32_t minimum_size = 72;
    constexpr uint32_t known_size = 80;
    std::array<uint8_t, known_size> data{};

    if (!file.read(reinterpret_cast<char *>(data.data()), prefix_size))
    {
        return file.bad() ? VBLF_ERROR_IO : VBLF_ERROR_TRUNCATED;
    }
    if (std::memcmp(data.data(), VBLF_FILE_SIGNATURE, sizeof(VBLF_FILE_SIGNATURE) - 1) != 0)
    {
        return VBLF_ERROR_INVALID_SIGNATURE;
    }

    const uint32_t header_size = read_u32_le(data.data() + 4);
    if (header_size < minimum_size)
    {
        return VBLF_ERROR_INVALID_SIZE;
    }

    file.seekg(0, std::ios::end);
    const std::streamoff file_size = file.tellg();
    if (!file || file_size < 0)
    {
        return VBLF_ERROR_IO;
    }
    if (static_cast<uint64_t>(file_size) < header_size)
    {
        return VBLF_ERROR_TRUNCATED;
    }

    file.seekg(prefix_size, std::ios::beg);
    if (!file)
    {
        return VBLF_ERROR_IO;
    }
    const uint32_t read_size = header_size >= known_size ? known_size : minimum_size;
    if (!file.read(reinterpret_cast<char *>(data.data() + prefix_size), read_size - prefix_size))
    {
        return file.bad() ? VBLF_ERROR_IO : VBLF_ERROR_TRUNCATED;
    }

    vblf_file_header header;
    header.statistics_size = header_size;
    header.api_number = read_u32_le(data.data() + 8);
    header.application_id = data[12];
    header.compression_level = data[13];
    header.application_major = data[14];
    header.application_minor = data[15];
    header.file_size = read_u64_le(data.data() + 16);
    header.uncompressed_file_size = read_u64_le(data.data() + 24);
    header.object_count = read_u32_le(data.data() + 32);
    header.application_build = read_u32_le(data.data() + 36);
    header.measurement_start_time = read_system_time(data.data() + 40);
    header.last_object_time = read_system_time(data.data() + 56);
    if (read_size == known_size)
    {
        header.restore_points_offset = read_u64_le(data.data() + 72);
    }

    file.seekg(static_cast<std::streamoff>(header_size), std::ios::beg);
    if (!file)
    {
        return VBLF_ERROR_IO;
    }
    out_header = header;
    return VBLF_OK;
}

} // namespace

/**
 * @brief Open a file, validate its BLF header in read mode and transfer handle ownership.
 * @note Write mode currently manages file resources only.
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

        if (mode == VBLF_OPEN_READ)
        {
            const vblf_result_t result = read_file_header(handle->file, handle->header);
            if (result != VBLF_OK)
            {
                return result;
            }
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
