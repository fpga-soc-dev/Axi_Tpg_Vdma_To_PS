
#include "VideoOverlay.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace Video
{

    VideoOverlay::VideoOverlay() = default;

    void VideoOverlay::AddTimestamp(cv::Mat &frame)
    {
        if (!m_timestamp_enabled)
            return;

        std::string timestamp = GetCurrentTimeString();
        cv::Point position(10, 30); // Top-left corner

        cv::putText(frame, timestamp, position, m_font_face, m_font_scale,
                    m_text_color, m_thickness);
    }

    void VideoOverlay::AddFPS(cv::Mat &frame, double fps)
    {
        if (!m_fps_enabled)
            return;

        std::string fps_text = "FPS: " + std::to_string(static_cast<int>(fps));
        cv::Point position(10, frame.rows - 20); // Bottom-left corner

        cv::putText(frame, fps_text, position, m_font_face, m_font_scale,
                    m_text_color, m_thickness);
    }

    void VideoOverlay::AddCustomText(cv::Mat &frame, const std::string &text, cv::Point position)
    {
        cv::putText(frame, text, position, m_font_face, m_font_scale,
                    m_text_color, m_thickness);
    }

    std::string VideoOverlay::GetCurrentTimeString()
    {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);

        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

} // namespace Video