// Video/VideoStreamer.cpp
#include "VideoStreamer.hpp"
#include <spdlog/spdlog.h>

namespace Video
{

    VideoStreamer::VideoStreamer() = default;

    VideoStreamer::~VideoStreamer()
    {
        Stop();
    }

    bool VideoStreamer::Initialize(int width, int height, int fps,
                                   const std::string &host, int port, GStreamerEncoder encoder)
    {
        m_width = width;
        m_height = height;
        m_fps = fps;
        m_host = host;
        m_port = port;

        std::string pipeline = BuildGStreamerPipeline(width, height, fps, host, port, encoder);
        SPDLOG_INFO("GStreamer pipeline: {}", pipeline);

        // Open GStreamer pipeline through OpenCV
        if (!m_writer.open(pipeline, cv::CAP_GSTREAMER, 0, fps, cv::Size(width, height), true))
        {
            SPDLOG_ERROR("Failed to open GStreamer pipeline");
            return false;
        }

        SPDLOG_INFO("Video streamer initialized");
        return true;
    }

    std::string VideoStreamer::BuildGStreamerPipeline(int width, int height, int fps,
                                                      const std::string &host, int port, GStreamerEncoder encoder)
    {

        if (encoder == GStreamerEncoder::MJPEG)
        {
            // MJPEG encoding pipeline
            return fmt::format(
                "appsrc ! "
                "videoconvert ! "
                "jpegenc quality=80 ! "
                "rtpjpegpay ! "
                "udpsink host={} port={}",
                host, port);
        }
        else if (encoder == GStreamerEncoder::RAW)
        {
            return fmt::format(
                "appsrc do-timestamp=true ! "
                "video/x-raw,format=BGR,width={},height={},framerate={}/1 ! "
                "queue max-size-buffers=0 ! "
                "rtpvrawpay pt=96 mtu=1200 ! "
                "udpsink host={} port={} sync=true buffer-size=524288",
                width, height, fps, host, port);
        }
        else
        {
            SPDLOG_WARN("Unknown encoder type");
        }

        // Simple MJPEG over UDP pipeline - lightest encoding
        return fmt::format(
            "appsrc ! "
            "videoconvert ! "
            "jpegenc quality=80 ! "
            "rtpjpegpay ! "
            "udpsink host={} port={}",
            host, port);

        // Alternative: Raw video over UDP (no encoding, highest performance)
        // return fmt::format(
        //     "appsrc ! "
        //     "videoconvert ! "
        //     "udpsink host={} port={}",
        //     host, port
        // );
    }

    bool VideoStreamer::Start()
    {
        if (!m_writer.isOpened())
        {
            SPDLOG_ERROR("Video writer not initialized");
            return false;
        }

        m_streaming = true;
        SPDLOG_INFO("Video streaming started to {}:{}", m_host, m_port);
        return true;
    }

    bool VideoStreamer::Stop()
    {
        m_streaming = false;
        if (m_writer.isOpened())
        {
            m_writer.release();
        }
        SPDLOG_INFO("Video streaming stopped");
        return true;
    }

    bool VideoStreamer::StreamFrame(const cv::Mat &frame)
    {
        if (!m_streaming || !m_writer.isOpened())
        {
            return false;
        }

        try
        {
            m_writer.write(frame);
            return true;
        }
        catch (const std::exception &e)
        {
            SPDLOG_ERROR("Failed to stream frame: {}", e.what());
            return false;
        }
    }

} // namespace Video