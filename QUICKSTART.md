# Quick Start Guide

Get up and running with DeepStream YOLOv7 Pose in 5 minutes.

## Prerequisites

- DeepStream 7.0 container running
- NVIDIA GPU with CUDA support
- Apache Kafka (optional, for message broker)

## Installation (One Command)

```bash
cd /workspace/DeepStream-Yolo-Pose-Complete
bash INSTALL.sh
```

This will:
1. Download YOLOv7 model
2. Build custom parsing library
3. Build modified test5 app
4. Configure all paths automatically

## Running

### Option 1: Video Output

Generate video with pose visualization:

```bash
/opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test5/deepstream-test5-app \
  -c /workspace/DeepStream-Yolo-Pose-Complete/configs/test5_video_output.txt
```

**Output:** `output/pose_output.mp4`

### Option 2: Kafka Streaming

Send pose data to Kafka:

```bash
# Terminal 1: Run DeepStream
/opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test5/deepstream-test5-app \
  -c /workspace/DeepStream-Yolo-Pose-Complete/configs/test5_pose_config.txt

# Terminal 2: Monitor messages
python3 /workspace/DeepStream-Yolo-Pose-Complete/scripts/kafka_consumer.py
```

## Expected Results

- **Processing Speed:** 110-120 FPS
- **Pose Keypoints:** 17 COCO keypoints per person
- **Visualization:** White circles (joints) + Blue lines (skeleton)
- **Message Format:** JSON with bbox + pose coordinates

## Troubleshooting

**Problem:** "No module named 'kafka'"
```bash
pip3 install kafka-python
```

**Problem:** "Config file parse error"
- Use absolute paths in all config files
- Or run `bash INSTALL.sh` to fix paths

**Problem:** "Engine not found"
- First run builds TensorRT engine (takes 1-2 minutes)
- Wait for: "deserialized trt engine from: ..."

## Next Steps

1. Read `README.md` for detailed configuration
2. See `docs/DEVELOPER_GUIDE.md` to modify code
3. Check `FILE_MANIFEST.md` for complete file descriptions

## Quick Test

Verify pose extraction without full pipeline:

```bash
python3 scripts/test_pose_extraction.py
```

Should print pose keypoints to console.

---

For more information, see the complete documentation in `README.md`.
