# Developer Guide: DeepStream YOLOv7 Pose Estimation

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Code Modifications](#code-modifications)
3. [Building the Application](#building-the-application)
4. [Message Payload Structure](#message-payload-structure)
5. [Visualization Implementation](#visualization-implementation)
6. [Custom Parsing Library](#custom-parsing-library)
7. [Troubleshooting Development Issues](#troubleshooting-development-issues)
8. [Extending the Application](#extending-the-application)

---

## Architecture Overview

### Pipeline Flow

```
┌─────────────┐     ┌──────────────┐     ┌─────────────┐     ┌──────────┐
│   Input     │────▶│  nvstreammux │────▶│   nvinfer   │────▶│  nvdsosd │
│   Source    │     │   (Muxer)    │     │  (YOLOv7)   │     │   (OSD)  │
└─────────────┘     └──────────────┘     └─────────────┘     └──────────┘
                                                │                    │
                                                │                    │
                                                ▼                    ▼
                                         ┌─────────────┐     ┌──────────┐
                                         │  Custom     │     │ Video    │
                                         │  Parsing    │     │ Output   │
                                         │  Library    │     │ Sink     │
                                         └─────────────┘     └──────────┘
                                                │
                                                ▼
                                         ┌─────────────┐
                                         │   Kafka     │
                                         │  Message    │
                                         │   Broker    │
                                         └─────────────┘
```

### Component Responsibilities

1. **nvstreammux**: Batches frames from input sources
2. **nvinfer**: Runs YOLOv7 pose inference on GPU
3. **Custom Parsing Library**: Extracts pose keypoints from model output
4. **Modified Test5 App**: Generates metadata with pose information
5. **nvdsosd**: Renders bounding boxes and pose visualization
6. **Message Broker**: Sends pose data to Kafka

---

## Code Modifications

### 1. Modified DeepStream Test5 App

**File:** `src/deepstream_test5_app_main.c`

#### A. Added Pose Visualization Function

**Location:** Lines 638-748 (before `bbox_generated_probe_after_analytics` function)

```c
/* Skeleton connections for COCO 17 keypoints */
static const int skeleton[][2] = {
    {16, 14}, {14, 12}, {17, 15}, {15, 13}, {12, 13}, {6, 12}, {7, 13},
    {6, 7}, {6, 8}, {7, 9}, {8, 10}, {9, 11}, {2, 3}, {1, 2}, {1, 3},
    {2, 4}, {3, 5}, {4, 6}, {5, 7}
};

#define MAX_DISPLAY_META_ELEMENTS 16

static void
draw_pose_keypoints (NvDsBatchMeta *batch_meta, NvDsFrameMeta *frame_meta,
                     NvDsObjectMeta *obj_meta, guint pipeline_width, guint pipeline_height)
{
  // Extract pose keypoints from mask_params
  // Draw circles for joints
  // Draw lines for skeleton
}
```

**Purpose:**
- Reads pose keypoints from `obj_meta->mask_params`
- Applies coordinate transformation (model resolution → video resolution)
- Uses `NvDsDisplayMeta` to draw circles and lines on the frame

**Key Logic:**

1. **Coordinate Scaling:**
```c
float gain = fmin((float)obj_meta->mask_params.width / pipeline_width,
                  (float)obj_meta->mask_params.height / pipeline_height);
float pad_x = (obj_meta->mask_params.width - pipeline_width * gain) * 0.5;
float pad_y = (obj_meta->mask_params.height - pipeline_height * gain) * 0.5;

// Transform coordinates
float xc = (xc_raw - pad_x) / gain;
float yc = (yc_raw - pad_y) / gain;
```

2. **Drawing Circles (Joints):**
```c
NvOSD_CircleParams *circle_params = &display_meta->circle_params[display_meta->num_circles];
circle_params->xc = (int)fmin(pipeline_width - 1, fmax(0, xc));
circle_params->yc = (int)fmin(pipeline_height - 1, fmax(0, yc));
circle_params->radius = 6;
circle_params->circle_color.red = 1.0;      // White
circle_params->circle_color.green = 1.0;
circle_params->circle_color.blue = 1.0;
circle_params->has_bg_color = 1;
circle_params->bg_color.blue = 1.0;         // Blue background
```

3. **Drawing Lines (Skeleton):**
```c
NvOSD_LineParams *line_params = &display_meta->line_params[display_meta->num_lines];
line_params->x1 = (int)fmin(pipeline_width - 1, fmax(0, x1));
line_params->y1 = (int)fmin(pipeline_height - 1, fmax(0, y1));
line_params->x2 = (int)fmin(pipeline_width - 1, fmax(0, x2));
line_params->y2 = (int)fmin(pipeline_height - 1, fmax(0, y2));
line_params->line_width = 6;
line_params->line_color.blue = 1.0;         // Blue lines
```

#### B. Pose Extraction for Message Broker

**Location:** `generate_event_msg_meta()` function (lines ~540-620)

```c
/** Extract and populate pose keypoints if available */
if (obj_params->mask_params.data && obj_params->mask_params.size > 0) {
  /** Calculate number of joints from mask size (each joint has x, y, confidence = 3 floats) */
  guint num_joints = obj_params->mask_params.size / (sizeof(float) * 3);

  if (num_joints > 0) {
    /** Allocate memory for joints */
    meta->pose.joints = (NvDsJoint *) g_malloc0 (num_joints * sizeof(NvDsJoint));
    meta->pose.num_joints = num_joints;
    meta->pose.pose_type = 0; /** 0 = 2D pose */

    /** Get mask data (pose keypoints) */
    float *mask_data = (float *) obj_params->mask_params.data;

    /** Calculate scaling factors to convert from model resolution to original resolution */
    float gain = fmin((float)obj_params->mask_params.width / appCtx->config.streammux_config.pipeline_width,
                      (float)obj_params->mask_params.height / appCtx->config.streammux_config.pipeline_height);
    float pad_x = (obj_params->mask_params.width - appCtx->config.streammux_config.pipeline_width * gain) * 0.5;
    float pad_y = (obj_params->mask_params.height - appCtx->config.streammux_config.pipeline_height * gain) * 0.5;

    /** Extract each joint and scale back to original resolution */
    for (guint i = 0; i < num_joints; i++) {
      float xc = mask_data[i * 3 + 0];
      float yc = mask_data[i * 3 + 1];
      float confidence = mask_data[i * 3 + 2];

      /** Scale coordinates back to original resolution */
      meta->pose.joints[i].x = ((xc - pad_x) / gain) * scaleW;
      meta->pose.joints[i].y = ((yc - pad_y) / gain) * scaleH;
      meta->pose.joints[i].z = 0.0f;  /** 2D pose, no z coordinate */
      meta->pose.joints[i].confidence = confidence;
    }
  }
}
```

**Purpose:**
- Populates `NvDsEventMsgMeta.pose` structure with keypoint data
- Scales coordinates to match original video resolution
- Sent to Kafka via message broker

#### C. Memory Management

**Location:** `meta_copy_func()` and `meta_free_func()`

**Copy Function:**
```c
/** Copy pose keypoints if available */
if (srcMeta->pose.num_joints > 0 && srcMeta->pose.joints) {
  dstMeta->pose.joints = (NvDsJoint *) g_memdup2 (srcMeta->pose.joints,
      srcMeta->pose.num_joints * sizeof(NvDsJoint));
  dstMeta->pose.num_joints = srcMeta->pose.num_joints;
  dstMeta->pose.pose_type = srcMeta->pose.pose_type;
}
```

**Free Function:**
```c
/** Free pose keypoints if allocated */
if (srcMeta->pose.joints) {
  g_free (srcMeta->pose.joints);
  srcMeta->pose.joints = NULL;
  srcMeta->pose.num_joints = 0;
}
```

#### D. Function Call Integration

**Location:** Inside `bbox_generated_probe_after_analytics()` (line ~843)

```c
/** Draw pose keypoints on frame */
draw_pose_keypoints (batch_meta, frame_meta, obj_meta,
    appCtx->config.streammux_config.pipeline_width,
    appCtx->config.streammux_config.pipeline_height);
```

**Purpose:** Calls visualization function for each detected object with pose data

---

### 2. Custom Parsing Library

**File:** `src/nvdsinfer_custom_impl_Yolo_pose/nvdsparsepose_Yolo.cpp`

#### Key Function: `NvDsInferParseYoloPose`

```cpp
extern "C" bool NvDsInferParseYoloPose(
    std::vector<NvDsInferLayerInfo> const& outputLayersInfo,
    NvDsInferNetworkInfo const& networkInfo,
    NvDsInferParseDetectionParams const& detectionParams,
    std::vector<NvDsInferInstanceMaskInfo>& objectList)
{
    // 1. Extract output tensor
    const float* outputData = (const float*)outputLayersInfo[0].buffer;

    // 2. Parse each detection
    for (int i = 0; i < numDetections; i++) {
        const float* detection = outputData + (i * stride);

        // Extract bbox
        float x_center = detection[0];
        float y_center = detection[1];
        float width = detection[2];
        float height = detection[3];
        float confidence = detection[4];

        // Extract pose keypoints (17 joints x 3 values)
        for (int j = 0; j < 17; j++) {
            maskInfo.mask[j * 3 + 0] = detection[5 + j * 3 + 0];  // x
            maskInfo.mask[j * 3 + 1] = detection[5 + j * 3 + 1];  // y
            maskInfo.mask[j * 3 + 2] = detection[5 + j * 3 + 2];  // confidence
        }

        objectList.push_back(maskInfo);
    }

    return true;
}
```

**Output Format:**
- Model outputs: `[25500, 56]`
  - 25500 detections (all anchors)
  - 56 values per detection:
    - [0-3]: bbox (x, y, w, h)
    - [4]: confidence
    - [5-55]: 17 keypoints × 3 (x, y, conf)

---

## Building the Application

### Step 1: Build Custom Parsing Library

```bash
cd src/nvdsinfer_custom_impl_Yolo_pose/

# Compile
make

# Output
libnvdsinfer_custom_impl_Yolo_pose.so
```

**Makefile Flags:**
- `-fPIC`: Position-independent code for shared library
- `-std=c++14`: C++14 standard
- `-shared`: Create shared library
- CUDA include paths
- DeepStream library paths

### Step 2: Build Modified Test5 App

```bash
# Inside DeepStream container
cd /opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test5/

# Backup original
cp deepstream_test5_app_main.c deepstream_test5_app_main.c.original

# Copy modified version
cp /workspace/DeepStream-Yolo-Pose-Complete/src/deepstream_test5_app_main.c .

# Clean and build
make clean
make

# Output
deepstream-test5-app
```

**Compilation Dependencies:**
- GStreamer 1.0
- DeepStream SDK libraries
- CUDA runtime
- nvds_meta, nvdsgst_meta libraries

### Step 3: TensorRT Engine Building

On first run, TensorRT will optimize the ONNX model:

```bash
# Automatic process (takes 1-2 minutes)
# Creates: yolov7-w6-pose_b1_gpu0_fp32.engine

# Engine parameters:
# - Batch size: 1
# - Precision: FP32
# - GPU: 0
```

---

## Message Payload Structure

### Schema Definition

The message broker uses `nvdsmeta_schema.h` structures:

```c
typedef struct NvDsJoint {
  float x;              // X coordinate
  float y;              // Y coordinate
  float z;              // Z coordinate (0 for 2D)
  float confidence;     // Detection confidence
} NvDsJoint;

typedef struct NvDsJoints {
  NvDsJoint *joints;    // Array of joints
  int num_joints;       // Number of joints (17 for COCO)
  int pose_type;        // 0 = 2D, 1 = 3D
} NvDsJoints;

typedef struct NvDsEventMsgMeta {
  // ... other fields ...
  NvDsJoints pose;      // Pose keypoints
  // ...
} NvDsEventMsgMeta;
```

### JSON Payload Example

```json
{
  "messageid": "12345-67890",
  "mdsversion": "1.0",
  "timestamp": "2026-02-07T04:51:48.000Z",
  "@timestamp": "2026-02-07T04:51:48.000Z",
  "place": {
    "id": "1",
    "name": "Camera-1",
    "type": "Camera",
    "location": {
      "lat": 0.0,
      "lon": 0.0,
      "alt": 0.0
    }
  },
  "sensor": {
    "id": "sensor-1",
    "type": "Camera",
    "description": "Video Source"
  },
  "analyticsModule": {
    "id": "YOLOv7-Pose",
    "description": "Pose Estimation",
    "source": "DeepStream",
    "version": "7.0"
  },
  "object": {
    "id": "0",
    "speed": 0.0,
    "direction": 0.0,
    "orientation": 0.0,
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
        {
          "x": 150.234,
          "y": 100.567,
          "z": 0.0,
          "confidence": 0.952
        },
        // ... 16 more joints
      ]
    },
    "tracking": {
      "id": 0,
      "status": "active"
    }
  },
  "event": {
    "id": "event-123",
    "type": "person-detected"
  },
  "videoPath": "file:///path/to/video.mp4"
}
```

---

## Visualization Implementation

### Coordinate Transformation

The model outputs coordinates in its input resolution (640x640), which must be transformed to the video resolution (1920x1080):

```c
// 1. Calculate letterbox parameters
float gain = min(model_width / video_width, model_height / video_height);
float pad_x = (model_width - video_width * gain) / 2.0;
float pad_y = (model_height - video_height * gain) / 2.0;

// 2. Remove letterbox padding
float x_unpadded = (x_model - pad_x) / gain;
float y_unpadded = (y_model - pad_y) / gain;

// 3. Scale to output resolution
float x_output = x_unpadded * (output_width / video_width);
float y_output = y_unpadded * (output_height / video_height);
```

### Skeleton Definition

COCO 17-keypoint skeleton connections:

```c
static const int skeleton[][2] = {
    // Head
    {1, 2},   // left_eye → right_eye
    {1, 3},   // left_eye → nose
    {2, 3},   // right_eye → nose
    {2, 4},   // right_eye → left_ear
    {3, 5},   // nose → right_ear

    // Torso
    {6, 7},   // left_shoulder → right_shoulder
    {6, 12},  // left_shoulder → left_hip
    {7, 13},  // right_shoulder → right_hip
    {12, 13}, // left_hip → right_hip

    // Left arm
    {6, 8},   // left_shoulder → left_elbow
    {8, 10},  // left_elbow → left_wrist

    // Right arm
    {7, 9},   // right_shoulder → right_elbow
    {9, 11},  // right_elbow → right_wrist

    // Left leg
    {12, 14}, // left_hip → left_knee
    {14, 16}, // left_knee → left_ankle

    // Right leg
    {13, 15}, // right_hip → right_knee
    {15, 17}  // right_knee → right_ankle
};
```

### Display Metadata Management

```c
NvDsDisplayMeta *display_meta = nvds_acquire_display_meta_from_pool(batch_meta);

// Add circles (max 16 per metadata)
for (int i = 0; i < num_joints && display_meta->num_circles < MAX_DISPLAY_META_ELEMENTS; i++) {
    // Configure circle
    display_meta->num_circles++;
}

// Add lines (max 16 per metadata)
for (int i = 0; i < num_lines && display_meta->num_lines < MAX_DISPLAY_META_ELEMENTS; i++) {
    // Configure line
    display_meta->num_lines++;
}

// Attach to frame
nvds_add_display_meta_to_frame(frame_meta, display_meta);
```

**Note:** If more than 16 circles/lines needed, acquire multiple `NvDsDisplayMeta` objects.

---

## Custom Parsing Library

### Architecture

```
┌─────────────────────────────────────────┐
│       TensorRT Engine Output            │
│       Shape: [1, 25500, 56]             │
└─────────────┬───────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────┐
│    NvDsInferParseYoloPose Function      │
│    - Confidence filtering                │
│    - NMS (Non-Maximum Suppression)       │
│    - Keypoint extraction                 │
└─────────────┬───────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────┐
│    NvDsInferInstanceMaskInfo            │
│    - bbox coordinates                    │
│    - mask data (pose keypoints)          │
│    - classId, confidence                 │
└─────────────────────────────────────────┘
```

### Building Custom Library

**Makefile:**
```makefile
CXX := g++
SRCS := nvdsparsepose_Yolo.cpp nvdsparsebbox_Yolo.cpp
TARGET_LIB := libnvdsinfer_custom_impl_Yolo_pose.so

CXXFLAGS := -fPIC -std=c++14 -I/opt/nvidia/deepstream/deepstream/sources/includes \
            -I/usr/local/cuda/include

LDFLAGS := -shared -Wl,--start-group -lnvinfer -lnvparsers -L/usr/local/cuda/lib64 \
           -lcudart -lcublas -Wl,--end-group

$(TARGET_LIB): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET_LIB)
```

### Key Parameters

**In `config_infer_primary_yoloV7_pose.txt`:**

```ini
# Enable pose output
output-instance-mask=1

# Custom parsing function
parse-bbox-instance-mask-func-name=NvDsInferParseYoloPose

# Custom library path
custom-lib-path=/path/to/libnvdsinfer_custom_impl_Yolo_pose.so

# Clustering configuration
cluster-mode=4          # DBSCAN clustering
segmentation-threshold=0.0
```

---

## Troubleshooting Development Issues

### Issue 1: Segmentation Fault on Startup

**Symptom:**
```
Segmentation fault (core dumped)
```

**Cause:** Invalid pointer access in pose extraction

**Solution:**
```c
// Always check before accessing
if (obj_meta->mask_params.data && obj_meta->mask_params.size > 0) {
    // Safe to access
}
```

### Issue 2: Keypoints Not Appearing in Messages

**Symptom:** Kafka messages don't contain pose data

**Diagnosis:**
```bash
# Check if mask data is populated
gdb deepstream-test5-app
(gdb) break generate_event_msg_meta
(gdb) print obj_params->mask_params.size
```

**Solution:**
- Verify `output-instance-mask=1` in inference config
- Check custom parsing library is loaded
- Ensure `parse-bbox-instance-mask-func-name` is correct

### Issue 3: Compilation Errors

**Symptom:**
```
undefined reference to `nvds_acquire_display_meta_from_pool'
```

**Solution:**
```makefile
# Add to linker flags
LDFLAGS += -lnvdsgst_meta -lnvds_meta
```

### Issue 4: Incorrect Coordinates

**Symptom:** Keypoints appear in wrong positions

**Diagnosis:**
- Check coordinate transformation logic
- Verify `pipeline_width` and `pipeline_height` values
- Confirm scaling factors

**Solution:**
```c
// Debug print
printf("Raw: (%.2f, %.2f), Transformed: (%.2f, %.2f)\n",
       xc_raw, yc_raw, xc_final, yc_final);
```

### Issue 5: Memory Leaks

**Symptom:** Increasing memory usage over time

**Diagnosis:**
```bash
valgrind --leak-check=full ./deepstream-test5-app -c config.txt
```

**Solution:**
- Ensure `meta_free_func` properly frees `pose.joints`
- Check all `g_malloc0` have corresponding `g_free`

---

## Extending the Application

### Adding 3D Pose Support

1. Modify parsing library to extract Z coordinates
2. Update `NvDsJoint` z-field population
3. Change `pose_type` to 1 (3D)

```c
meta->pose.pose_type = 1;  // 3D pose
meta->pose.joints[i].z = depth_value;
```

### Adding Pose Classification

Classify poses (standing, sitting, lying):

```c
typedef enum {
    POSE_STANDING = 0,
    POSE_SITTING = 1,
    POSE_LYING = 2
} PoseClassification;

PoseClassification classify_pose(NvDsJoints* pose) {
    // Calculate hip-to-shoulder angle
    // Compare knee-to-hip distance
    // Return classification
}
```

### Multi-Person Tracking

Enable tracker in config:

```ini
[tracker]
enable=1
tracker-width=640
tracker-height=384
ll-lib-file=/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so
```

Track pose over time:
```c
// Store previous frame poses
std::map<int, NvDsJoints> tracked_poses;

// Match current to previous using object_id
tracked_poses[obj_meta->object_id] = current_pose;
```

### Performance Optimization

1. **Use FP16 Precision:**
```ini
network-mode=2  # FP16
```

2. **Adjust Batch Size:**
```ini
batch-size=4  # Process 4 frames simultaneously
```

3. **Enable Dynamic Batching:**
```ini
batched-push-timeout=40000
```

4. **Profile Performance:**
```bash
nsys profile --trace=cuda,nvtx ./deepstream-test5-app -c config.txt
```

---

## API Reference

### DeepStream Functions Used

```c
// Metadata acquisition
NvDsDisplayMeta* nvds_acquire_display_meta_from_pool(NvDsBatchMeta *batch_meta);
NvDsUserMeta* nvds_acquire_user_meta_from_pool(NvDsBatchMeta *batch_meta);

// Metadata attachment
void nvds_add_display_meta_to_frame(NvDsFrameMeta *frame_meta, NvDsDisplayMeta *display_meta);
void nvds_add_user_meta_to_frame(NvDsFrameMeta *frame_meta, NvDsUserMeta *user_meta);

// Metadata iteration
NvDsMetaList* batch_meta->frame_meta_list;
NvDsMetaList* frame_meta->obj_meta_list;

// Memory management
void* g_malloc0(gsize n_bytes);
void g_free(gpointer mem);
gpointer g_memdup2(gconstpointer mem, gsize byte_size);
```

### Configuration Parameters

**Inference Engine:**
- `gie-unique-id`: Unique identifier for GIE
- `gpu-id`: GPU device ID
- `batch-size`: Number of frames per batch
- `network-mode`: 0=FP32, 1=INT8, 2=FP16
- `interval`: Inference interval (0=every frame)

**OSD (On-Screen Display):**
- `enable`: Enable/disable OSD
- `border-width`: Bounding box line width
- `text-size`: Label text size
- `process-mode`: 0=CPU, 1=GPU (HW accelerated)

**Streammux:**
- `batch-size`: Maximum sources to batch
- `width`, `height`: Output resolution
- `enable-padding`: Letterbox padding
- `nvbuf-memory-type`: 0=default, 3=unified

---

## Testing

### Unit Tests

Test pose extraction:
```bash
python3 scripts/test_pose_extraction.py
```

### Integration Tests

Test full pipeline:
```bash
# 1. Start Kafka
# 2. Run application
# 3. Verify output

./run_integration_test.sh
```

### Performance Benchmarking

```bash
# Measure FPS
gst-launch-1.0 filesrc location=test.mp4 ! ... ! fpsdisplaysink video-sink=fakesink

# Profile GPU usage
nvidia-smi dmon -i 0 -s pucvmet -d 1
```

---

## Contributing

When modifying code:

1. **Follow DeepStream conventions**
2. **Add comments for complex logic**
3. **Test with multiple video sources**
4. **Profile performance impact**
5. **Update documentation**

---

## References

- [DeepStream Plugin Manual](https://docs.nvidia.com/metropolis/deepstream/dev-guide/text/DS_plugin_gst-nvinfer.html)
- [TensorRT Developer Guide](https://docs.nvidia.com/deeplearning/tensorrt/developer-guide/)
- [GStreamer Documentation](https://gstreamer.freedesktop.org/documentation/)
- [COCO Dataset Format](https://cocodataset.org/#format-data)

---

## Changelog

### Version 1.0
- Initial implementation with pose visualization
- Kafka message broker integration
- COCO 17-keypoint support
- Video file output

---

## License

This developer guide is provided as documentation for the DeepStream YOLOv7 Pose implementation.
