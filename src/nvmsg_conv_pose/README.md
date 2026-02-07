# Custom nvmsgconv Library for Pose Estimation

This directory contains the custom message converter library that handles serialization of pose keypoint data to JSON format for Kafka/AMQP message brokers.

## Purpose

The default DeepStream nvmsgconv library doesn't handle the `NvDsJoints` structure. This custom library extends nvmsgconv to properly serialize:
- Pose keypoints (17 COCO joints)
- Joint coordinates (x, y, z)
- Joint confidence values
- Bounding boxes
- Tracking IDs
- Sensor/Place/Analytics metadata

## Files

- `nvmsg_conv_pose.cpp` - Main implementation
- `Makefile` - Build configuration
- `README.md` - This file

## Building

```bash
cd src/nvmsg_conv_pose/
make clean && make
```

**Output:** `libnvds_msgconv_pose.so`

## Usage

Configure in `test5_pose_config.txt`:

```ini
[sink1]
enable=1
type=6
msg-conv-config=msgconv_config_pose.txt
msg-conv-payload-type=1  # Custom payload
msg-conv-msg2p-lib=path/to/libnvds_msgconv_pose.so
msg-broker-proto-lib=/opt/nvidia/deepstream/deepstream/lib/libnvds_kafka_proto.so
msg-broker-conn-str=localhost;9092;deepstream_pose_topic
```

## JSON Output Format

```json
{
  "events": [
    {
      "id": "uuid",
      "timestamp": "2026-02-07T10:30:00.000Z",
      "object": {
        "id": "person",
        "trackingId": "12345",
        "bbox": {
          "topleftx": 100.0,
          "toplefty": 50.0,
          "bottomrightx": 300.0,
          "bottomrighty": 450.0
        },
        "pose": {
          "num_joints": 17,
          "pose_type": 0,
          "joints": [
            {"x": 150.2, "y": 100.5, "z": 0.0, "confidence": 0.95},
            ...
          ]
        }
      },
      "sensor": {
        "id": "pose-camera-001",
        "type": "Pose-Camera"
      },
      "place": {
        "id": "place-0",
        "type": "Pose-Location"
      },
      "analytics": {
        "id": "analytics-0",
        "description": "YOLOv7-Pose-Analytics",
        "version": "1.0"
      }
    }
  ]
}
```

## Key Functions

### `generate_pose_object()`
Serializes NvDsJoints structure to JSON with all 17 keypoints.

### `generate_object_object()`
Combines bounding box, tracking ID, and pose data.

### `generate_event_object()`
Creates complete event with sensor, place, and analytics metadata.

### `nvds_msgconv_pose_generate_json()`
Main entry point for JSON generation.

## Dependencies

- `json-glib-1.0` - JSON serialization
- `uuid` - UUID generation
- `glib-2.0` - GLib utilities
- DeepStream libraries (nvds_meta, nvds_utils)

## Installation

Included in automated install script (`INSTALL.sh`):

```bash
cd /workspace/DeepStream-Yolo-Pose-Complete
bash INSTALL.sh
```

The library will be built and paths updated automatically.

## Troubleshooting

### Library not found
Ensure the path in config is correct:
```ini
msg-conv-msg2p-lib=/full/path/to/libnvds_msgconv_pose.so
```

### Pose data missing in messages
1. Check that `msg-conv-payload-type=1` (custom)
2. Verify library loads: `ldd libnvds_msgconv_pose.so`
3. Check DeepStream logs for conversion errors

### Build errors
Install dependencies:
```bash
apt-get install libjson-glib-dev uuid-dev libglib2.0-dev
```

## License

Based on NVIDIA DeepStream SDK samples (NVIDIA Proprietary License)
