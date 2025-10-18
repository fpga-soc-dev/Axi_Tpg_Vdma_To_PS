#pragma once
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <opencv2/core.hpp>
#include <string>
#include <mutex>
#include <stdexcept>

class UdpVideoSender
{
public:
    struct Config
    {
        std::string host = "192.168.1.20";
        int port = 5004;
        int width = 640;
        int height = 480;
        int fps = 60;
        int mtu = 1200;
        bool sync = true;           // udpsink sync
        int socket_buffer = 524288; // udpsink buffer-size
        bool convert_to_rgb = true; // keep true for rtpvrawpay compatibility
    };

    explicit UdpVideoSender(const Config &cfg) : m_cfg(cfg)
    {
        std::scoped_lock lk(m_mtx);
        gst_init(nullptr, nullptr);

        // Build pipeline string
        // We feed BGR from OpenCV; convert to RGB before rtpvrawpay
        std::string desc =
            "appsrc name=mysrc is-live=true format=time do-timestamp=true block=true "
            "caps=video/x-raw,format=RGB,width=" +
            std::to_string(m_cfg.width) +
            ",height=" + std::to_string(m_cfg.height) +
            ",framerate=" + std::to_string(m_cfg.fps) + "/1 ! "
                                                        "queue max-size-buffers=0 ! "
                                                        "rtpvrawpay pt=96 mtu=" +
            std::to_string(m_cfg.mtu) + " ! "
                                        "udpsink host=" +
            m_cfg.host + " port=" + std::to_string(m_cfg.port) +
            " sync=true buffer-size=" + std::to_string(m_cfg.socket_buffer);

        GError *err = nullptr;
        m_pipeline = gst_parse_launch(desc.c_str(), &err);
        if (!m_pipeline)
        {
            std::string msg = err ? err->message : "gst_parse_launch failed";
            if (err)
                g_error_free(err);
            throw std::runtime_error(msg);
        }

        m_appsrc = GST_APP_SRC(gst_bin_get_by_name(GST_BIN(m_pipeline), "mysrc"));
        if (!m_appsrc)
        {
            gst_object_unref(m_pipeline);
            throw std::runtime_error("failed to get appsrc");
        }

        // Appsrc settings
        gst_app_src_set_stream_type(m_appsrc, GST_APP_STREAM_TYPE_STREAM);

        if (gst_element_set_state(m_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
        {
            gst_object_unref(m_appsrc);
            gst_object_unref(m_pipeline);
            throw std::runtime_error("failed to set pipeline to PLAYING");
        }
    }

    ~UdpVideoSender()
    {
        std::scoped_lock lk(m_mtx);
        if (m_pipeline)
        {
            gst_element_set_state(m_pipeline, GST_STATE_NULL);
            if (m_appsrc)
                gst_object_unref(m_appsrc);
            gst_object_unref(m_pipeline);
        }
        m_appsrc = nullptr;
        m_pipeline = nullptr;
    }

    // Push one BGR frame (stride-aware). Safe to call from capture thread.
    void pushBGR(const cv::Mat &bgr)
    {
        std::scoped_lock lk(m_mtx);
        if (!m_appsrc)
            return;

        // Allocate GstBuffer of packed width*height*3 RGB/BGR bytes
        const int W = m_cfg.width, H = m_cfg.height, C = 3;
        const size_t contiguous = static_cast<size_t>(W) * H * C;

        GstBuffer *buffer = gst_buffer_new_allocate(nullptr, contiguous, nullptr);
        if (!buffer)
            return;

        GstMapInfo map;
        if (!gst_buffer_map(buffer, &map, GST_MAP_WRITE))
        {
            gst_buffer_unref(buffer);
            return;
        }

        // If Mat is tightly packed, memcpy once; else copy row-by-row
        if (bgr.step == static_cast<size_t>(W) * C)
        {
            std::memcpy(map.data, bgr.data, contiguous);
        }
        else
        {
            for (int y = 0; y < H; ++y)
            {
                std::memcpy(map.data + y * W * C, bgr.ptr(y), static_cast<size_t>(W) * C);
            }
        }

        gst_buffer_unmap(buffer, &map);

        // PTS/DURATION (optional since do-timestamp=true)
        // Keeping explicit duration helps some receivers
        const guint64 duration = gst_util_uint64_scale_int(1, GST_SECOND, m_cfg.fps);
        GST_BUFFER_DURATION(buffer) = duration;

        const GstFlowReturn ret = gst_app_src_push_buffer(m_appsrc, buffer);
        if (ret != GST_FLOW_OK)
        {
            // On EOS or error, GStreamer takes ownership (or frees) the buffer.
            // You can add logging if desired.
        }
    }

private:
    Config m_cfg;
    std::mutex m_mtx;
    GstElement *m_pipeline{nullptr};
    GstAppSrc *m_appsrc{nullptr};
};
