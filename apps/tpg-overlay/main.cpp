#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>

int main()
{
    // --- Configuration ---
    int width = 640;
    int height = 480;
    int framerate = 30;
    std::string pc_ip_address = "192.168.1.20";
    int port = 5004;

    std::string capture_pipeline =
        "v4l2src device=/dev/video0 io-mode=dmabuf ! "
        "video/x-raw,format=RGB,width=" +
        std::to_string(width) +
        ",height=" + std::to_string(height) + ",framerate=" + std::to_string(framerate) + "/1 ! "
                                                                                          "videoconvert ! video/x-raw,format=BGR ! appsink";

    cv::VideoCapture cap;
    std::cout << "Opening capture pipeline..." << std::endl;
    cap.open("/dev/video0", cv::CAP_V4L2);
    std::cout << "Capture pipeline opened. Setting properties..." << std::endl;
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('R', 'G', 'B', '3'));
    cap.set(cv::CAP_PROP_FRAME_WIDTH, width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);
    cap.set(cv::CAP_PROP_FPS, framerate);

    if (!cap.isOpened())
    {
        std::cerr << "ERROR: Could not open capture pipeline." << std::endl;
        return -1;
    }
    std::cout << "Capture pipeline opened successfully." << std::endl;

    // --- MODIFIED: VideoWriter pipeline for UDP streaming ---

    std::string writer_pipeline =
        "appsrc ! queue ! videoconvert ! video/x-raw,format=I420 ! jpegenc quality=90 ! rtpjpegpay ! "
        "udpsink host=" +
        pc_ip_address + " port=" + std::to_string(port) + " sync=true";

    cv::VideoWriter udp_writer(writer_pipeline, cv::CAP_GSTREAMER, 0, framerate, cv::Size(width, height), true);

    if (!udp_writer.isOpened())
    {
        std::cerr << "ERROR: Could not open UDP writer pipeline." << std::endl;
        return -1;
    }
    std::cout << "UDP writer pipeline opened. Streaming to " << pc_ip_address << ":" << port << std::endl;

    cv::Mat frame;
    int frame_count = 0;

    std::cout << "Starting capture and streaming loop..." << std::endl;

    while (true)
    {
        bool success = cap.read(frame);

        if (!success || frame.empty())
        {
            std::cerr << "Capture failed or pipeline ended after " << frame_count << " frames." << std::endl;
            break;
        }

        if (frame.empty())
        { // Check for empty frame
            std::cerr << "DEBUG: cap.read() returned an empty frame. Loop is breaking." << std::endl;
            break;
        }

        if (frame_count % 30 == 0)
        {
            std::cout << "Successfully captured frame " << frame_count << std::endl;
        }

        // Send the captured frame over the network
        udp_writer.write(frame);
        frame_count++;
    }

    std::cout << "Loop finished. Sent a total of " << frame_count << " frames." << std::endl;

    udp_writer.release();
    cap.release();

    return 0;
}