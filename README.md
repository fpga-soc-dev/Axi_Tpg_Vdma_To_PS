# AXI TPG and AXI VDMA Example

## Setup Environment
- Vivado 2024.2
- Petalinux 2024.2
- PYNQ Z1 Board
- Ubuntu 24.04
- [GStreamer](https://gstreamer.freedesktop.org/documentation/installing/index.html?gi-language=c)


## Build Vivado Project
```bash
source <vivado_shell_script>
cd Vivado
make
```

## Build Yocto Project
Check requirements for Yocto Build

[Preparing host build](https://docs.yoctoproject.org/singleindex.html#preparing-the-build-host)

[System Requirements](https://docs.yoctoproject.org/scarthgap/ref-manual/system-requirements.html#system-requirements)


Install KAS tool
```bash
pip install kas
```

Build project
```bash
cd yocto
make
```

Write generated .wic image(under `yocto/build/tmp-glibc/deploy/images/pynq-z1/image-pynq-z1-pynq-z1.rootfs.wic` folder) using tools such as Balana Etcher.


## V4L2 and Gstreamer Commands

Check media devices
```bash
media-ctl -p -d /dev/media0 
```

This command lists all the device nodes in the /dev directory that are related to video, media, and specific hardware accelerators.

```bash
ls /dev/ | grep -E 'media|video|v4l|mali|xlnx|allegro'
```

This command lists all the available controls and their current values for a specific Video4Linux (V4L2) sub-device.

```bash
v4l2-ctl -d /dev/v4l2-subdev0 --list-ctrls 
```

This command lists the available options for any menu-type controls on a specific Video4Linux (V4L2) sub-device.

```bash
v4l2-ctl -d /dev/v4l-subdev0 --list-ctrls
```

```bash
gst-launch-1.0 udpsrc port="5004" caps = "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)RAW, sampling=(string)BGR, depth=(string)8, width=(string)640, height=(string)480, colorimetry=(string)BT601-5, payload=(int)96, ssrc=(uint)1103043224, timestamp-offset=(uint)1948293153, seqnum-offset=(uint)27904" ! rtpvrawdepay ! videoconvert ! queue ! fpsdisplaysink text-overlay=true sync=true
```

This command lists all the supported video formats, resolutions, and frame rates for the video device located at /dev/video0

```bash
v4l2-ctl -d /dev/video0 --list-formats 
```

```bash
v4l2-ctl -d /dev/v4l-subdev0 --list-ctrls-menus 
```

This command configures the video format for a specific component within a media device.
Specifically, it sets the output of a Test Pattern Generator (TPG) to produce an RGB video stream at a 640x480 resolution.

```bash
media-ctl -v –d /dev/media0 -V '"43c00000.v_tpg":0[fmt:RBG888_1X24/640x480 field:none]' 
```

This command configures the video device at /dev/video0 to capture video with a specific resolution and pixel format. It tells the device to start producing a 640x480 video stream using 24-bit RGB color.

```bash
v4l2-ctl -d /dev/video0 --set-fmt-video width=640,height=480,pixelformat=BGR3 
```

This command asks the video device at /dev/video0 to report its currently configured video format.

```bash
v4l2-ctl -d /dev/video0 --get-fmt-video
```


Set test pattern number and foreground pattern

```bash
v4l2-ctl -d /dev/v4l-subdev0 --set-ctrl test_pattern=11 
```

```bash
v4l2-ctl -d /dev/v4l-subdev0 --set-ctrl test_pattern_foreground_patter=1 
```

Send Video Stream over UDP to Host PC

```bash
gst-launch-1.0 -v v4l2src device=/dev/video0 io-mode=dmabuf do-timestamp=true ! video/x-raw,format=BGR,width=640,height=480,framerate=60/1 ! queue max-size-buffers=0 ! rtpvrawpay pt=96 mtu=1400 ! udpsink host=192.168.1.20 port=5004 sync=false buffer-size=524288
```

Receive video stream on Host:

```bash
gst-launch-1.0 udpsrc port="5004" caps = "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)RAW, sampling=(string)BGR, depth=(string)8, width=(string)640, height=(string)480, colorimetry=(string)BT601-5, payload=(int)96, ssrc=(uint)1103043224, timestamp-offset=(uint)1948293153, seqnum-offset=(uint)27904" ! rtpvrawdepay ! videoconvert ! queue ! fpsdisplaysink text-overlay=true sync=true
```

Example flow at 1280x720 at 60Hz

Sender:

```bash
media-ctl -v –d /dev/media0 -V '"43c00000.v_tpg":0[fmt:BGR888_1X24/1280x720 field:none]' 
```

```bash
v4l2-ctl -d /dev/video0 --set-fmt-video width=1280,height=720,pixelformat=BGR3 
```

```bash
v4l2-ctl -d /dev/v4l-subdev0 --set-ctrl test_pattern=16 
```

```bash
v4l2-ctl -d /dev/v4l-subdev0 --set-ctrl test_pattern_foreground_patter=1 
```

```bash
gst-launch-1.0 -v v4l2src device=/dev/video0 io-mode=dmabuf do-timestamp=true ! video/x-raw,format=BGR,width=1280,height=720,framerate=60/1 ! queue max-size-buffers=0 ! rtpvrawpay pt=96 mtu=1400 ! udpsink host=192.168.1.20 port=5004 sync=false buffer-size=524288
```
Reciever:

```bash
gst-launch-1.0 udpsrc port="5004" caps = "application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)RAW, sampling=(string)BGR, depth=(string)8, width=(string)1280, height=(string)720, colorimetry=(string)BT601-5, payload=(int)96, ssrc=(uint)1103043224, timestamp-offset=(uint)1948293153, seqnum-offset=(uint)27904" ! rtpvrawdepay ! videoconvert ! queue ! fpsdisplaysink text-overlay=true sync=true
```

