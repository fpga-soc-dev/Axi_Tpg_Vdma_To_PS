// Video/VideoCapture.cpp
#include "VideoCapture.hpp"
#include <spdlog/spdlog.h>

namespace Video
{

    VideoCapture::VideoCapture(const std::string &device)
        : m_device(device)
    {
    }

    VideoCapture::~VideoCapture()
    {
        Stop();
    }

    bool VideoCapture::Initialize(int width, int height, int fps)
    {
        m_width = width;
        m_height = height;
        m_fps = fps;

        // Open V4L2 device
        if (!m_cap.open(m_device, cv::CAP_V4L2))
        {
            SPDLOG_ERROR("Failed to open video device: {}", m_device);
            return false;
        }

        m_cap.set(cv::CAP_PROP_FRAME_WIDTH, width);
        m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);
        m_cap.set(cv::CAP_PROP_FPS, fps);

        SPDLOG_INFO("Video capture initialized: {}x{} @ {}fps", width, height, fps);
        return true;
    }

    bool VideoCapture::Start()
    {
        if (m_capturing)
        {
            return true;
        }

        if (!m_cap.isOpened())
        {
            SPDLOG_ERROR("Video device not opened");
            return false;
        }

        m_should_stop = false;
        m_capture_thread = std::thread(&VideoCapture::CaptureLoop, this);
        m_capturing = true;

        SPDLOG_INFO("Video capture started");
        return true;
    }

    bool VideoCapture::Stop()
    {
        if (!m_capturing)
            return true;

        m_should_stop = true;
        if (m_capture_thread.joinable())
        {
            m_capture_thread.join();
        }
        m_capturing = false;

        SPDLOG_INFO("Video capture stopped");
        return true;
    }

    void VideoCapture::CaptureLoop()
    {
        cv::Mat frame;

        while (!m_should_stop)
        {
            if (!m_cap.read(frame))
            {
                SPDLOG_WARN("Failed to read frame");
                std::this_thread::sleep_for(std::chrono::milliseconds(33));
                continue;
            }

            if (m_callback)
            {
                m_callback(frame);
            }
        }
    }

} // namespace Video