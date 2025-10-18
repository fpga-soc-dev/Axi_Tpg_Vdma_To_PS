#pragma once

#include <opencv2/opencv.hpp>
#include <functional>
#include <atomic>
#include <thread>

namespace Video
{

    class VideoCapture
    {
    public:
        using FrameCallback = std::function<void(cv::Mat &frame)>;

        VideoCapture(const std::string &device = "/dev/video0");
        ~VideoCapture();

        bool Initialize(int width, int height, int fps);
        bool Start();
        bool Stop();

        void SetFrameCallback(FrameCallback callback) { m_callback = callback; }

        bool IsCapturing() const { return m_capturing; }

    private:
        void CaptureLoop();

        cv::VideoCapture m_cap;
        FrameCallback m_callback;
        std::atomic<bool> m_capturing{false};
        std::atomic<bool> m_should_stop{false};
        std::thread m_capture_thread;

        std::string m_device;
        int m_width, m_height, m_fps;
    };

} // namespace Video