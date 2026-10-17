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
#include <zlib.h>

#include <algorithm>
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

/** @brief Bounded input buffer, decompressor and pending logical object header. */
struct vblf_reader
{
    std::array<uint8_t, 16 * 1024> input{};
    z_stream inflater{};
    bool inflater_initialized = false;
    bool source_active = false;
    uint16_t compression_method = 0;
    uint32_t input_remaining = 0;
    uint32_t output_remaining = 0;
    bool header_pending = false;
    vblf_object_header_base_t pending_header{};
    vblf_result_t terminal_result = VBLF_OK;

    vblf_reader() = default;
    vblf_reader(const vblf_reader &) = delete;
    vblf_reader &operator=(const vblf_reader &) = delete;

    ~vblf_reader()
    {
        if (inflater_initialized)
        {
            inflateEnd(&inflater);
        }
    }
};

/** @brief Private owner of one binary file stream and its decoded file header. */
struct vblf_handle
{
    std::fstream file;
    vblf_open_mode_t mode = 0;
    vblf_file_header header;
    std::unique_ptr<vblf_reader> reader;
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

/** @brief Read a fixed number of bytes, distinguishing clean EOF from truncation. */
vblf_result_t read_file_bytes(std::fstream &file, uint8_t *data, uint32_t count)
{
    if (file.read(reinterpret_cast<char *>(data), count))
    {
        return VBLF_OK;
    }
    if (file.bad())
    {
        return VBLF_ERROR_IO;
    }
    return file.gcount() == 0 && file.eof() ? VBLF_EOF : VBLF_ERROR_TRUNCATED;
}

/** @brief Decode and validate the metadata shared by all serialized objects. */
vblf_result_t decode_object_header(const uint8_t *data, vblf_object_header_base_t &out_header)
{
    if (std::memcmp(data, VBLF_OBJ_SIGNATURE, VBLF_OBJ_SIGNATURE_SIZE) != 0)
    {
        return VBLF_ERROR_INVALID_SIGNATURE;
    }
    vblf_object_header_base_t header{};
    std::memcpy(header.signature, data, VBLF_OBJ_SIGNATURE_SIZE);
    header.header_size = read_u16_le(data + 4);
    header.header_version = read_u16_le(data + 6);
    header.object_size = read_u32_le(data + 8);
    header.object_type = read_u32_le(data + 12);
    if (header.header_size < 16 || header.object_size < header.header_size)
    {
        return VBLF_ERROR_INVALID_SIZE;
    }
    out_header = header;
    return VBLF_OK;
}

/** @brief Check a physical object's declared boundary without reading its payload. */
vblf_result_t check_file_extent(std::fstream &file, uint32_t remaining)
{
    const std::streamoff position = file.tellg();
    file.seekg(0, std::ios::end);
    const std::streamoff end = file.tellg();
    if (!file || position < 0 || end < position)
    {
        return VBLF_ERROR_IO;
    }
    if (static_cast<uint64_t>(end - position) < remaining)
    {
        return VBLF_ERROR_TRUNCATED;
    }
    file.seekg(position, std::ios::beg);
    return file ? VBLF_OK : VBLF_ERROR_IO;
}

/** @brief Open the next raw object or standard uncompressed/zlib log container. */
vblf_result_t begin_source(std::fstream &file, vblf_reader &reader)
{
    std::array<uint8_t, 16> data{};
    uint32_t padding = 0;
    do
    {
        const vblf_result_t result = read_file_bytes(file, data.data(), 1);
        if (result != VBLF_OK)
        {
            return result;
        }
        if (data[0] != 0)
        {
            break;
        }
        if (++padding > 7)
        {
            return VBLF_ERROR_INVALID_SIGNATURE;
        }
    } while (true);

    vblf_result_t result = read_file_bytes(file, data.data() + 1, 15);
    if (result != VBLF_OK)
    {
        return result == VBLF_EOF ? VBLF_ERROR_TRUNCATED : result;
    }
    vblf_object_header_base_t header{};
    result = decode_object_header(data.data(), header);
    if (result != VBLF_OK)
    {
        return result;
    }
    result = check_file_extent(file, header.object_size - 16);
    if (result != VBLF_OK)
    {
        return result;
    }

    if (header.object_type != VBLF_OBJ_TYPE_LOG_CONTAINER)
    {
        file.seekg(-16, std::ios::cur);
        if (!file)
        {
            return VBLF_ERROR_IO;
        }
        reader.compression_method = 0;
        reader.input_remaining = header.object_size;
        reader.output_remaining = header.object_size;
    }
    else
    {
        if (header.object_size < 32)
        {
            return VBLF_ERROR_INVALID_SIZE;
        }
        if (header.header_version != 1 || (header.header_size != 16 && header.header_size != 32))
        {
            return VBLF_ERROR_UNSUPPORTED_HEADER;
        }
        result = read_file_bytes(file, data.data(), 16);
        if (result != VBLF_OK)
        {
            return result == VBLF_EOF ? VBLF_ERROR_TRUNCATED : result;
        }
        reader.compression_method = read_u16_le(data.data());
        reader.input_remaining = header.object_size - 32;
        reader.output_remaining = read_u32_le(data.data() + 8);
        if (reader.compression_method == 0)
        {
            if (reader.input_remaining != reader.output_remaining)
            {
                return VBLF_ERROR_INVALID_SIZE;
            }
        }
        else if (reader.compression_method == 2)
        {
            const int status = reader.inflater_initialized ? inflateReset(&reader.inflater) : inflateInit(&reader.inflater);
            if (status != Z_OK)
            {
                return status == Z_MEM_ERROR ? VBLF_ERROR_OUT_OF_MEMORY : VBLF_ERROR_INTERNAL;
            }
            reader.inflater_initialized = true;
            reader.inflater.next_in = Z_NULL;
            reader.inflater.avail_in = 0;
        }
        else
        {
            return VBLF_ERROR_UNSUPPORTED_COMPRESSION;
        }
    }
    reader.source_active = true;
    return VBLF_OK;
}

/**
 * @brief Read logical object bytes across container boundaries with bounded memory.
 * @note zlib input is owned by the reader. Output buffers are used only during
 * this call. The stream is finalized when its declared output length is reached.
 */
vblf_result_t read_object_bytes(std::fstream &file, vblf_reader &reader, uint8_t *data, uint32_t count)
{
    uint32_t written = 0;
    while (written < count || (reader.source_active && reader.output_remaining == 0))
    {
        if (!reader.source_active)
        {
            const vblf_result_t result = begin_source(file, reader);
            if (result != VBLF_OK)
            {
                return result == VBLF_EOF && written != 0 ? VBLF_ERROR_TRUNCATED : result;
            }
        }

        if (reader.compression_method == 0)
        {
            const uint32_t length = std::min(count - written, reader.output_remaining);
            if (length != 0)
            {
                const vblf_result_t result = read_file_bytes(file, data + written, length);
                if (result != VBLF_OK)
                {
                    return result == VBLF_EOF ? VBLF_ERROR_TRUNCATED : result;
                }
                written += length;
                reader.input_remaining -= length;
                reader.output_remaining -= length;
            }
            reader.source_active = reader.output_remaining != 0;
            continue;
        }

        if (reader.inflater.avail_in == 0 && reader.input_remaining != 0)
        {
            const uint32_t length = std::min(reader.input_remaining, static_cast<uint32_t>(reader.input.size()));
            const vblf_result_t result = read_file_bytes(file, reader.input.data(), length);
            if (result != VBLF_OK)
            {
                return result == VBLF_EOF ? VBLF_ERROR_TRUNCATED : result;
            }
            reader.input_remaining -= length;
            reader.inflater.next_in = reader.input.data();
            reader.inflater.avail_in = length;
        }

        uint8_t excess = 0;
        const bool finishing = reader.output_remaining == 0;
        const uint32_t capacity = finishing ? 1 : std::min(count - written, reader.output_remaining);
        reader.inflater.next_out = finishing ? &excess : data + written;
        reader.inflater.avail_out = capacity;
        const uInt input_before = reader.inflater.avail_in;
        const int status = inflate(&reader.inflater, Z_NO_FLUSH);
        const uint32_t produced = capacity - reader.inflater.avail_out;
        reader.inflater.next_out = Z_NULL;
        reader.inflater.avail_out = 0;
        if (finishing && produced != 0)
        {
            return VBLF_ERROR_INVALID_COMPRESSION;
        }
        if (!finishing)
        {
            written += produced;
            reader.output_remaining -= produced;
        }
        if (status == Z_STREAM_END)
        {
            if (reader.output_remaining != 0 || reader.input_remaining != 0 || reader.inflater.avail_in != 0)
            {
                return VBLF_ERROR_INVALID_COMPRESSION;
            }
            reader.source_active = false;
        }
        else if (status == Z_MEM_ERROR)
        {
            return VBLF_ERROR_OUT_OF_MEMORY;
        }
        else if (status != Z_OK && status != Z_BUF_ERROR)
        {
            return status == Z_STREAM_ERROR ? VBLF_ERROR_INTERNAL : VBLF_ERROR_INVALID_COMPRESSION;
        }
        else if (produced == 0 && reader.inflater.avail_in == input_before)
        {
            return VBLF_ERROR_INVALID_COMPRESSION;
        }
    }
    return VBLF_OK;
}

/** @brief Read a common business-object header, skipping only bounded zero padding. */
vblf_result_t read_next_object_header(std::fstream &file, vblf_reader &reader, vblf_object_header_base_t &out_header)
{
    std::array<uint8_t, 16> data{};
    uint32_t padding = 0;
    do
    {
        const vblf_result_t result = read_object_bytes(file, reader, data.data(), 1);
        if (result != VBLF_OK)
        {
            return result;
        }
        if (data[0] != 0)
        {
            break;
        }
        if (++padding > 7)
        {
            return VBLF_ERROR_INVALID_SIGNATURE;
        }
    } while (true);

    const vblf_result_t result = read_object_bytes(file, reader, data.data() + 1, 15);
    if (result != VBLF_OK)
    {
        return result == VBLF_EOF ? VBLF_ERROR_TRUNCATED : result;
    }
    vblf_object_header_base_t header{};
    const vblf_result_t decoded = decode_object_header(data.data(), header);
    if (decoded != VBLF_OK)
    {
        return decoded;
    }
    if (header.object_type == VBLF_OBJ_TYPE_LOG_CONTAINER)
    {
        return VBLF_ERROR_INVALID_PAYLOAD;
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

/** @brief Cache and return the next logical object header without consuming its payload. */
VBLF_API vblf_peek_object(vblf_handle_t *handle, vblf_object_header_base_t *out_header)
{
    if (handle == nullptr || out_header == nullptr)
    {
        return VBLF_ERROR_INVALID_ARGUMENT;
    }
    if (handle->mode != VBLF_OPEN_READ)
    {
        return VBLF_ERROR_INVALID_MODE;
    }

    vblf_result_t result = VBLF_ERROR_INTERNAL;
    try
    {
        if (!handle->reader)
        {
            handle->reader = std::make_unique<vblf_reader>();
        }
        auto &reader = *handle->reader;
        if (reader.header_pending)
        {
            *out_header = reader.pending_header;
            return VBLF_OK;
        }
        if (reader.terminal_result != VBLF_OK)
        {
            return reader.terminal_result;
        }
        vblf_object_header_base_t header{};
        result = read_next_object_header(handle->file, reader, header);
        if (result == VBLF_OK)
        {
            reader.pending_header = header;
            reader.header_pending = true;
            *out_header = header;
            return VBLF_OK;
        }
    }
    catch (const std::bad_alloc &)
    {
        result = VBLF_ERROR_OUT_OF_MEMORY;
    }
    catch (const std::system_error &)
    {
        result = VBLF_ERROR_IO;
    }
    catch (...)
    {
        result = VBLF_ERROR_INTERNAL;
    }
    if (handle->reader)
    {
        handle->reader->terminal_result = result;
    }
    return result;
}

/** @brief Discard one logical object using bounded storage without decoding its payload. */
VBLF_API vblf_skip_object(vblf_handle_t *handle)
{
    if (handle == nullptr)
    {
        return VBLF_ERROR_INVALID_ARGUMENT;
    }
    if (handle->mode != VBLF_OPEN_READ)
    {
        return VBLF_ERROR_INVALID_MODE;
    }

    vblf_result_t result = VBLF_ERROR_INTERNAL;
    try
    {
        vblf_object_header_base_t header{};
        result = vblf_peek_object(handle, &header);
        if (result != VBLF_OK)
        {
            return result;
        }

        auto &reader = *handle->reader;
        std::array<uint8_t, 4096> discard{};
        uint32_t remaining = header.object_size - 16;
        while (remaining != 0)
        {
            const uint32_t length = std::min(remaining, static_cast<uint32_t>(discard.size()));
            result = read_object_bytes(handle->file, reader, discard.data(), length);
            if (result != VBLF_OK)
            {
                if (result == VBLF_EOF)
                {
                    result = VBLF_ERROR_TRUNCATED;
                }
                break;
            }
            remaining -= length;
        }
        if (remaining == 0)
        {
            reader.header_pending = false;
            return VBLF_OK;
        }
    }
    catch (const std::bad_alloc &)
    {
        result = VBLF_ERROR_OUT_OF_MEMORY;
    }
    catch (const std::system_error &)
    {
        result = VBLF_ERROR_IO;
    }
    catch (...)
    {
        result = VBLF_ERROR_INTERNAL;
    }
    if (handle->reader)
    {
        // A failed discard may have consumed bytes, so the cached header is no longer usable.
        handle->reader->header_pending = false;
        handle->reader->terminal_result = result;
    }
    return result;
}
