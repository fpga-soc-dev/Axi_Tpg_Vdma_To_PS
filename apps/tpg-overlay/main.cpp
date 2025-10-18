#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <atomic>
#include <memory>
#include <condition_variable>
#include <mutex>
#include <csignal>

#include "Gpio.hpp"
#include "TpgDriver.hpp"
#include <spdlog/spdlog.h>

constexpr uint32_t SWITCH1_PIN = 0; // GPIO pin for Switch 1
constexpr uint32_t SWITCH2_PIN = 1; // GPIO pin for Switch 2
constexpr uint32_t BUTTON1_PIN = 2; // GPIO pin for Button 1
constexpr uint32_t BUTTON2_PIN = 3; // GPIO pin for Button 2
constexpr uint32_t BUTTON3_PIN = 4; // GPIO pin for Button 3
constexpr uint32_t BUTTON4_PIN = 5; // GPIO pin for Button 4

constexpr int WIDTH = 1280;
constexpr int HEIGHT = 720;
constexpr int FRAMERATE = 30;
constexpr std::string PC_IP_ADDRESS = "192.168.1.20";
constexpr int PORT = 5004;

std::unique_ptr<Tpg::TpgDriver> g_tpg = nullptr;

std::mutex g_status_mutex;
std::condition_variable g_status_cv;
std::atomic<bool> g_status_update_requested{false};
std::atomic<bool> g_should_exit{false};

void TriggerStatusUpdate()
{
    {
        std::lock_guard<std::mutex> lock(g_status_mutex);
        g_status_update_requested = true;
    }
    g_status_cv.notify_one();
}

void SwitchCallback(unsigned int pin, Gpio::Value value, std::chrono::steady_clock::time_point timestamp)
{
    if (!g_tpg)
    {
        return;
    }

    uint32_t current_speed = g_tpg->GetMotionSpeed();

    if (pin == SWITCH1_PIN)
    {
        uint32_t new_speed = std::min(current_speed + 2, 255u);
        if (g_tpg->SetMotionSpeed(new_speed))
        {
            TriggerStatusUpdate();
        }
        else
        {
            SPDLOG_ERROR("Failed to increase motion speed");
        }
    }
    else if (pin == SWITCH2_PIN)
    {
        uint32_t new_speed = (current_speed >= 2) ? current_speed - 2 : 0;
        if (g_tpg->SetMotionSpeed(new_speed))
        {
            TriggerStatusUpdate();
        }
        else
        {
            SPDLOG_ERROR("Failed to decrease motion speed");
        }
    }
}

void ButtonPressCallback(unsigned int pin, Gpio::Value value, std::chrono::steady_clock::time_point timestamp)
{
    if (!g_tpg)
    {
        return;
    }

    if (value == Gpio::Value::HIGH)
    {
        switch (pin)
        {
        case BUTTON1_PIN:
        {
            auto current_pattern = g_tpg->GetTestPattern();
            if (current_pattern == Tpg::TestPattern::CUSTOM_PATTERN)
            {
                current_pattern = Tpg::TestPattern::HORIZONTAL_RAMP;
            }
            else
            {
                current_pattern = static_cast<Tpg::TestPattern>(static_cast<uint32_t>(current_pattern) + 1);
            }

            if (g_tpg->SetTestPattern(current_pattern))
            {
                TriggerStatusUpdate();
            }
            else
            {
                SPDLOG_ERROR("Failed to change background pattern");
            }
            break;
        }
        case BUTTON2_PIN:
        {
            uint32_t current_fg = g_tpg->GetForegroundPattern();
            uint32_t new_fg = (current_fg + 1) % 3;

            if (g_tpg->SetForegroundPattern(new_fg))
            {
                TriggerStatusUpdate();
            }
            else
            {
                SPDLOG_ERROR("Failed to change foreground pattern");
            }
            break;
        }

        case BUTTON3_PIN:
        {
            uint32_t current_size = g_tpg->GetBoxSize();
            uint32_t new_size = std::min(current_size + 20, 1023u);

            if (g_tpg->SetBoxSize(new_size))
            {
                TriggerStatusUpdate();
            }
            else
            {
                SPDLOG_ERROR("Failed to increase box size");
            }
            break;
        }

        case BUTTON4_PIN:
        {
            uint32_t current_size = g_tpg->GetBoxSize();
            uint32_t new_size = (current_size >= 21) ? current_size - 20 : 1;

            if (g_tpg->SetBoxSize(new_size))
            {
                TriggerStatusUpdate();
            }
            else
            {
                SPDLOG_ERROR("Failed to decrease box size");
            }
            break;
        }
        default:
            SPDLOG_WARN("Unknown button pin: {}", pin);
            break;
        }
    }
}

void SignalHandler(int signal)
{
    SPDLOG_INFO("Received signal {}, initiating shutdown...", signal);
    g_should_exit = true;
    g_status_cv.notify_all();
}

int main()
{
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%^%l%$] %v");

    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    try
    {
        // Initialize TPG
        Tpg::TpgConfig config;
        config.format = Tpg::PixelFormat::RBG888;
        config.pattern = Tpg::TestPattern::COLOR_BARS;
        config.foreground_pattern = 1;
        config.media_device = "/dev/media0";
        config.video_device = "/dev/video0";
        config.subdev_device = "/dev/v4l-subdev0";
        config.tpg_entity = "43c00000.v_tpg";
        config.width = WIDTH;
        config.height = HEIGHT;

        g_tpg = std::make_unique<Tpg::TpgDriver>(config);

        if (!g_tpg->Initialize())
        {
            SPDLOG_ERROR("Failed to initialize TPG driver");
            return -1;
        }

        Gpio::Gpio gpio("/dev/gpiochip0", {0, 5}, {0, 0});

        Gpio::InterruptConfig switchConfig;
        switchConfig.edgeType = Gpio::EdgeType::BOTH;
        switchConfig.debouncePeriod = std::chrono::milliseconds(200);
        switchConfig.callback = SwitchCallback;
        switchConfig.enabled = true;

        Gpio::InterruptConfig buttonConfig;
        buttonConfig.edgeType = Gpio::EdgeType::RISING;
        buttonConfig.debouncePeriod = std::chrono::milliseconds(200);
        buttonConfig.callback = ButtonPressCallback;
        buttonConfig.enabled = true;

        gpio.EnableInterrupt(SWITCH1_PIN, switchConfig);
        gpio.EnableInterrupt(SWITCH2_PIN, switchConfig);
        gpio.EnableInterrupt(BUTTON1_PIN, buttonConfig);
        gpio.EnableInterrupt(BUTTON2_PIN, buttonConfig);
        gpio.EnableInterrupt(BUTTON3_PIN, buttonConfig);
        gpio.EnableInterrupt(BUTTON4_PIN, buttonConfig);

        std::string capture_pipeline =
            "v4l2src device=/dev/video0 io-mode=dmabuf ! "
            "video/x-raw,format=BGR,width=" +
            std::to_string(config.width) +
            ",height=" + std::to_string(config.height) + ",framerate=" + std::to_string(FRAMERATE) + "/1 ! appsink";

        cv::VideoCapture cap(capture_pipeline, cv::CAP_GSTREAMER);

        if (!cap.isOpened())
        {
            SPDLOG_ERROR("Could not open capture pipeline.");
            return -1;
        }

        std::string writer_pipeline =
            "appsrc format=time do-timestamp=true "
            "caps=video/x-raw,format=BGR,width=" +
            std::to_string(config.width) +
            ",height=" + std::to_string(config.height) + ",framerate=" + std::to_string(FRAMERATE) + "/1 ! "
                                                                                                     "queue ! videoconvert ! video/x-raw,format=I420 ! "
                                                                                                     "jpegenc quality=90 ! rtpjpegpay pt=26 ! "
                                                                                                     "udpsink host=" +
            PC_IP_ADDRESS + " port=" + std::to_string(PORT) + " sync=true";

        cv::VideoWriter udp_writer(writer_pipeline, cv::CAP_GSTREAMER, 0, FRAMERATE, cv::Size(config.width, config.height), true);

        if (!udp_writer.isOpened())
        {
            SPDLOG_ERROR("Could not open UDP writer pipeline.");
            return -1;
        }

        SPDLOG_INFO("SW1 : Increase Motion Speed");
        SPDLOG_INFO("SW2 : Decrease Motion Speed");
        SPDLOG_INFO("BTN1: Cycle Background Pattern");
        SPDLOG_INFO("BTN2: Cycle Foreground Pattern");
        SPDLOG_INFO("BTN3: Increase Box Size");
        SPDLOG_INFO("BTN4: Decrease Box Size");
        SPDLOG_INFO("Press Ctrl+C to exit.");

        cv::Mat frame;
        int frame_count{0};

        while (!g_should_exit.load())
        {
            bool success = cap.read(frame);

            if (!success || frame.empty())
            {
                SPDLOG_ERROR("Capture failed or pipeline ended after {} frames.", frame_count);
                break;
            }

            if (frame.empty())
            {
                SPDLOG_DEBUG("cap.read() returned an empty frame. Loop is breaking.");
                break;
            }

            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);

            std::stringstream ss;
            ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
            std::string timestamp = ss.str();
            cv::putText(frame, timestamp, cv::Point(10, 30),
                        cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(52, 50, 168), 2);
            cv::putText(frame, timestamp, cv::Point(10, 30),
                        cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(52, 50, 168), 1);

            auto [width, height] = g_tpg->GetResolution();
            auto format = g_tpg->GetPixelFormat();
            auto pattern = g_tpg->GetTestPattern();
            auto fg_pattern = g_tpg->GetForegroundPattern();
            auto speed = g_tpg->GetMotionSpeed();
            auto box_size = g_tpg->GetBoxSize();

            int y_offset = 60;
            int line_height = 25;

            std::string info_text;

            info_text = "Resolution: " + std::to_string(width) + "x" + std::to_string(height);
            cv::putText(frame, info_text, cv::Point(10, y_offset),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 255), 2);
            y_offset += line_height;

            info_text = "BG Pattern: " + std::to_string(static_cast<uint32_t>(pattern));
            cv::putText(frame, info_text, cv::Point(10, y_offset),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 255), 2);
            y_offset += line_height;

            info_text = "FG Pattern: " + std::to_string(fg_pattern);
            cv::putText(frame, info_text, cv::Point(10, y_offset),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 255), 2);
            y_offset += line_height;

            info_text = "Speed: " + std::to_string(speed);
            cv::putText(frame, info_text, cv::Point(10, y_offset),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 255), 2);
            y_offset += line_height;

            info_text = "Box Size: " + std::to_string(box_size);
            cv::putText(frame, info_text, cv::Point(10, y_offset),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 255), 2);

            y_offset += line_height;
            info_text = "Streaming to: " + PC_IP_ADDRESS + ":" + std::to_string(PORT);
            cv::putText(frame, info_text, cv::Point(10, y_offset),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 255), 2);

            udp_writer.write(frame);
            frame_count++;
        }

        udp_writer.release();
        cap.release();

        gpio.DisableAllInterrupts();
    }
    catch (const std::exception &e)
    {
        SPDLOG_ERROR("Exception in main: {}", e.what());
        g_should_exit = true;
        g_status_cv.notify_all();
    }

    g_tpg.reset();
    SPDLOG_INFO("Application shutdown complete");

    return 0;
}