// Video/VideoOverlay.hpp
#pragma once
#include <opencv2/opencv.hpp>
#include <string>

namespace Video
{

    class VideoOverlay
    {
    public:
        VideoOverlay();

        void AddTimestamp(cv::Mat &frame);
        void AddFPS(cv::Mat &frame, double fps);
        void AddCustomText(cv::Mat &frame, const std::string &text, cv::Point position);

        void SetTimestampEnabled(bool enabled) { m_timestamp_enabled = enabled; }
        void SetFPSEnabled(bool enabled) { m_fps_enabled = enabled; }

    private:
        std::string GetCurrentTimeString();

        bool m_timestamp_enabled{true};
        bool m_fps_enabled{true};

        cv::Scalar m_text_color{0, 255, 0}; // Green
        int m_font_face{cv::FONT_HERSHEY_SIMPLEX};
        double m_font_scale{0.6};
        int m_thickness{2};
    };

} // namespace Video