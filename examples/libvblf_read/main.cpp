#include "vblf.h"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <tuple>

namespace
{

/** @brief Identify one CAN identifier on a channel, preserving STD/EXT distinction. */
struct frame_key
{
    uint16_t channel;
    uint32_t id;
    bool extended;

    bool operator<(const frame_key &other) const
    {
        return std::tie(channel, id, extended) < std::tie(other.channel, other.id, other.extended);
    }
};

/** @brief Count supported frames by channel and identifier, then print a sorted list. */
int read_blf(const char *path_utf8)
{
    vblf_handle_t *handle = nullptr;
    vblf_result_t result = vblf_create_file(path_utf8, VBLF_OPEN_READ, &handle);
    if (result != VBLF_OK)
    {
        std::cerr << "Failed to open BLF file (result " << result << ").\n";
        return EXIT_FAILURE;
    }
    std::unique_ptr<vblf_handle_t, decltype(&vblf_close_handle)> owner(handle, &vblf_close_handle);

    // Only distinct channel/ID combinations occupy storage; individual frames are not retained.
    std::map<frame_key, uint64_t> counts;
    uint64_t skipped = 0;
    while (true)
    {
        vblf_object_header_base_t header{};
        result = vblf_peek_object(handle, &header);
        if (result == VBLF_EOF)
        {
            break;
        }
        if (result != VBLF_OK)
        {
            std::cerr << "Failed to read BLF object header (result " << result << ").\n";
            break;
        }

        uint16_t channel = 0;
        uint32_t frame_id = 0;
        if (header.object_type == VBLF_OBJ_TYPE_CAN_MESSAGE)
        {
            vblf_can_message_t message{};
            result = vblf_read_object_secure(handle, &message.header.base, static_cast<uint32_t>(sizeof(message)));
            channel = message.channel;
            frame_id = message.frame_id;
        }
        else if (header.object_type == VBLF_OBJ_TYPE_CAN_FD_MESSAGE_64)
        {
            vblf_can_fd_message_64_t message{};
            result = vblf_read_object_secure(handle, &message.header.base, static_cast<uint32_t>(sizeof(message)));
            channel = message.channel;
            frame_id = message.frame_id;
        }
        else
        {
            result = VBLF_ERROR_UNSUPPORTED_OBJECT;
        }
        if (result == VBLF_ERROR_UNSUPPORTED_OBJECT || result == VBLF_ERROR_UNSUPPORTED_HEADER)
        {
            result = vblf_skip_object(handle);
            if (result != VBLF_OK)
            {
                std::cerr << "Failed to skip BLF object (result " << result << ").\n";
                break;
            }
            ++skipped;
            continue;
        }
        if (result != VBLF_OK)
        {
            std::cerr << "Failed to read CAN message (result " << result << ").\n";
            break;
        }

        // Both supported object types keep data inline and do not require vblf_free_object().
        ++counts[{channel, frame_id & UINT32_C(0x1FFFFFFF), (frame_id & UINT32_C(0x80000000)) != 0}];
    }

    const vblf_result_t close_result = vblf_close_handle(owner.release());
    if (close_result != VBLF_OK)
    {
        std::cerr << "Failed to close BLF file (result " << close_result << ").\n";
    }
    if (result != VBLF_EOF || close_result != VBLF_OK)
    {
        return EXIT_FAILURE;
    }

    std::cout << "Channel  ID                              Frames\n";
    for (const auto &[key, count] : counts)
    {
        std::cout << std::right << std::dec << std::setw(7) << key.channel
                  << "  0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << key.id
                  << std::setfill(' ') << std::dec << (key.extended ? " EXT" : " STD")
                  << "  " << std::setw(20) << count << '\n';
        if (!std::cout)
        {
            break;
        }
    }
    std::cout.flush();
    if (!std::cout)
    {
        std::cerr << "Failed to write frame statistics to standard output.\n";
        return EXIT_FAILURE;
    }
    if (skipped != 0)
    {
        std::cerr << "Skipped unsupported objects: " << skipped << ".\n";
    }
    return EXIT_SUCCESS;
}

} // namespace

/** @brief Accept a native file path and pass its UTF-8 representation to the C API. */
#if defined(_WIN32)
int wmain(int argc, wchar_t *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    if (argc != 2)
    {
        std::cerr << "Usage: libvblf_read <file.blf>\n"
                     "Lists frame counts by channel and CAN ID, keeping STD and EXT identifiers separate.\n";
        return EXIT_FAILURE;
    }

    try
    {
        const auto path = std::filesystem::path(argv[1]).u8string();
        const std::string path_utf8(path.begin(), path.end());
        return read_blf(path_utf8.c_str());
    }
    catch (const std::exception &error)
    {
        std::cerr << "Failed to read BLF file: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
