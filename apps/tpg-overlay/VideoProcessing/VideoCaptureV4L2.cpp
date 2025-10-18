#include "VideoCaptureV4L2.hpp"

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <poll.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <utility>

namespace Video
{

    VideoCaptureV4L2::VideoCaptureV4L2() : m_cfg{} {}
    VideoCaptureV4L2::VideoCaptureV4L2(const Config &cfg) : m_cfg(cfg) {}
    VideoCaptureV4L2::~VideoCaptureV4L2()
    {
        stop();
        close();
    }

    VideoCaptureV4L2::VideoCaptureV4L2(VideoCaptureV4L2 &&o) noexcept { *this = std::move(o); }
    VideoCaptureV4L2 &VideoCaptureV4L2::operator=(VideoCaptureV4L2 &&o) noexcept
    {
        if (this == &o)
            return *this;
        stop();
        close();
        m_cfg = std::move(o.m_cfg);
        m_fd = std::exchange(o.m_fd, -1);
        m_width = std::exchange(o.m_width, 0);
        m_height = std::exchange(o.m_height, 0);
        m_fps = std::exchange(o.m_fps, 0);
        m_strideBytes = std::exchange(o.m_strideBytes, 0);
        m_buffers = std::move(o.m_buffers);
        m_callback = std::move(o.m_callback);
        m_running.store(o.m_running.load(std::memory_order_relaxed), std::memory_order_relaxed);
        return *this;
    }

    int VideoCaptureV4L2::xioctl_(int fd, unsigned long req, void *arg)
    {
        int r;
        do
        {
            r = ::ioctl(fd, req, arg);
        } while (r == -1 && errno == EINTR);
        return r;
    }

    bool VideoCaptureV4L2::setFormat_()
    {
        // Set desired format (mplane, 1 plane, BGR24)
        v4l2_format fmt{};
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        fmt.fmt.pix_mp.width = m_cfg.width;
        fmt.fmt.pix_mp.height = m_cfg.height;
        fmt.fmt.pix_mp.pixelformat = m_cfg.pixfmt; // BGR3
        fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
        fmt.fmt.pix_mp.num_planes = 1;

        if (xioctl_(m_fd, VIDIOC_S_FMT, &fmt) < 0)
        {
            SPDLOG_ERROR("VIDIOC_S_FMT failed: {}", strerror(errno));
            return false;
        }

        // Read back negotiated fmt
        if (xioctl_(m_fd, VIDIOC_G_FMT, &fmt) < 0)
        {
            SPDLOG_WARN("VIDIOC_G_FMT failed: {}", strerror(errno));
            m_width = m_cfg.width;
            m_height = m_cfg.height;
            m_strideBytes = static_cast<uint32_t>(m_width * 3);
        }
        else
        {
            m_width = fmt.fmt.pix_mp.width;
            m_height = fmt.fmt.pix_mp.height;
            if (fmt.fmt.pix_mp.num_planes >= 1)
            {
                m_strideBytes = fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
            }
            if (m_strideBytes == 0)
                m_strideBytes = static_cast<uint32_t>(m_width * 3);
        }

        SPDLOG_INFO("Negotiated V4L2 fmt: {}x{} BGR24, stride={} bytes", m_width, m_height, m_strideBytes);
        return true;
    }

    bool VideoCaptureV4L2::setFps_()
    {
        // Many capture nodes (framebuffer write) don't support S_PARM. Treat as non-fatal.
        v4l2_streamparm parm{};
        parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        parm.parm.capture.timeperframe.numerator = 1;
        parm.parm.capture.timeperframe.denominator = m_cfg.fps;

        if (xioctl_(m_fd, VIDIOC_S_PARM, &parm) < 0)
        {
            if (errno == ENOTTY || errno == EINVAL)
            {
                SPDLOG_INFO("Device does not support setting FPS (S_PARM). Using source rate.");
            }
            else
            {
                SPDLOG_WARN("VIDIOC_S_PARM failed: {}", strerror(errno));
            }
        }

        // Try to read back (may be unsupported; ignore if fails)
        if (xioctl_(m_fd, VIDIOC_G_PARM, &parm) == 0 &&
            parm.parm.capture.timeperframe.numerator &&
            parm.parm.capture.timeperframe.denominator)
        {
            m_fps = parm.parm.capture.timeperframe.denominator /
                    parm.parm.capture.timeperframe.numerator;
        }
        else
        {
            m_fps = m_cfg.fps; // best-effort for logs/timestamps
        }

        SPDLOG_INFO("Effective FPS: {}", m_fps);
        return true;
    }

    bool VideoCaptureV4L2::requestBuffers_()
    {
        v4l2_requestbuffers req{};
        req.count = m_cfg.num_buffers;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        req.memory = V4L2_MEMORY_MMAP;

        if (xioctl_(m_fd, VIDIOC_REQBUFS, &req) < 0)
        {
            SPDLOG_ERROR("VIDIOC_REQBUFS failed: {}", strerror(errno));
            return false;
        }
        if (req.count < 2)
        {
            SPDLOG_ERROR("Insufficient buffers granted: {}", req.count);
            return false;
        }

        m_buffers.resize(req.count);
        return true;
    }

    bool VideoCaptureV4L2::mapBuffers_()
    {
        for (uint32_t i = 0; i < m_buffers.size(); ++i)
        {
            v4l2_buffer buf{};
            v4l2_plane planes[VIDEO_MAX_PLANES]{};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            buf.length = 1;
            buf.m.planes = planes;

            if (xioctl_(m_fd, VIDIOC_QUERYBUF, &buf) < 0)
            {
                SPDLOG_ERROR("VIDIOC_QUERYBUF[{}] failed: {}", i, strerror(errno));
                return false;
            }

            void *addr = ::mmap(nullptr, planes[0].length, PROT_READ | PROT_WRITE,
                                MAP_SHARED, m_fd, planes[0].m.mem_offset);
            if (addr == MAP_FAILED)
            {
                SPDLOG_ERROR("mmap[{}] failed: {}", i, strerror(errno));
                return false;
            }
            m_buffers[i].addr = addr;
            m_buffers[i].length = planes[0].length;
        }
        return true;
    }

    bool VideoCaptureV4L2::queueAll_()
    {
        for (uint32_t i = 0; i < m_buffers.size(); ++i)
        {
            v4l2_buffer buf{};
            v4l2_plane planes[VIDEO_MAX_PLANES]{};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            buf.length = 1;
            buf.m.planes = planes;

            // IMPORTANT for mplane: provide plane length when (re)queuing
            planes[0].length = static_cast<uint32_t>(m_buffers[i].length);
            planes[0].bytesused = 0;

            if (xioctl_(m_fd, VIDIOC_QBUF, &buf) < 0)
            {
                SPDLOG_ERROR("VIDIOC_QBUF[{}] failed: {}", i, strerror(errno));
                return false;
            }
        }
        return true;
    }

    bool VideoCaptureV4L2::streamOn_()
    {
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        if (xioctl_(m_fd, VIDIOC_STREAMON, &type) < 0)
        {
            SPDLOG_ERROR("VIDIOC_STREAMON failed: {}", strerror(errno));
            return false;
        }
        return true;
    }

    void VideoCaptureV4L2::streamOff_()
    {
        if (m_fd < 0)
            return;
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        if (xioctl_(m_fd, VIDIOC_STREAMOFF, &type) < 0)
        {
            SPDLOG_WARN("VIDIOC_STREAMOFF: {}", strerror(errno));
        }
    }

    void VideoCaptureV4L2::unmapBuffers_()
    {
        for (auto &b : m_buffers)
        {
            if (b.addr)
                ::munmap(b.addr, b.length);
            b = {};
        }
        m_buffers.clear();
    }

    bool VideoCaptureV4L2::open()
    {
        if (isOpen())
            return true;

        m_fd = ::open(m_cfg.device.c_str(), m_cfg.open_flags);
        if (m_fd < 0)
        {
            SPDLOG_ERROR("open({}): {}", m_cfg.device, strerror(errno));
            return false;
        }

        if (!setFormat_())
        {
            close();
            return false;
        }
        (void)setFps_(); // non-fatal if not supported
        if (!requestBuffers_())
        {
            close();
            return false;
        }
        if (!mapBuffers_())
        {
            close();
            return false;
        }
        if (!queueAll_())
        {
            close();
            return false;
        }
        if (!streamOn_())
        {
            close();
            return false;
        }

        SPDLOG_INFO("V4L2 opened: {} ({}x{} @ ~{} fps)", m_cfg.device, m_width, m_height, m_fps);
        return true;
    }

    void VideoCaptureV4L2::close()
    {
        if (m_fd < 0)
            return;
        streamOff_();
        unmapBuffers_();
        ::close(m_fd);
        m_fd = -1;
    }

    bool VideoCaptureV4L2::start()
    {
        if (!isOpen())
        {
            SPDLOG_ERROR("start(): device not open");
            return false;
        }
        if (isRunning())
            return true;

        m_running.store(true, std::memory_order_relaxed);
        m_thread = std::jthread([this](std::stop_token st)
                                { captureLoop_(st); });
        SPDLOG_INFO("Capture thread started");
        return true;
    }

    void VideoCaptureV4L2::stop()
    {
        if (!isRunning())
            return;
        m_running.store(false, std::memory_order_relaxed);
        if (m_thread.joinable())
        {
            m_thread.request_stop();
            m_thread.join();
        }
        SPDLOG_INFO("Capture thread stopped");
    }

    void VideoCaptureV4L2::captureLoop_(std::stop_token st)
    {
        const int fd = m_fd;
        struct pollfd pfd{.fd = fd, .events = POLLIN | POLLPRI, .revents = 0};

        while (!st.stop_requested())
        {

            // Non-blocking quick path: try DQBUF immediately
            {
                v4l2_buffer buf{};
                v4l2_plane planes[VIDEO_MAX_PLANES]{};
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.length = 1;
                buf.m.planes = planes;
                // Provide plane length as a safe default (driver overwrites fields)
                planes[0].length = m_buffers.empty() ? 0u : static_cast<uint32_t>(m_buffers[0].length);

                if (xioctl_(fd, VIDIOC_DQBUF, &buf) == 0)
                {
                    const uint32_t idx = buf.index;
                    const uint32_t stride = m_strideBytes ? m_strideBytes : (m_width * 3);
                    cv::Mat view(m_height, m_width, CV_8UC3, m_buffers[idx].addr, stride);
                    if (m_callback)
                        m_callback(view);

                    // Re-queue with plane length set
                    planes[0].length = static_cast<uint32_t>(m_buffers[idx].length);
                    planes[0].bytesused = 0;
                    if (xioctl_(fd, VIDIOC_QBUF, &buf) < 0)
                    {
                        SPDLOG_ERROR("VIDIOC_QBUF: {}", strerror(errno));
                        break;
                    }
                    continue; // next iteration
                }
                if (errno != EAGAIN)
                {
                    SPDLOG_ERROR("VIDIOC_DQBUF: {}", strerror(errno));
                    break;
                }
            }

            // Block waiting for a frame
            pfd.revents = 0;
            int r = ::poll(&pfd, 1, /*ms*/ 2000);
            if (r == 0)
            {
                SPDLOG_WARN("poll() timeout");
                continue;
            }
            if (r < 0)
            {
                if (errno == EINTR)
                    continue;
                SPDLOG_ERROR("poll(): {}", strerror(errno));
                break;
            }

            // Dequeue after readiness
            v4l2_buffer buf{};
            v4l2_plane planes[VIDEO_MAX_PLANES]{};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.length = 1;
            buf.m.planes = planes;
            planes[0].length = m_buffers.empty() ? 0u : static_cast<uint32_t>(m_buffers[0].length);

            if (xioctl_(fd, VIDIOC_DQBUF, &buf) < 0)
            {
                if (errno == EAGAIN)
                    continue; // spurious wakeup
                SPDLOG_ERROR("VIDIOC_DQBUF(after poll): {}", strerror(errno));
                break;
            }

            const uint32_t idx = buf.index;
            const uint32_t stride = m_strideBytes ? m_strideBytes : (m_width * 3);
            cv::Mat view(m_height, m_width, CV_8UC3, m_buffers[idx].addr, stride);
            if (m_callback)
                m_callback(view);

            // Re-queue with plane length set
            planes[0].length = static_cast<uint32_t>(m_buffers[idx].length);
            planes[0].bytesused = 0;
            if (xioctl_(fd, VIDIOC_QBUF, &buf) < 0)
            {
                SPDLOG_ERROR("VIDIOC_QBUF: {}", strerror(errno));
                break;
            }
        }
    }

} // namespace Video
