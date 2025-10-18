// VideoConfig.hpp
#pragma once
#include <string>
#include <chrono>

namespace Video
{

    enum class PixelFormat
    {
        RGB24,
        BGR24,
        YUV420,
        YUYV,
        NV12
    };

    struct CaptureConfig
    {
        std::string device_path = "/dev/video0";
        uint32_t width = 640;
        uint32_t height = 480;
        uint32_t fps = 30;
        PixelFormat format = PixelFormat::YUYV;
        bool enable_timestamp = true;
        bool enable_fps_overlay = true;
    };

    enum class GStreamerEncoder
    {
        MJPEG,
        RAW
    };

    enum class StreamProtocol
    {
        UDP,
        FILE
    };

    struct EncodingConfig
    {
        GStreamerEncoder encoder = GStreamerEncoder::MJPEG;
        uint32_t jpeg_quality = 80; // JPEG quality (0-100)
        uint32_t fps = 30;
    };

    struct StreamConfig
    {
        StreamProtocol protocol = StreamProtocol::UDP;
        std::string host = "127.0.0.1";
        uint32_t port = 5000;
        std::string output_file = "output.avi"; // For FILE protocol
        bool enable_file_recording = false;
    };

} // namespace Video