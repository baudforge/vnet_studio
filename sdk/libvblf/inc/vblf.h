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

#ifndef VBLF_H_INCLUDED
#define VBLF_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

/**
 * @file vblf.h
 * @brief Public C API and constants for BLF file access.
 *
 * @details This initial header contains constants, the common object header and
 * seven core file/object API declarations. Concrete object layouts and function
 * implementations will be introduced separately. Original constant values,
 * reserved IDs and flag combinations are preserved.
 *
 * @note Constants and base header fields are adapted from the MIT-licensed
 * @c src/vblf/constants.py and @c src/vblf/general.py at revision
 * @c e509ae9c64c834851418e74e9bd89a32efe41d3c.
 * Python @c from_int() conversion helpers are not implemented in this header.
 * No C++ exception may cross the C API boundary.
 *
 * @see https://github.com/zariiii9003/vblf
 */

/** @brief BLF file signature. Its terminating C NUL is not stored in the file. */
#define VBLF_FILE_SIGNATURE "LOGG"
/** @brief BLF object signature. Its terminating C NUL is not stored in the file. */
#define VBLF_OBJ_SIGNATURE "LOBJ"
/** @brief Number of stored bytes in the BLF object signature. */
#define VBLF_OBJ_SIGNATURE_SIZE UINT32_C(4)

/**
 * @def VBLF_API
 * @brief Declare a VBLF function returning vblf_result_t.
 * @details Combines symbol visibility, result type and calling convention.
 * Windows declarations include @c __stdcall.
 * @note Define @c VBLF_SHARED for DLL use and @c VBLF_BUILD_EXPORTS when building
 * that DLL. Other platforms use their default calling convention.
 */
#ifndef VBLF_API
#if defined(_WIN32) && defined(VBLF_SHARED) && defined(VBLF_BUILD_EXPORTS)
#define VBLF_API __declspec(dllexport) vblf_result_t __stdcall
#elif defined(_WIN32) && defined(VBLF_SHARED)
#define VBLF_API __declspec(dllimport) vblf_result_t __stdcall
#elif defined(_WIN32)
#define VBLF_API vblf_result_t __stdcall
#elif defined(VBLF_SHARED) && defined(VBLF_BUILD_EXPORTS) &&                                       \
    (defined(__GNUC__) || defined(__clang__))
#define VBLF_API __attribute__((visibility("default"))) vblf_result_t
#else
#define VBLF_API vblf_result_t
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Signed 32-bit result returned by every core API.
 * @details Zero indicates success, a positive value indicates EOF, and negative
 * values indicate errors.
 * @see VBLF_OK, VBLF_EOF
 */
typedef int32_t vblf_result_t;

/** @brief Result codes for the core file APIs. */
enum {
    VBLF_OK = 0,                       /**< Operation succeeded. */
    VBLF_EOF = 1,                      /**< Clean end of the input stream. */
    VBLF_ERROR_INVALID_ARGUMENT = -1,  /**< A required argument is invalid. */
    VBLF_ERROR_IO = -2,                /**< An input/output operation failed. */
    VBLF_ERROR_TRUNCATED = -3,         /**< Input ends before its declared data is complete. */
    VBLF_ERROR_INVALID_SIGNATURE = -4, /**< A file or object signature is invalid. */
    VBLF_ERROR_INVALID_SIZE = -5,      /**< A declared header or object size is invalid. */
    VBLF_ERROR_LIMIT_EXCEEDED = -6,    /**< A configured resource limit was exceeded. */
    VBLF_ERROR_UNSUPPORTED_COMPRESSION = -7, /**< The compression method is unsupported. */
    VBLF_ERROR_INVALID_COMPRESSION = -8,     /**< Compressed data or its checksum is invalid. */
    VBLF_ERROR_UNSUPPORTED_HEADER = -9,      /**< The object header format is unsupported. */
    VBLF_ERROR_INVALID_PAYLOAD = -10,        /**< The object payload is invalid. */
    VBLF_ERROR_TIMESTAMP_OVERFLOW = -11,     /**< A timestamp conversion would overflow. */
    VBLF_ERROR_CANCELLED = -12,              /**< The operation was cancelled. */
    VBLF_ERROR_OUT_OF_MEMORY = -13,          /**< Memory allocation failed. */
    VBLF_ERROR_INTERNAL = -14,               /**< An unexpected implementation failure occurred. */
    VBLF_ERROR_INVALID_MODE = -15, /**< The requested operation or open mode is incompatible. */
    VBLF_ERROR_UNSUPPORTED_OBJECT = -16, /**< The object type is unsupported by the operation. */
    VBLF_ERROR_BUFFER_TOO_SMALL = -17    /**< Caller-provided object storage is too small. */
};

/**
 * @defgroup vblf_constants BLF constants
 * @brief Constants migrated from upstream @c constants.py.
 * @details Fixed-width aliases keep native enum size out of API parameters and
 * future object fields. Unlisted numeric values and unknown flag bits remain
 * representable. Defining a constant does not imply decoder support.
 * @{
 */

/**
 * @brief BLF object type identifier.
 * @note Adapted from upstream @c ObjType (IntEnum).
 */
typedef uint32_t vblf_obj_type_t;

/** @brief Upstream object type identifiers. */
enum {
    VBLF_OBJ_TYPE_UNKNOWN = 0,
    VBLF_OBJ_TYPE_CAN_MESSAGE = 1,
    VBLF_OBJ_TYPE_CAN_ERROR = 2,
    VBLF_OBJ_TYPE_CAN_OVERLOAD = 3,
    VBLF_OBJ_TYPE_CAN_STATISTIC = 4,
    VBLF_OBJ_TYPE_APP_TRIGGER = 5,
    VBLF_OBJ_TYPE_ENV_INTEGER = 6,
    VBLF_OBJ_TYPE_ENV_DOUBLE = 7,
    VBLF_OBJ_TYPE_ENV_STRING = 8,
    VBLF_OBJ_TYPE_ENV_DATA = 9,
    VBLF_OBJ_TYPE_LOG_CONTAINER = 10,
    VBLF_OBJ_TYPE_LIN_MESSAGE = 11,
    VBLF_OBJ_TYPE_LIN_CRC_ERROR = 12,
    VBLF_OBJ_TYPE_LIN_DLC_INFO = 13,
    VBLF_OBJ_TYPE_LIN_RCV_ERROR = 14,
    VBLF_OBJ_TYPE_LIN_SND_ERROR = 15,
    VBLF_OBJ_TYPE_LIN_SLV_TIMEOUT = 16,
    VBLF_OBJ_TYPE_LIN_SCHED_MODCH = 17,
    VBLF_OBJ_TYPE_LIN_SYN_ERROR = 18,
    VBLF_OBJ_TYPE_LIN_BAUDRATE = 19,
    VBLF_OBJ_TYPE_LIN_SLEEP = 20,
    VBLF_OBJ_TYPE_LIN_WAKEUP = 21,
    VBLF_OBJ_TYPE_MOST_SPY = 22,
    VBLF_OBJ_TYPE_MOST_CTRL = 23,
    VBLF_OBJ_TYPE_MOST_LIGHTLOCK = 24,
    VBLF_OBJ_TYPE_MOST_STATISTIC = 25,
    VBLF_OBJ_TYPE_RESERVED_1 = 26, /**< Reserved BLF object type. */
    VBLF_OBJ_TYPE_RESERVED_2 = 27, /**< Reserved BLF object type. */
    VBLF_OBJ_TYPE_RESERVED_3 = 28, /**< Reserved BLF object type. */
    VBLF_OBJ_TYPE_FLEXRAY_DATA = 29,
    VBLF_OBJ_TYPE_FLEXRAY_SYNC = 30,
    VBLF_OBJ_TYPE_CAN_DRIVER_ERROR = 31,
    VBLF_OBJ_TYPE_MOST_PKT = 32,
    VBLF_OBJ_TYPE_MOST_PKT2 = 33,
    VBLF_OBJ_TYPE_MOST_HWMODE = 34,
    VBLF_OBJ_TYPE_MOST_REG = 35,
    VBLF_OBJ_TYPE_MOST_GENREG = 36,
    VBLF_OBJ_TYPE_MOST_NETSTATE = 37,
    VBLF_OBJ_TYPE_MOST_DATALOST = 38,
    VBLF_OBJ_TYPE_MOST_TRIGGER = 39,
    VBLF_OBJ_TYPE_FLEXRAY_CYCLE = 40,
    VBLF_OBJ_TYPE_FLEXRAY_MESSAGE = 41,
    VBLF_OBJ_TYPE_LIN_CHECKSUM_INFO = 42,
    VBLF_OBJ_TYPE_LIN_SPIKE_EVENT = 43,
    VBLF_OBJ_TYPE_CAN_DRIVER_SYNC = 44,
    VBLF_OBJ_TYPE_FLEXRAY_STATUS = 45,
    VBLF_OBJ_TYPE_GPS_EVENT = 46,
    VBLF_OBJ_TYPE_FR_ERROR = 47,
    VBLF_OBJ_TYPE_FR_STATUS = 48,
    VBLF_OBJ_TYPE_FR_STARTCYCLE = 49,
    VBLF_OBJ_TYPE_FR_RCVMESSAGE = 50,
    VBLF_OBJ_TYPE_REALTIMECLOCK = 51,
    VBLF_OBJ_TYPE_AVAILABLE2 = 52,
    VBLF_OBJ_TYPE_AVAILABLE3 = 53,
    VBLF_OBJ_TYPE_LIN_STATISTIC = 54,
    VBLF_OBJ_TYPE_J1708_MESSAGE = 55,
    VBLF_OBJ_TYPE_J1708_VIRTUAL_MSG = 56,
    VBLF_OBJ_TYPE_LIN_MESSAGE2 = 57,
    VBLF_OBJ_TYPE_LIN_SND_ERROR2 = 58,
    VBLF_OBJ_TYPE_LIN_SYN_ERROR2 = 59,
    VBLF_OBJ_TYPE_LIN_CRC_ERROR2 = 60,
    VBLF_OBJ_TYPE_LIN_RCV_ERROR2 = 61,
    VBLF_OBJ_TYPE_LIN_WAKEUP2 = 62,
    VBLF_OBJ_TYPE_LIN_SPIKE_EVENT2 = 63,
    VBLF_OBJ_TYPE_LIN_LONG_DOM_SIG = 64,
    VBLF_OBJ_TYPE_APP_TEXT = 65,
    VBLF_OBJ_TYPE_FR_RCVMESSAGE_EX = 66,
    VBLF_OBJ_TYPE_MOST_STATISTICEX = 67,
    VBLF_OBJ_TYPE_MOST_TXLIGHT = 68,
    VBLF_OBJ_TYPE_MOST_ALLOCTAB = 69,
    VBLF_OBJ_TYPE_MOST_STRESS = 70,
    VBLF_OBJ_TYPE_ETHERNET_FRAME = 71,
    VBLF_OBJ_TYPE_SYS_VARIABLE = 72,
    VBLF_OBJ_TYPE_CAN_ERROR_EXT = 73,
    VBLF_OBJ_TYPE_CAN_DRIVER_ERROR_EXT = 74,
    VBLF_OBJ_TYPE_LIN_LONG_DOM_SIG2 = 75,
    VBLF_OBJ_TYPE_MOST_150_MESSAGE = 76,
    VBLF_OBJ_TYPE_MOST_150_PKT = 77,
    VBLF_OBJ_TYPE_MOST_ETHERNET_PKT = 78,
    VBLF_OBJ_TYPE_MOST_150_MESSAGE_FRAGMENT = 79,
    VBLF_OBJ_TYPE_MOST_150_PKT_FRAGMENT = 80,
    VBLF_OBJ_TYPE_MOST_ETHERNET_PKT_FRAGMENT = 81,
    VBLF_OBJ_TYPE_MOST_SYSTEM_EVENT = 82,
    VBLF_OBJ_TYPE_MOST_150_ALLOCTAB = 83,
    VBLF_OBJ_TYPE_MOST_50_MESSAGE = 84,
    VBLF_OBJ_TYPE_MOST_50_PKT = 85,
    VBLF_OBJ_TYPE_CAN_MESSAGE2 = 86,
    VBLF_OBJ_TYPE_LIN_UNEXPECTED_WAKEUP = 87,
    VBLF_OBJ_TYPE_LIN_SHORT_OR_SLOW_RESPONSE = 88,
    VBLF_OBJ_TYPE_LIN_DISTURBANCE_EVENT = 89,
    VBLF_OBJ_TYPE_SERIAL_EVENT = 90,
    VBLF_OBJ_TYPE_OVERRUN_ERROR = 91,
    VBLF_OBJ_TYPE_EVENT_COMMENT = 92,
    VBLF_OBJ_TYPE_WLAN_FRAME = 93,
    VBLF_OBJ_TYPE_WLAN_STATISTIC = 94,
    VBLF_OBJ_TYPE_MOST_ECL = 95,
    VBLF_OBJ_TYPE_GLOBAL_MARKER = 96,
    VBLF_OBJ_TYPE_AFDX_FRAME = 97,
    VBLF_OBJ_TYPE_AFDX_STATISTIC = 98,
    VBLF_OBJ_TYPE_KLINE_STATUSEVENT = 99,
    VBLF_OBJ_TYPE_CAN_FD_MESSAGE = 100,
    VBLF_OBJ_TYPE_CAN_FD_MESSAGE_64 = 101,
    VBLF_OBJ_TYPE_ETHERNET_RX_ERROR = 102,
    VBLF_OBJ_TYPE_ETHERNET_STATUS = 103,
    VBLF_OBJ_TYPE_CAN_FD_ERROR_64 = 104,
    VBLF_OBJ_TYPE_LIN_SHORT_OR_SLOW_RESPONSE2 = 105,
    VBLF_OBJ_TYPE_AFDX_STATUS = 106,
    VBLF_OBJ_TYPE_AFDX_BUS_STATISTIC = 107,
    VBLF_OBJ_TYPE_RESERVED_4 = 108, /**< Reserved BLF object type. */
    VBLF_OBJ_TYPE_AFDX_ERROR_EVENT = 109,
    VBLF_OBJ_TYPE_A429_ERROR = 110,
    VBLF_OBJ_TYPE_A429_STATUS = 111,
    VBLF_OBJ_TYPE_A429_BUS_STATISTIC = 112,
    VBLF_OBJ_TYPE_A429_MESSAGE = 113,
    VBLF_OBJ_TYPE_ETHERNET_STATISTIC = 114,
    VBLF_OBJ_TYPE_RESERVED_5 = 115, /**< Reserved BLF object type. */
    VBLF_OBJ_TYPE_RESERVED_6 = 116, /**< Reserved BLF object type. */
    VBLF_OBJ_TYPE_RESERVED_7 = 117, /**< Reserved BLF object type. */
    VBLF_OBJ_TYPE_TEST_STRUCTURE = 118,
    VBLF_OBJ_TYPE_DIAG_REQUEST_INTERPRETATION = 119,
    VBLF_OBJ_TYPE_ETHERNET_FRAME_EX = 120,
    VBLF_OBJ_TYPE_ETHERNET_FRAME_FORWARDED = 121,
    VBLF_OBJ_TYPE_ETHERNET_ERROR_EX = 122,
    VBLF_OBJ_TYPE_ETHERNET_ERROR_FORWARDED = 123,
    VBLF_OBJ_TYPE_FUNCTION_BUS = 124,
    VBLF_OBJ_TYPE_DATA_LOST_BEGIN = 125,
    VBLF_OBJ_TYPE_DATA_LOST_END = 126,
    VBLF_OBJ_TYPE_WATER_MARK_EVENT = 127,
    VBLF_OBJ_TYPE_TRIGGER_CONDITION = 128
};

/**
 * @brief BLF producer application identifier.
 * @note Adapted from upstream @c AppId (IntEnum).
 */
typedef uint32_t vblf_app_id_t;

/** @brief Upstream application identifiers. */
enum {
    VBLF_APP_ID_UNKNOWN = 0,
    VBLF_APP_ID_CANALYZER = 1,
    VBLF_APP_ID_CANOE = 2,
    VBLF_APP_ID_CANSTRESS = 3,
    VBLF_APP_ID_CANLOG = 4,
    VBLF_APP_ID_CANAPE = 5,
    VBLF_APP_ID_CANCASEXLLOG = 6,
    VBLF_APP_ID_VLCONFIG = 7,
    VBLF_APP_ID_PORSCHELOGGER = 200
};

/**
 * @brief BLF compression level.
 * @note Adapted from upstream @c Compression (IntEnum).
 * @note These are compression levels, not log-container compression-method IDs.
 */
typedef uint32_t vblf_compression_t;

/** @brief Upstream compression levels. */
enum {
    VBLF_COMPRESSION_NONE = 0,
    VBLF_COMPRESSION_SPEED = 1,
    VBLF_COMPRESSION_DEFAULT = 6,
    VBLF_COMPRESSION_MAX = 9
};

/**
 * @brief BLF object header flag mask.
 * @note Adapted from upstream @c ObjFlags (IntFlag).
 * @note Flags can be combined with bitwise OR.
 */
typedef uint32_t vblf_obj_flags_t;

/** @brief Upstream object header flags. */
enum { VBLF_OBJ_FLAGS_TIME_TEN_MICS = 0x1, VBLF_OBJ_FLAGS_TIME_ONE_NANS = 0x2 };

/**
 * @brief Trigger event flag mask.
 * @note Adapted from upstream @c TriggerFlag (IntFlag).
 * @note Flags can be combined with bitwise OR.
 */
typedef uint32_t vblf_trigger_flag_t;

/** @brief Upstream trigger event flags. */
enum {
    VBLF_TRIGGER_FLAG_SINGLE_TRIGGER = 0x0,
    VBLF_TRIGGER_FLAG_LOGGING_START = 0x1,
    VBLF_TRIGGER_FLAG_LOGGING_STOP = 0x2
};

/**
 * @brief CAN FD flag mask.
 * @note Adapted from upstream @c CanFdFlags (IntFlag).
 * @note Flags can be combined with bitwise OR.
 */
typedef uint32_t vblf_can_fd_flags_t;

/** @brief Upstream CAN FD flags. */
enum {
    VBLF_CAN_FD_FLAGS_NERR = 0x0004,
    VBLF_CAN_FD_FLAGS_HIGH_VOLTAGE_WAKE_UP = 0x0008,
    VBLF_CAN_FD_FLAGS_REMOTE_FRAME = 0x0010,
    VBLF_CAN_FD_FLAGS_TX_ACKNOWLEDGE = 0x0040,
    VBLF_CAN_FD_FLAGS_TX_REQUEST = 0x0080,
    VBLF_CAN_FD_FLAGS_SRR = 0x0200,
    VBLF_CAN_FD_FLAGS_R0 = 0x0400,
    VBLF_CAN_FD_FLAGS_R1 = 0x0800,
    VBLF_CAN_FD_FLAGS_FDF = 0x1000,
    VBLF_CAN_FD_FLAGS_BRS = 0x2000,
    VBLF_CAN_FD_FLAGS_ESI = 0x4000,
    VBLF_CAN_FD_FLAGS_FRAME_PART_OF_BURST = 0x20000,
    VBLF_CAN_FD_FLAGS_SINGLE_SHOT_MODE_NOT_TRANSMITTED = 0x40000,
    VBLF_CAN_FD_FLAGS_SINGLE_SHOT_MODE_REASON =
        0x80000 /**< Clear: arbitration lost; set: frame disturbed. */
};

/**
 * @brief System variable type identifier.
 * @note Adapted from upstream @c SysVarType (IntEnum).
 */
typedef uint32_t vblf_sys_var_type_t;

/** @brief Upstream system variable types. */
enum {
    VBLF_SYS_VAR_TYPE_DOUBLE = 1,
    VBLF_SYS_VAR_TYPE_LONG = 2,
    VBLF_SYS_VAR_TYPE_STRING = 3,
    VBLF_SYS_VAR_TYPE_DOUBLEARRAY = 4,
    VBLF_SYS_VAR_TYPE_LONGARRAY = 5,
    VBLF_SYS_VAR_TYPE_LONGLONG = 6,
    VBLF_SYS_VAR_TYPE_BYTEARRAY = 7
};

/**
 * @brief Bus type identifier.
 * @note Adapted from upstream @c BusType (IntEnum).
 */
typedef uint32_t vblf_bus_type_t;

/** @brief Upstream bus type identifiers. */
enum {
    VBLF_BUS_TYPE_CAN = 1,
    VBLF_BUS_TYPE_LIN = 5,
    VBLF_BUS_TYPE_MOST = 6,
    VBLF_BUS_TYPE_FLEXRAY = 7,
    VBLF_BUS_TYPE_J1708 = 9,
    VBLF_BUS_TYPE_ETHERNET = 10,
    VBLF_BUS_TYPE_WLAN = 13,
    VBLF_BUS_TYPE_AFDX = 14
};

/**
 * @brief Application text source identifier.
 * @note Adapted from upstream @c AppTextSource (IntEnum).
 */
typedef uint32_t vblf_app_text_source_t;

/** @brief Upstream application text sources. */
enum {
    VBLF_APP_TEXT_SOURCE_MEASUREMENTCOMMENT = 0,
    VBLF_APP_TEXT_SOURCE_DBCHANNELINFO = 1,
    VBLF_APP_TEXT_SOURCE_METADATA = 2,
    VBLF_APP_TEXT_SOURCE_ATTACHMENT = 3,
    VBLF_APP_TEXT_SOURCE_TRACELINE = 4
};

/**
 * @brief Function bus type identifier.
 * @note Adapted from upstream @c FunctionBusType (IntEnum).
 */
typedef uint32_t vblf_function_bus_type_t;

/** @brief Upstream function bus types. */
enum {
    VBLF_FUNCTION_BUS_TYPE_UNDEFINED = 0,
    VBLF_FUNCTION_BUS_TYPE_SIGNAL = 1,
    VBLF_FUNCTION_BUS_TYPE_SERVICE_FUNCTION = 2,
    VBLF_FUNCTION_BUS_TYPE_STATE = 3
};

/**
 * @brief Trigger condition status value.
 * @note Adapted from upstream @c TriggerConditionStatus (IntEnum).
 */
typedef uint32_t vblf_trigger_condition_status_t;

/** @brief Upstream trigger condition status values. */
enum {
    VBLF_TRIGGER_CONDITION_STATUS_UNKNOWN = 0,
    VBLF_TRIGGER_CONDITION_STATUS_START = 1,
    VBLF_TRIGGER_CONDITION_STATUS_STOP = 2,
    VBLF_TRIGGER_CONDITION_STATUS_STARTSTOP = 3
};

/** @} */ /* vblf_constants */

/**
 * @brief Mode used to open a BLF file.
 * @note This is a library API value, not an upstream BLF constant.
 */
typedef uint32_t vblf_open_mode_t;

/** @brief Supported open modes; modes must not be combined. */
enum {
    VBLF_OPEN_READ = 1, /**< Open an existing file for reading. */
    VBLF_OPEN_WRITE = 2 /**< Create a file or truncate an existing file for writing. */
};

/**
 * @brief Common metadata prefix of every public object.
 *
 * @details An extended header embeds this structure as its first member, and a
 * concrete object embeds its header as its first member. The base header must
 * therefore begin at offset zero in every concrete object; no shared union is
 * required. Pass the address of that actual base member to the object APIs.
 *
 * @note This is a naturally aligned host-memory structure. Implementations must
 * decode and encode fields explicitly rather than copying C structures to or
 * from the file. The size fields describe serialized data, not C storage.
 * @see vblf_peek_object, vblf_read_object_secure, vblf_write_object
 */
typedef struct vblf_object_header_base {
    uint8_t signature[VBLF_OBJ_SIGNATURE_SIZE]; /**< Four signature bytes, without a C NUL. */
    uint16_t header_size;                       /**< Serialized header size in bytes. */
    uint16_t header_version;                    /**< Serialized object header version. */
    uint32_t object_size; /**< Serialized header and body size, excluding alignment padding. */
    vblf_obj_type_t object_type; /**< Original BLF object type, including unsupported values. */
} vblf_object_header_base_t;

/**
 * @brief Opaque, library-owned file handle.
 * @note Separate handles may be used concurrently. Calls on one handle require
 * external synchronization, including object release and handle close.
 * @see vblf_create_file, vblf_close_handle
 */
typedef struct vblf_handle vblf_handle_t;

/**
 * @brief Open a BLF file for reading or writing.
 *
 * @param[in] path_utf8 NUL-terminated UTF-8 file path.
 * @param[in] mode Either VBLF_OPEN_READ or VBLF_OPEN_WRITE; do not combine modes.
 * @param[out] out_handle Required output pointer receiving the library-owned handle.
 *
 * @retval VBLF_OK The file was opened successfully.
 * @retval VBLF_ERROR_INVALID_ARGUMENT A required argument is invalid.
 * @retval VBLF_ERROR_INVALID_MODE The open mode is unsupported.
 * @return Another negative vblf_result_t on failure.
 *
 * @post If @p out_handle is valid, @c *out_handle is NULL on failure.
 * @note Write mode creates a new file or truncates an existing file.
 * Close any previous handle before reusing its output slot.
 * @see vblf_close_handle
 */
VBLF_API vblf_create_file(const char *path_utf8, vblf_open_mode_t mode, vblf_handle_t **out_handle);

/**
 * @brief Finalize pending writes and release a file handle.
 *
 * @param[in] handle Handle to release. NULL is allowed and performs no operation.
 *
 * @retval VBLF_OK Finalization succeeded, or @p handle was NULL.
 * @return A negative vblf_result_t if finalization failed.
 *
 * @pre This call must not overlap any other operation on the same handle.
 * Release all outstanding read-object dynamic members before closing the handle.
 * @post A non-NULL handle is released even if finalization fails. It must not
 * be reused or closed again.
 * @see vblf_free_object
 */
VBLF_API vblf_close_handle(vblf_handle_t *handle);

/**
 * @brief Inspect the next object header without consuming the object.
 *
 * @param[in] handle File handle opened with VBLF_OPEN_READ.
 * @param[out] out_header Caller-owned storage receiving the common base header.
 *
 * @retval VBLF_OK The next object header is available.
 * @retval VBLF_EOF The input stream reached a clean end.
 * @retval VBLF_ERROR_INVALID_ARGUMENT A required argument is invalid.
 * @retval VBLF_ERROR_INVALID_MODE The handle was not opened for reading.
 * @return Another negative vblf_result_t on failure.
 *
 * @post The output remains unchanged at EOF or on error.
 * @note Repeated peeks report the same object until a successful read or skip.
 * The object type need not have a decoder. No payload memory is transferred to
 * the caller, and the returned header does not require vblf_free_object().
 * @see vblf_read_object_secure, vblf_skip_object
 */
VBLF_API vblf_peek_object(vblf_handle_t *handle, vblf_object_header_base_t *out_header);

/**
 * @brief Read the next object into caller-owned storage.
 *
 * @param[in] handle File handle opened with VBLF_OPEN_READ.
 * @param[out] out_object Address of the actual base header member in storage for
 * the concrete object type reported by vblf_peek_object().
 * @param[in] object_capacity Available storage in bytes, normally sizeof the
 * complete concrete C structure, including its header and alignment padding.
 * This capacity excludes separately allocated payload bytes.
 *
 * @retval VBLF_OK One object was read and consumed.
 * @retval VBLF_EOF The input stream reached a clean end.
 * @retval VBLF_ERROR_INVALID_ARGUMENT A required argument is invalid.
 * @retval VBLF_ERROR_INVALID_MODE The handle was not opened for reading.
 * @retval VBLF_ERROR_BUFFER_TOO_SMALL The concrete C structure does not fit.
 * @retval VBLF_ERROR_UNSUPPORTED_OBJECT The object type has no decoder.
 * @retval VBLF_ERROR_UNSUPPORTED_HEADER The object header version is unsupported.
 * @return Another negative vblf_result_t on failure.
 *
 * @pre The storage must not contain dynamic members from an earlier read that
 * have not yet been released. Free those members before reusing the storage.
 * @post The output remains unchanged at EOF or on error.
 * @post Buffer-too-small, unsupported-object and unsupported-header errors leave
 * the object pending. The caller may retry or skip it. Other errors do not
 * guarantee a recoverable stream position.
 * @note The library allocates variable-length members. They remain valid across
 * later reads until released through vblf_free_object() on the same handle.
 * The caller-owned structure itself is never allocated or freed by the library.
 * @note Concrete object layouts will be introduced separately. A serialized
 * object_size is not a valid substitute for the C storage capacity.
 * @see vblf_peek_object, vblf_skip_object, vblf_free_object
 */
VBLF_API vblf_read_object_secure(vblf_handle_t *handle, vblf_object_header_base_t *out_object,
                                 size_t object_capacity);

/**
 * @brief Skip the next object without decoding its payload.
 *
 * @param[in] handle File handle opened with VBLF_OPEN_READ.
 *
 * @retval VBLF_OK One object was skipped and consumed.
 * @retval VBLF_EOF The input stream reached a clean end.
 * @retval VBLF_ERROR_INVALID_ARGUMENT The handle is invalid.
 * @retval VBLF_ERROR_INVALID_MODE The handle was not opened for reading.
 * @return Another negative vblf_result_t on failure.
 *
 * @note Unsupported object types and extended header versions can be skipped
 * when their base header and declared boundaries are valid. Corrupt framing or
 * compressed input can still cause an error. No object storage is required.
 * @see vblf_peek_object, vblf_read_object_secure
 */
VBLF_API vblf_skip_object(vblf_handle_t *handle);

/**
 * @brief Accept one object for writing.
 *
 * @param[in] handle File handle opened with VBLF_OPEN_WRITE.
 * @param[in] object Address of the actual base header member in a concrete
 * object. Dynamic member pointers remain owned by the caller.
 * @param[in] object_capacity Available storage in bytes, normally sizeof the
 * complete concrete C structure. This capacity excludes pointed-to payloads
 * and is independent of the serialized object_size field.
 *
 * @retval VBLF_OK The object was accepted for writing.
 * @retval VBLF_ERROR_INVALID_ARGUMENT A required argument is invalid.
 * @retval VBLF_ERROR_INVALID_MODE The handle was not opened for writing.
 * @retval VBLF_ERROR_BUFFER_TOO_SMALL The concrete C structure does not fit.
 * @retval VBLF_ERROR_UNSUPPORTED_OBJECT The object type has no encoder.
 * @retval VBLF_ERROR_UNSUPPORTED_HEADER The object header version is unsupported.
 * @return Another negative vblf_result_t on failure.
 *
 * @pre The concrete layout must match the object's type and header version.
 * Each dynamic member must point to readable data for its declared length.
 * @post The library retains neither the object pointer nor its dynamic member
 * pointers. Input storage may be changed or released after this call returns.
 * @note Success does not imply a completed file flush. vblf_close_handle() reports
 * finalization failures.
 * @note Concrete layouts, write-field initialization and supported types will
 * be introduced separately. C structures must not be written as raw file bytes.
 * @see vblf_close_handle
 */
VBLF_API vblf_write_object(vblf_handle_t *handle, const vblf_object_header_base_t *object,
                           size_t object_capacity);

/**
 * @brief Release dynamic members allocated by a successful object read.
 *
 * @param[in] handle Still-open read handle that originally read the object.
 * May be NULL only when @p object is NULL.
 * @param[in,out] object Base header member of the original caller-owned object
 * structure. NULL is allowed and performs no operation.
 *
 * @retval VBLF_OK Dynamic members were released, no dynamic members required
 * release, or @p object was NULL.
 * @retval VBLF_ERROR_INVALID_ARGUMENT A required argument is invalid.
 * @retval VBLF_ERROR_INVALID_MODE The handle was not opened for reading.
 * @return Another negative vblf_result_t on failure.
 *
 * @pre A non-NULL object must come from a successful vblf_read_object_secure()
 * call on this handle. Do not replace its type, header version or dynamic
 * members before release. A shallow copy does not acquire ownership and must
 * not be freed independently. A peek header or caller-created write object is
 * not a read object and must not be passed here.
 * @post On success, dynamic pointers and their lengths are cleared. Repeated
 * release of the same unmodified structure is allowed while the handle is open.
 * The caller-owned structure itself is not released, and its base header is kept.
 * @note Release all read objects before closing their handle. Do not use free(),
 * delete or another allocator to release library-allocated members.
 * @see vblf_read_object_secure, vblf_close_handle
 */
VBLF_API vblf_free_object(vblf_handle_t *handle, vblf_object_header_base_t *object);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* VBLF_H_INCLUDED */
