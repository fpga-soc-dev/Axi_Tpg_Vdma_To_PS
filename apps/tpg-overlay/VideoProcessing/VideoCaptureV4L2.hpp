#pragma once

#include <opencv2/core.hpp>
#include <spdlog/spdlog.h>

#include <linux/videodev2.h>
#include <fcntl.h> // O_RDWR, O_NONBLOCK
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <cstdint>

namespace Video
{

    class VideoCaptureV4L2
    {
    public:
        using FrameCallback = std::function<void(const cv::Mat &)>;

        struct Config
        {
            std::string device = "/dev/video0";
            int width = 640;
            int height = 480;
            int fps = 30;                         // Best-effort (often controlled by source)
            uint32_t pixfmt = V4L2_PIX_FMT_BGR24; // 'BGR3'
            uint32_t num_buffers = 4;             // >= 2
            int open_flags = O_RDWR;              // start with blocking; switch to O_NONBLOCK if you like
        };

        VideoCaptureV4L2(); // default config
        explicit VideoCaptureV4L2(const Config &cfg);
        ~VideoCaptureV4L2();

        VideoCaptureV4L2(const VideoCaptureV4L2 &) = delete;
        VideoCaptureV4L2 &operator=(const VideoCaptureV4L2 &) = delete;
        VideoCaptureV4L2(VideoCaptureV4L2 &&) noexcept;
        VideoCaptureV4L2 &operator=(VideoCaptureV4L2 &&) noexcept;

        bool open();  // open + S_FMT + (optionally S_PARM) + REQBUFS + MMAP + QBUF + STREAMON
        void close(); // STREAMOFF + unmap + close

        bool start(); // spawn capture thread
        void stop();  // join thread

        bool isOpen() const noexcept { return m_fd >= 0; }
        bool isRunning() const noexcept { return m_running.load(std::memory_order_relaxed); }

        void setCallback(FrameCallback cb) { m_callback = std::move(cb); }

        // Negotiated properties after open()
        int width() const noexcept { return m_width; }
        int height() const noexcept { return m_height; }
        int fps() const noexcept { return m_fps; }
        uint32_t stride() const noexcept { return m_strideBytes; }

    private:
        struct Buffer
        {
            void *addr = nullptr;
            size_t length = 0;
        };

        // low-level steps
        bool setFormat_();
        bool setFps_(); // non-fatal if ENOTTY
        bool requestBuffers_();
        bool mapBuffers_();
        bool queueAll_();
        bool streamOn_();
        void streamOff_();
        void unmapBuffers_();

        static int xioctl_(int fd, unsigned long req, void *arg);

        void captureLoop_(std::stop_token st);

    private:
        Config m_cfg{};
        int m_fd{-1};

        int m_width{0};
        int m_height{0};
        int m_fps{0};
        uint32_t m_strideBytes{0};

        std::vector<Buffer> m_buffers;
        std::jthread m_thread;
        std::atomic<bool> m_running{false};
        FrameCallback m_callback;
    };

} // namespace Video
