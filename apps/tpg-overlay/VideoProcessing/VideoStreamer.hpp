
#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <atomic>
#include "VideoConfig.hpp"

namespace Video
{

    class VideoStreamer
    {
    public:
        VideoStreamer();
        ~VideoStreamer();

        // Initialize with GStreamer pipeline
        bool Initialize(int width, int height, int fps,
                        const std::string &host, int port, GStreamerEncoder encoder);

        bool Start();
        bool Stop();

        // Stream a frame
        bool StreamFrame(const cv::Mat &frame);

        bool IsStreaming() const { return m_streaming; }

    private:
        std::string BuildGStreamerPipeline(int width, int height, int fps,
                                           const std::string &host, int port, GStreamerEncoder encoder);

        cv::VideoWriter m_writer;
        std::atomic<bool> m_streaming{false};

        int m_width, m_height, m_fps;
        std::string m_host;
        int m_port;
    };

} // namespace Video