# File Manifest

Complete listing of all files in the DeepStream YOLOv7 Pose project.

## Directory Structure

```
DeepStream-Yolo-Pose-Complete/
├── README.md                                    # Main documentation
├── INSTALL.sh                                   # Automated installation script
├── FILE_MANIFEST.md                            # This file
│
├── configs/                                    # Configuration files
│   ├── config_infer_primary_yoloV7_pose.txt   # Inference engine config
│   ├── test5_video_output.txt                  # Video output configuration
│   ├── test5_pose_config.txt                   # Kafka message broker config
│   └── labels.txt                              # Class labels
│
├── src/                                        # Source code
│   ├── deepstream_test5_app_main.c            # Modified DeepStream test5 app
│   └── nvdsinfer_custom_impl_Yolo_pose/       # Custom parsing library
│       ├── nvdsparsepose_Yolo.cpp             # Pose parsing implementation
│       ├── nvdsparsebbox_Yolo.cpp             # Bounding box parsing
│       ├── nvdsinfer_yolo_pose.h              # Header file
│       ├── Makefile                            # Build configuration
│       └── libnvdsinfer_custom_impl_Yolo_pose.so  # Built library (generated)
│
├── scripts/                                    # Utility scripts
│   ├── kafka_consumer.py                       # Kafka message monitor
│   └── test_pose_extraction.py                 # Pose extraction test
│
├── models/                                     # Model files (user-provided)
│   ├── yolov7-w6-pose.onnx                    # ONNX model (download separately)
│   └── yolov7-w6-pose_b1_gpu0_fp32.engine     # TensorRT engine (auto-generated)
│
├── docs/                                       # Documentation
│   └── DEVELOPER_GUIDE.md                      # Comprehensive developer guide
│
└── output/                                     # Generated outputs (created on run)
    ├── pose_output.mp4                         # Video with pose visualization
    └── *.jpg                                   # Extracted frames
```

## File Descriptions

### Root Directory

#### README.md
**Purpose:** Main project documentation
**Size:** ~15 KB
**Content:**
- Project overview and features
- System requirements
- Quick start guide
- Configuration explanations
- Running instructions
- Troubleshooting tips

#### INSTALL.sh
**Purpose:** Automated installation and setup script
**Size:** ~5 KB
**Usage:** `bash INSTALL.sh`
**Functions:**
- Checks environment (DeepStream container)
- Downloads YOLOv7 model if missing
- Builds custom parsing library
- Builds modified test5 application
- Updates configuration file paths
- Creates output directory

#### FILE_MANIFEST.md
**Purpose:** Complete file listing and descriptions (this file)
**Size:** ~10 KB

---

### configs/

#### config_infer_primary_yoloV7_pose.txt
**Purpose:** Primary inference engine configuration for YOLOv7 pose model
**Size:** ~2 KB
**Key Parameters:**
```ini
[property]
gpu-id=0
net-scale-factor=0.0039215697906911373
model-color-format=0
onnx-file=/path/to/yolov7-w6-pose.onnx
model-engine-file=/path/to/yolov7-w6-pose_b1_gpu0_fp32.engine
labelfile-path=/path/to/labels.txt
batch-size=1
network-input-order=2
network-mode=0
num-detected-classes=1
interval=0
gie-unique-id=1
output-instance-mask=1
parse-bbox-instance-mask-func-name=NvDsInferParseYoloPose
custom-lib-path=/path/to/libnvdsinfer_custom_impl_Yolo_pose.so
```

**Critical Settings:**
- `output-instance-mask=1`: Enables pose keypoint output
- `parse-bbox-instance-mask-func-name`: Custom parsing function name
- `custom-lib-path`: Path to pose parsing library

#### test5_video_output.txt
**Purpose:** DeepStream test5 application configuration for video file output
**Size:** ~1.5 KB
**Key Sections:**
- `[source0]`: Video input source
- `[sink0]`: MP4 file output sink (type=3)
- `[osd]`: On-screen display settings
- `[streammux]`: Stream multiplexer configuration
- `[primary-gie]`: Links to inference config

**Output:** Generates MP4 video with pose visualization

#### test5_pose_config.txt
**Purpose:** Configuration with Kafka message broker
**Size:** ~2 KB
**Additional Sections:**
- `[sink1]`: Kafka message broker sink (type=6)
- `msg-broker-proto-lib`: Kafka protocol library
- `msg-broker-conn-str`: localhost;9092;deepstream_pose_topic
- `topic`: Kafka topic name

**Output:** Sends pose data to Kafka + optional video

#### labels.txt
**Purpose:** Class labels for detected objects
**Size:** 100 bytes
**Content:**
```
person
```

---

### src/

#### deepstream_test5_app_main.c
**Purpose:** Modified DeepStream test5 application with pose visualization
**Size:** ~50 KB
**Original:** `/opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test5/deepstream_test5_app_main.c`

**Modifications:**

1. **Added Skeleton Definition** (Line 638-646):
   ```c
   static const int skeleton[][2] = { /* 19 connections */ };
   #define MAX_DISPLAY_META_ELEMENTS 16
   ```

2. **Added Visualization Function** (Line 648-748):
   ```c
   static void draw_pose_keypoints(...)
   ```
   - Draws white circles for keypoints
   - Draws blue skeleton lines
   - Handles coordinate transformation

3. **Modified generate_event_msg_meta()** (Line ~540-620):
   - Extracts pose keypoints from mask_params
   - Populates NvDsEventMsgMeta.pose structure
   - Scales coordinates for Kafka messages

4. **Updated meta_copy_func()** (Line ~389-396):
   - Copies pose.joints array
   - Uses g_memdup2 for deep copy

5. **Updated meta_free_func()** (Line ~448-460):
   - Frees pose.joints memory
   - Prevents memory leaks

6. **Added Function Call** (Line ~843):
   ```c
   draw_pose_keypoints(batch_meta, frame_meta, obj_meta, ...);
   ```

**Build Command:**
```bash
cd /opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test5
make clean && make
```

**Output:** `deepstream-test5-app` executable

---

### src/nvdsinfer_custom_impl_Yolo_pose/

#### nvdsparsepose_Yolo.cpp
**Purpose:** Custom parsing function for YOLOv7 pose model output
**Size:** ~15 KB
**Key Function:**
```cpp
extern "C" bool NvDsInferParseYoloPose(
    std::vector<NvDsInferLayerInfo> const& outputLayersInfo,
    NvDsInferNetworkInfo const& networkInfo,
    NvDsInferParseDetectionParams const& detectionParams,
    std::vector<NvDsInferInstanceMaskInfo>& objectList)
```

**Responsibilities:**
- Parses TensorRT output tensor [1, 25500, 56]
- Applies confidence threshold
- Performs NMS (Non-Maximum Suppression)
- Extracts 17 COCO keypoints per person
- Populates NvDsInferInstanceMaskInfo.mask with pose data

**Output Format:**
- mask[0-2]: Keypoint 0 (nose): x, y, confidence
- mask[3-5]: Keypoint 1 (left_eye): x, y, confidence
- ...
- mask[48-50]: Keypoint 16 (right_ankle): x, y, confidence

#### nvdsparsebbox_Yolo.cpp
**Purpose:** Bounding box parsing (standard YOLO)
**Size:** ~10 KB
**Function:** `NvDsInferParseCustomYoloV7`

#### nvdsinfer_yolo_pose.h
**Purpose:** Header file with common definitions
**Size:** ~2 KB
**Content:**
- Structure definitions
- Constants
- Helper functions

#### Makefile
**Purpose:** Build configuration for custom library
**Size:** ~500 bytes
**Compiler:** g++
**Flags:**
- `-fPIC`: Position-independent code
- `-std=c++14`: C++14 standard
- `-shared`: Create shared library

**Libraries:**
- CUDA runtime
- TensorRT (nvinfer, nvparsers)
- cuBLAS

**Output:** `libnvdsinfer_custom_impl_Yolo_pose.so`

**Build Command:**
```bash
make clean && make
```

---

### scripts/

#### kafka_consumer.py
**Purpose:** Monitor and display Kafka messages containing pose data
**Size:** ~3 KB
**Dependencies:** `kafka-python` (auto-installed)

**Usage:**
```bash
python3 kafka_consumer.py
```

**Features:**
- Connects to localhost:9092
- Subscribes to 'deepstream_pose_topic'
- Parses JSON messages
- Displays pose keypoint data
- Shows first 3 joints as sample
- Stops after 10 messages (for demo)

**Output Example:**
```
======================================================================
MESSAGE #1 - Offset: 0
======================================================================
{
  "object": {
    "pose": {
      "num_joints": 17,
      "joints": [
        {"x": 150.2, "y": 100.5, "confidence": 0.95},
        ...
      ]
    }
  }
}

✓ POSE DATA DETECTED!
  Num Joints: 17
  First 3 Joints:
    Joint 0: x=150.20, y=100.50, confidence=0.950
    Joint 1: x=155.30, y=95.20, confidence=0.880
    Joint 2: x=145.10, y=96.50, confidence=0.920
  ✓✓✓ SUCCESS: Pose keypoints are in the message! ✓✓✓
```

#### test_pose_extraction.py
**Purpose:** Test pose keypoint extraction without full pipeline
**Size:** ~5 KB
**Dependencies:** `pyds`, `gi` (GStreamer)

**Usage:**
```bash
python3 test_pose_extraction.py
```

**Pipeline:**
```
filesrc → h264parse → nvv4l2decoder → nvstreammux → nvinfer → nvdsosd → fakesink
```

**Output:**
- Processes first 10 frames
- Prints pose keypoints to console
- Validates extraction logic

---

### models/

#### yolov7-w6-pose.onnx
**Purpose:** YOLOv7-w6 pose estimation model in ONNX format
**Size:** ~280 MB
**Download:**
```bash
wget https://github.com/WongKinYiu/yolov7/releases/download/v0.1/yolov7-w6-pose.onnx
```

**Input:** [1, 3, 640, 640] (RGB image)
**Output:** [1, 25500, 56]
- 25500: Number of detection anchors
- 56: [bbox(4) + conf(1) + keypoints(51)]
  - bbox: x_center, y_center, width, height
  - conf: Object confidence
  - keypoints: 17 joints × 3 (x, y, confidence)

#### yolov7-w6-pose_b1_gpu0_fp32.engine
**Purpose:** TensorRT optimized engine (auto-generated)
**Size:** ~403 MB
**Format:** TensorRT serialized engine
**Precision:** FP32
**Batch Size:** 1
**GPU:** 0

**Generation:** Automatically created on first run (takes 1-2 minutes)

---

### docs/

#### DEVELOPER_GUIDE.md
**Purpose:** Comprehensive developer documentation
**Size:** ~40 KB
**Sections:**
1. Architecture Overview
2. Code Modifications (detailed explanations)
3. Building the Application
4. Message Payload Structure
5. Visualization Implementation
6. Custom Parsing Library
7. Troubleshooting Development Issues
8. Extending the Application
9. API Reference
10. Testing

**Audience:** Developers modifying or extending the code

---

### output/

#### pose_output.mp4
**Purpose:** Generated video with pose visualization
**Size:** ~12 MB (for 8-second sample video)
**Format:** MP4 (H.264)
**Resolution:** 1920x1080
**Framerate:** 30 FPS
**Visualization:**
- White circles with blue centers (keypoints)
- Blue skeleton lines
- Red bounding boxes

**Generated by:** Running test5 app with video output config

---

## File Dependencies

```
deepstream_test5_app_main.c
  ├─ Depends on: nvdsmeta_schema.h (NvDsJoint, NvDsJoints structures)
  ├─ Depends on: GStreamer libraries
  ├─ Depends on: DeepStream metadata libraries
  └─ Links with: nvds_meta, nvdsgst_meta

config_infer_primary_yoloV7_pose.txt
  ├─ References: yolov7-w6-pose.onnx
  ├─ References: labels.txt
  └─ References: libnvdsinfer_custom_impl_Yolo_pose.so

libnvdsinfer_custom_impl_Yolo_pose.so
  ├─ Depends on: CUDA runtime
  ├─ Depends on: TensorRT (nvinfer)
  └─ Depends on: cuBLAS

test5_video_output.txt
  └─ References: config_infer_primary_yoloV7_pose.txt

kafka_consumer.py
  └─ Depends on: kafka-python package
```

---

## Build Artifacts

Generated files during build process:

```
src/nvdsinfer_custom_impl_Yolo_pose/
  ├─ libnvdsinfer_custom_impl_Yolo_pose.so  # Custom parsing library
  ├─ nvdsparsepose_Yolo.o                    # Object file
  └─ nvdsparsebbox_Yolo.o                    # Object file

/opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test5/
  ├─ deepstream-test5-app                    # Modified executable
  ├─ deepstream_test5_app_main.o            # Object file
  ├─ deepstream_utc.o                        # Object file
  └─ *.o                                     # Other object files

models/
  └─ yolov7-w6-pose_b1_gpu0_fp32.engine     # TensorRT engine (auto-generated)

output/
  ├─ pose_output.mp4                         # Output video
  └─ *.jpg                                   # Extracted frames
```

---

## Configuration File Updates Required

Before running, update these paths in configuration files:

**In `configs/config_infer_primary_yoloV7_pose.txt`:**
```ini
onnx-file=/full/path/to/models/yolov7-w6-pose.onnx
model-engine-file=/full/path/to/models/yolov7-w6-pose_b1_gpu0_fp32.engine
labelfile-path=/full/path/to/configs/labels.txt
custom-lib-path=/full/path/to/libnvdsinfer_custom_impl_Yolo_pose.so
```

**In `configs/test5_video_output.txt`:**
```ini
[primary-gie]
config-file=/full/path/to/configs/config_infer_primary_yoloV7_pose.txt

[sink0]
output-file=/full/path/to/output/pose_output.mp4
```

**Or run:** `bash INSTALL.sh` to update automatically

---

## Total Project Size

```
Source Code:      ~100 KB
Configuration:    ~10 KB
Scripts:          ~10 KB
Documentation:    ~70 KB
Model (ONNX):     ~280 MB
Engine (TRT):     ~403 MB
Output Video:     ~12 MB (example)
Total:            ~700 MB
```

---

## Version History

- **v1.0** (2026-02-07): Initial release
  - Pose visualization implementation
  - Kafka message broker integration
  - COCO 17-keypoint support
  - Video file output

---

## Related Files Not Included

Files you need to obtain separately:

1. **DeepStream SDK**: Install from NVIDIA NGC
2. **Sample Video**: Use any H.264/MP4 video or DeepStream samples
3. **Kafka Broker**: Install Apache Kafka if using message broker feature

---

## Quick Reference Commands

```bash
# Build parsing library
cd src/nvdsinfer_custom_impl_Yolo_pose && make

# Build test5 app
cd /opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test5 && make

# Run with video output
/opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test5/deepstream-test5-app \
  -c configs/test5_video_output.txt

# Monitor Kafka
python3 scripts/kafka_consumer.py

# Test pose extraction
python3 scripts/test_pose_extraction.py
```

---

## Support Files

For questions or issues, refer to:
- `README.md` - User guide
- `docs/DEVELOPER_GUIDE.md` - Developer documentation
- DeepStream documentation: https://docs.nvidia.com/metropolis/deepstream/

---

Last Updated: 2026-02-07
