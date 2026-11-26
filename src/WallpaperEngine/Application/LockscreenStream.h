#pragma once

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace WallpaperEngine::Application {
/** Short-lived, local wallpaper-only feed for a session-lock client. */
class LockscreenStream {
public:
    using Clock = std::chrono::steady_clock;

    bool renew (std::string_view value, Clock::time_point now) {
        int requested = 0;
        const auto [end, error] = std::from_chars (value.data (), value.data () + value.size (), requested);
        if (error != std::errc {} || end != value.data () + value.size () || requested < 0 || requested > 60)
            return false;
        m_fps = requested;
        m_expires = now + std::chrono::seconds (3);
        return true;
    }

    int fps (Clock::time_point now) const { return now < m_expires ? m_fps : 0; }

    static std::pair<int, int> frameSize (int width, int height) {
        if (width <= 0 || height <= 0) return { 0, 0 };
        const int scaledWidth = std::min (width, 960);
        return { scaledWidth, std::max (1, static_cast<int> (std::lround (double (height) * scaledWidth / width))) };
    }

    /** Encode BGRA rows in place. BI_RGB uses a negative height for top-down rows. */
    static bool writeFrame (const std::string& directory, const std::string& connector,
                            int width, int height, const std::vector<uint8_t>& pixels, bool bottomUp = true) {
        if (connector.empty () || connector.find_first_not_of (
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.") != std::string::npos
            || width <= 0 || height <= 0 || uint64_t (width) * height * 4 != pixels.size ()
            || pixels.size () > UINT32_MAX - 54)
            return false;
        if (mkdir (directory.c_str (), 0700) != 0 && errno != EEXIST) return false;
        const int dir = open (directory.c_str (), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (dir < 0) return false;
        struct stat info {};
        if (fstat (dir, &info) != 0 || info.st_uid != getuid () || (info.st_mode & 0777) != 0700) {
            close (dir);
            return false;
        }
        const std::string filename = connector + ".bmp";
        const std::string temporary = "." + connector + "." + std::to_string (getpid ()) + ".tmp";
        const int file = openat (dir, temporary.c_str (), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
        if (file < 0) {
            close (dir);
            return false;
        }
        std::array<uint8_t, 54> header {};
        const auto integer = [&] (size_t offset, uint32_t value) {
            for (int byte = 0; byte < 4; ++byte) header[offset + byte] = (value >> (byte * 8)) & 255;
        };
        header[0] = 'B'; header[1] = 'M';
        integer (2, 54 + pixels.size ());
        integer (10, 54);
        integer (14, 40);
        integer (18, width);
        integer (22, static_cast<uint32_t> (bottomUp ? height : -height));
        header[26] = 1;
        header[28] = 32;
        integer (34, pixels.size ());
        const auto writeAll = [file] (const uint8_t* data, size_t remaining) {
            while (remaining > 0) {
                const ssize_t written = write (file, data, remaining);
                if (written < 0 && errno == EINTR) continue;
                if (written <= 0) return false;
                data += written;
                remaining -= written;
            }
            return true;
        };
        bool success = writeAll (header.data (), header.size ()) && writeAll (pixels.data (), pixels.size ());
        if (close (file) != 0) success = false;
        if (success) success = renameat (dir, temporary.c_str (), dir, filename.c_str ()) == 0;
        if (!success) unlinkat (dir, temporary.c_str (), 0);
        close (dir);
        return success;
    }

private:
    int m_fps = 0;
    Clock::time_point m_expires {};
};
}
