# DeepStream YOLOv7 Pose Estimation with Kafka Integration

Complete implementation of pose estimation using NVIDIA DeepStream SDK 7.0 with YOLOv7-w6-pose model, featuring real-time pose keypoint visualization and Kafka message broker integration.

## Features

- Real-time human pose estimation (17 COCO keypoints)
- Pose keypoint visualization (circles for joints, lines for skeleton)
- Kafka message broker integration for pose data streaming
- Bounding box detection with pose coordinates
- Video file output with pose overlay
- High-performance processing (~110 FPS on GPU)

## System Requirements

- NVIDIA DeepStream SDK 7.0
- CUDA 12.2+
- TensorRT
- Apache Kafka (for message broker functionality)
- Podman/Docker (for containerized deployment)
- GPU with compute capability 7.0+

## Project Structure

```
DeepStream-Yolo-Pose-Complete/
├── configs/                          # Configuration files
│   ├── config_infer_primary_yoloV7_pose.txt
│   ├── test5_video_output.txt
│   ├── test5_pose_config.txt
│   └── labels.txt
├── src/                              # Source code
│   ├── deepstream_test5_app_main.c   # Modified DeepStream test5 app
│   └── nvdsinfer_custom_impl_Yolo_pose/  # Custom pose parsing library
│       ├── nvdsparsepose_Yolo.cpp
│       ├── nvdsparsebbox_Yolo.cpp
│       └── Makefile
├── scripts/                          # Utility scripts
│   ├── kafka_consumer.py
│   └── test_pose_extraction.py
├── models/                           # Model files (not included)
│   └── yolov7-w6-pose.onnx          # Download separately
├── docs/                             # Documentation
│   └── DEVELOPER_GUIDE.md
└── README.md                         # This file
```

## Quick Start

### Step 1: Prerequisites

Ensure you have a DeepStream container running:

```bash
# Check running containers
podman ps

# If needed, start DeepStream container
podman run -it --gpus all \
  -v /home/ai4m/Developer:/workspace \
  nvcr.io/nvidia/deepstream:7.0-triton-multiarch
```

### Step 2: Download Model

Download the YOLOv7-w6-pose ONNX model:

```bash
cd models/
wget https://github.com/WongKinYiu/yolov7/releases/download/v0.1/yolov7-w6-pose.onnx
```

### Step 3: Build Custom Parsing Library

```bash
cd src/nvdsinfer_custom_impl_Yolo_pose/
make
# Output: libnvdsinfer_custom_impl_Yolo_pose.so
```

### Step 4: Build Modified DeepStream Test5 App

```bash
# Inside container
cd /opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test5/

# Backup original file
cp deepstream_test5_app_main.c deepstream_test5_app_main.c.backup

# Copy modified version
cp /workspace/DeepStream-Yolo-Pose-Complete/src/deepstream_test5_app_main.c .

# Build
make clean && make
```

### Step 5: Configure Paths

Update configuration files with your absolute paths:

**configs/config_infer_primary_yoloV7_pose.txt:**
```ini
[property]
onnx-file=/workspace/DeepStream-Yolo-Pose-Complete/models/yolov7-w6-pose.onnx
model-engine-file=/workspace/DeepStream-Yolo-Pose-Complete/models/yolov7-w6-pose_b1_gpu0_fp32.engine
labelfile-path=/workspace/DeepStream-Yolo-Pose-Complete/configs/labels.txt
custom-lib-path=/workspace/DeepStream-Yolo-Pose-Complete/src/nvdsinfer_custom_impl_Yolo_pose/libnvdsinfer_custom_impl_Yolo_pose.so
```

**configs/test5_video_output.txt:**
```ini
[primary-gie]
config-file=/workspace/DeepStream-Yolo-Pose-Complete/configs/config_infer_primary_yoloV7_pose.txt

[sink0]
output-file=/workspace/DeepStream-Yolo-Pose-Complete/output/pose_output.mp4
```

## Running the Application

### Option 1: Video Output Only

Generate video with pose visualization:

```bash
cd /workspace/DeepStream-Yolo-Pose-Complete

/opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test5/deepstream-test5-app \
  -c configs/test5_video_output.txt
```

Output: `output/pose_output.mp4` with pose keypoints visualized

### Option 2: Kafka Message Broker

Send pose data to Kafka:

```bash
# Start Kafka (in separate container)
podman exec -it <kafka-container-id> /opt/kafka/bin/kafka-server-start.sh config/server.properties

# Run DeepStream with Kafka sink
/opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test5/deepstream-test5-app \
  -c configs/test5_pose_config.txt

# Monitor messages (in another terminal)
python3 scripts/kafka_consumer.py
```

### Option 3: Both Video + Kafka

Combine both outputs for complete functionality.

## Configuration Files

### config_infer_primary_yoloV7_pose.txt

Primary inference configuration for YOLOv7 pose model.

**Key Parameters:**
- `network-input-order`: 2 (RGB color format)
- `model-color-format`: 0 (RGB)
- `network-mode`: 0 (FP32 precision)
- `output-instance-mask`: 1 (Enable pose keypoint output)
- `parse-bbox-instance-mask-func-name`: NvDsInferParseYoloPose
- `custom-lib-path`: Path to pose parsing library

### test5_video_output.txt

DeepStream test5 application configuration for video file output.

**Key Sections:**
- `[source0]`: Input video source
- `[sink0]`: Video file sink (type=3, MP4 output)
- `[osd]`: On-screen display settings
- `[primary-gie]`: Pose inference configuration

### test5_pose_config.txt

Configuration with Kafka message broker integration.

**Additional Section:**
- `[sink1]`: Kafka message broker sink (type=6)
- `msg-broker-proto-lib`: Kafka protocol library
- `msg-broker-conn-str`: Kafka connection (localhost;9092;topic)

## Message Format

Pose data sent to Kafka includes:

```json
{
  "object": {
    "id": 0,
    "bbox": {
      "topleftx": 123.45,
      "toplefty": 234.56,
      "bottomrightx": 345.67,
      "bottomrighty": 456.78
    },
    "pose": {
      "num_joints": 17,
      "pose_type": 0,
      "joints": [
        {"x": 150.2, "y": 100.5, "confidence": 0.95},
        {"x": 155.3, "y": 95.2, "confidence": 0.88},
        ...
      ]
    }
  }
}
```

## COCO 17 Keypoint Format

```
0: nose
1: left_eye        2: right_eye
3: left_ear        4: right_ear
5: left_shoulder   6: right_shoulder
7: left_elbow      8: right_elbow
9: left_wrist      10: right_wrist
11: left_hip       12: right_hip
13: left_knee      14: right_knee
15: left_ankle     16: right_ankle
```

## Visualization

The application draws:
- **White circles** with blue centers for each keypoint (joints)
- **Blue lines** connecting joints to form the skeleton
- **Red bounding boxes** around detected persons

Confidence threshold: 0.5 (keypoints below this are not drawn)

## Performance

- Processing Speed: ~110-120 FPS (depending on number of people)
- Model: YOLOv7-w6-pose
- Input Resolution: 640x640
- Output Resolution: 1920x1080
- GPU: NVIDIA (tested on container with DeepStream 7.0)

## Troubleshooting

### TensorRT Engine Not Found

On first run, the engine will be automatically built from ONNX (takes 1-2 minutes):
```
Building TensorRT engine: yolov7-w6-pose_b1_gpu0_fp32.engine
```

### Kafka Connection Failed

Ensure Kafka is running:
```bash
podman ps | grep kafka
podman exec <kafka-container> /opt/kafka/bin/kafka-topics.sh --list --bootstrap-server localhost:9092
```

### No Pose Keypoints Visible

Check that:
1. Custom parsing library is built correctly
2. `output-instance-mask=1` in inference config
3. Modified test5 app is compiled with visualization code

### Config File Parse Error

Use absolute paths in all configuration files:
```ini
# Good
config-file=/workspace/DeepStream-Yolo-Pose-Complete/configs/config_infer_primary_yoloV7_pose.txt

# Bad
config-file=configs/config_infer_primary_yoloV7_pose.txt
```

## Testing

Test pose extraction without full pipeline:

```bash
python3 scripts/test_pose_extraction.py
```

This will show pose keypoint coordinates in console output.

## References

- [NVIDIA DeepStream SDK Documentation](https://docs.nvidia.com/metropolis/deepstream/dev-guide/)
- [YOLOv7 Pose Repository](https://github.com/WongKinYiu/yolov7)
- [DeepStream-Yolo Integration](https://github.com/marcoslucianops/DeepStream-Yolo)

## License

This project follows the licensing of its dependencies:
- DeepStream SDK: NVIDIA License
- YOLOv7: GPL-3.0 License

## Support

For issues and questions, refer to:
- Developer Guide: `docs/DEVELOPER_GUIDE.md`
- DeepStream Forums: https://forums.developer.nvidia.com/c/accelerated-computing/intelligent-video-analytics/deepstream-sdk/

## Contributors

Built using NVIDIA DeepStream SDK and YOLOv7-pose model integration.
