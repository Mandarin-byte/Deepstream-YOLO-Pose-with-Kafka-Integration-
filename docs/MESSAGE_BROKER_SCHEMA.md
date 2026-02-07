# Message Broker Schema and Payload Documentation

Complete reference for DeepStream message broker integration with pose estimation data.

## Table of Contents

1. [Overview](#overview)
2. [Schema Structure](#schema-structure)
3. [Payload Format](#payload-format)
4. [C Structures](#c-structures)
5. [JSON Examples](#json-examples)
6. [Configuration](#configuration)
7. [Custom Payload Generation](#custom-payload-generation)

---

## Overview

The DeepStream message broker converts metadata from the analytics pipeline into structured JSON messages that can be sent to messaging systems like Kafka, Azure IoT Hub, or AMQP brokers.

### Message Flow

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│   Analytics  │────▶│ NvDsEventMsg │────▶│  nvmsgconv   │────▶│    Kafka     │
│   Pipeline   │     │   Metadata   │     │   Library    │     │   Message    │
└──────────────┘     └──────────────┘     └──────────────┘     └──────────────┘
     (C code)           (C structs)        (JSON format)         (JSON string)
```

### Components

1. **NvDsEventMsgMeta**: C structure containing all analytics data
2. **nvmsgconv**: Library that converts C structures to JSON
3. **nvmsgbroker**: Broker-specific protocol adapter (Kafka, IoT Hub, etc.)
4. **Message Converter Config**: Maps source IDs to sensor/place metadata

---

## Schema Structure

### NvDsEventMsgMeta (C Structure)

Located in: `/opt/nvidia/deepstream/deepstream/sources/includes/nvdsmeta_schema.h`

```c
typedef struct NvDsEventMsgMeta {
  /** Event message meta data. This can be used to hold information like type of event,
   * event description, etc. */
  NvDsEventType type;                    // Type of event
  gint64 objSignature;                   // Signature for object
  NvDsObjectMeta *objMeta;               // Pointer to NvDsObjectMeta
  /** bbox info for object in frame */
  NvDsRect bbox;                         // Bounding box
  /** location */
  NvDsGeoLocation location;              // GPS location
  /** coordinates */
  NvDsCoordinate coordinate;             // 3D coordinates
  /** object data */
  gchar *objType;                        // Object type string
  gchar *objClassId;                     // Object class ID string
  gint sensorId;                         // Sensor ID
  gint moduleId;                         // Module ID
  gchar *placeId;                        // Place ID string
  gchar *componentId;                    // Component ID string
  gchar *videoPath;                      // Video file path
  /** detected object  */
  gpointer extMsg;                       // Extended message (Vehicle/Person)
  gsize extMsgSize;                      // Size of extended message

  /** POSE ESTIMATION DATA */
  NvDsJoints pose;                       // Pose keypoints (17 COCO joints)

  /** timestamp */
  gchar ts[MAX_TIME_STAMP_LEN];          // ISO 8601 timestamp
} NvDsEventMsgMeta;
```

### NvDsJoints (Pose Structure)

```c
typedef struct NvDsJoint {
  float x;                               // X coordinate in pixels
  float y;                               // Y coordinate in pixels
  float z;                               // Z coordinate (0 for 2D)
  float confidence;                      // Detection confidence [0-1]
} NvDsJoint;

typedef struct NvDsJoints {
  NvDsJoint *joints;                     // Array of joints
  int num_joints;                        // Number of joints (17 for COCO)
  int pose_type;                         // 0 = 2D pose, 1 = 3D pose
} NvDsJoints;
```

### NvDsRect (Bounding Box)

```c
typedef struct NvDsRect {
  float left;                            // Left edge X
  float top;                             // Top edge Y
  float width;                           // Width in pixels
  float height;                          // Height in pixels
} NvDsRect;
```

---

## Payload Format

### Complete JSON Schema

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "title": "DeepStream Pose Estimation Message",
  "type": "object",
  "properties": {
    "messageid": {
      "type": "string",
      "description": "Unique message identifier (UUID)"
    },
    "mdsversion": {
      "type": "string",
      "description": "Metadata schema version"
    },
    "@timestamp": {
      "type": "string",
      "format": "date-time",
      "description": "ISO 8601 timestamp"
    },
    "place": {
      "type": "object",
      "properties": {
        "id": {"type": "string"},
        "name": {"type": "string"},
        "type": {"type": "string"},
        "location": {
          "type": "object",
          "properties": {
            "lat": {"type": "number"},
            "lon": {"type": "number"},
            "alt": {"type": "number"}
          }
        },
        "coordinate": {
          "type": "object",
          "properties": {
            "x": {"type": "number"},
            "y": {"type": "number"},
            "z": {"type": "number"}
          }
        }
      }
    },
    "sensor": {
      "type": "object",
      "properties": {
        "id": {"type": "string"},
        "type": {"type": "string"},
        "description": {"type": "string"},
        "location": {
          "type": "object",
          "properties": {
            "lat": {"type": "number"},
            "lon": {"type": "number"},
            "alt": {"type": "number"}
          }
        }
      }
    },
    "analyticsModule": {
      "type": "object",
      "properties": {
        "id": {"type": "string"},
        "description": {"type": "string"},
        "source": {"type": "string"},
        "version": {"type": "string"}
      }
    },
    "object": {
      "type": "object",
      "properties": {
        "id": {"type": "string"},
        "speed": {"type": "number"},
        "direction": {"type": "number"},
        "orientation": {"type": "number"},
        "bbox": {
          "type": "object",
          "properties": {
            "topleftx": {"type": "number"},
            "toplefty": {"type": "number"},
            "bottomrightx": {"type": "number"},
            "bottomrighty": {"type": "number"}
          },
          "required": ["topleftx", "toplefty", "bottomrightx", "bottomrighty"]
        },
        "pose": {
          "type": "object",
          "properties": {
            "num_joints": {"type": "integer"},
            "pose_type": {"type": "integer"},
            "joints": {
              "type": "array",
              "items": {
                "type": "object",
                "properties": {
                  "x": {"type": "number"},
                  "y": {"type": "number"},
                  "z": {"type": "number"},
                  "confidence": {"type": "number", "minimum": 0, "maximum": 1}
                },
                "required": ["x", "y", "z", "confidence"]
              }
            }
          },
          "required": ["num_joints", "pose_type", "joints"]
        },
        "tracking": {
          "type": "object",
          "properties": {
            "id": {"type": "integer"},
            "status": {"type": "string"}
          }
        }
      }
    },
    "event": {
      "type": "object",
      "properties": {
        "id": {"type": "string"},
        "type": {"type": "string"}
      }
    },
    "videoPath": {
      "type": "string",
      "description": "Source video file path"
    }
  },
  "required": ["messageid", "@timestamp", "object"]
}
```

---

## C Structures

### Complete Structure Definitions

**File:** `/opt/nvidia/deepstream/deepstream/sources/includes/nvdsmeta_schema.h`

```c
/** Maximum time stamp length */
#define MAX_TIME_STAMP_LEN 32

/** Maximum label size */
#define MAX_LABEL_SIZE 128

/** Event types */
typedef enum {
  NVDS_EVENT_ENTRY = 0,
  NVDS_EVENT_EXIT,
  NVDS_EVENT_MOVING,
  NVDS_EVENT_STOPPED,
  NVDS_EVENT_EMPTY,
  NVDS_EVENT_PARKED,
  NVDS_EVENT_RESET,
  NVDS_EVENT_RESERVED,
  NVDS_EVENT_CUSTOM = 0x101,
  NVDS_EVENT_FORCE32 = 0x7FFFFFFF
} NvDsEventType;

/** Bounding box */
typedef struct NvDsRect {
  float left;
  float top;
  float width;
  float height;
} NvDsRect;

/** Geographic location */
typedef struct NvDsGeoLocation {
  gdouble lat;
  gdouble lon;
  gdouble alt;
} NvDsGeoLocation;

/** 3D coordinate */
typedef struct NvDsCoordinate {
  gdouble x;
  gdouble y;
  gdouble z;
} NvDsCoordinate;

/** Single pose keypoint */
typedef struct NvDsJoint {
  float x;
  float y;
  float z;
  float confidence;
} NvDsJoint;

/** Pose keypoints collection */
typedef struct NvDsJoints {
  NvDsJoint *joints;
  int num_joints;
  int pose_type;
} NvDsJoints;

/** Person metadata */
typedef struct NvDsPersonObject {
  gchar gender[MAX_LABEL_SIZE];
  gchar hair[MAX_LABEL_SIZE];
  gchar cap[MAX_LABEL_SIZE];
  gchar apparel[MAX_LABEL_SIZE];
  gint age;
} NvDsPersonObject;

/** Vehicle metadata */
typedef struct NvDsVehicleObject {
  gchar type[MAX_LABEL_SIZE];
  gchar make[MAX_LABEL_SIZE];
  gchar model[MAX_LABEL_SIZE];
  gchar color[MAX_LABEL_SIZE];
  gchar region[MAX_LABEL_SIZE];
  gchar license[MAX_LABEL_SIZE];
} NvDsVehicleObject;

/** Face metadata */
typedef struct NvDsFaceObject {
  gchar gender[MAX_LABEL_SIZE];
  gchar hair[MAX_LABEL_SIZE];
  gchar cap[MAX_LABEL_SIZE];
  gchar glasses[MAX_LABEL_SIZE];
  gchar facialhair[MAX_LABEL_SIZE];
  gchar name[MAX_LABEL_SIZE];
  gchar eyecolor[MAX_LABEL_SIZE];
  gint age;
} NvDsFaceObject;

/** Event message metadata */
typedef struct NvDsEventMsgMeta {
  NvDsEventType type;
  gint64 objSignature;
  gchar objType[MAX_LABEL_SIZE];
  gchar objClassId[16];
  gint sensorId;
  gint moduleId;
  gchar placeId[MAX_LABEL_SIZE];
  gchar componentId[MAX_LABEL_SIZE];
  gchar videoPath[MAX_LABEL_SIZE];

  NvDsRect bbox;
  NvDsGeoLocation location;
  NvDsCoordinate coordinate;
  NvDsJoints pose;

  gpointer extMsg;
  gsize extMsgSize;

  gchar ts[MAX_TIME_STAMP_LEN];
  gchar *objectId;
  gchar *sensorStr;
  gchar *otherAttrs;
  gchar *videoFileName;
} NvDsEventMsgMeta;
```

---

## JSON Examples

### Example 1: Single Person with Full Pose

```json
{
  "messageid": "f47ac10b-58cc-4372-a567-0e02b2c3d479",
  "mdsversion": "1.0",
  "@timestamp": "2026-02-07T10:35:42.123Z",
  "place": {
    "id": "0",
    "name": "Main Entrance",
    "type": "building/entrance",
    "location": {
      "lat": 37.4219999,
      "lon": -122.0840575,
      "alt": 10.5
    },
    "coordinate": {
      "x": 0.0,
      "y": 0.0,
      "z": 0.0
    },
    "place-sub-field1": "Area-A",
    "place-sub-field2": "Zone-1"
  },
  "sensor": {
    "id": "pose-camera-001",
    "type": "Camera",
    "description": "Pose Estimation Camera",
    "location": {
      "lat": 37.4219999,
      "lon": -122.0840575,
      "alt": 12.0
    },
    "coordinate": {
      "x": 0.0,
      "y": 0.0,
      "z": 0.0
    }
  },
  "analyticsModule": {
    "id": "pose-analytics-001",
    "description": "YOLOv7 Pose Estimation with 17 COCO Keypoints",
    "source": "DeepStream-YOLOv7-Pose",
    "version": "1.0"
  },
  "object": {
    "id": "0",
    "speed": 0.0,
    "direction": 0.0,
    "orientation": 0.0,
    "person": {
      "age": 0,
      "gender": "",
      "hair": "",
      "cap": "",
      "apparel": ""
    },
    "bbox": {
      "topleftx": 123.45,
      "toplefty": 234.56,
      "bottomrightx": 345.67,
      "bottomrighty": 678.90
    },
    "pose": {
      "num_joints": 17,
      "pose_type": 0,
      "joints": [
        {"x": 234.5, "y": 150.2, "z": 0.0, "confidence": 0.952},
        {"x": 245.3, "y": 145.7, "z": 0.0, "confidence": 0.881},
        {"x": 225.1, "y": 146.5, "z": 0.0, "confidence": 0.923},
        {"x": 255.8, "y": 148.2, "z": 0.0, "confidence": 0.756},
        {"x": 215.4, "y": 149.1, "z": 0.0, "confidence": 0.834},
        {"x": 270.2, "y": 195.6, "z": 0.0, "confidence": 0.945},
        {"x": 200.8, "y": 196.3, "z": 0.0, "confidence": 0.912},
        {"x": 295.4, "y": 250.1, "z": 0.0, "confidence": 0.887},
        {"x": 175.6, "y": 251.8, "z": 0.0, "confidence": 0.901},
        {"x": 310.2, "y": 305.4, "z": 0.0, "confidence": 0.823},
        {"x": 160.9, "y": 306.7, "z": 0.0, "confidence": 0.845},
        {"x": 255.7, "y": 350.2, "z": 0.0, "confidence": 0.934},
        {"x": 215.3, "y": 351.5, "z": 0.0, "confidence": 0.928},
        {"x": 265.4, "y": 475.8, "z": 0.0, "confidence": 0.891},
        {"x": 205.6, "y": 476.9, "z": 0.0, "confidence": 0.878},
        {"x": 270.1, "y": 590.3, "z": 0.0, "confidence": 0.812},
        {"x": 200.8, "y": 591.7, "z": 0.0, "confidence": 0.798}
      ]
    },
    "location": {
      "lat": 37.4219999,
      "lon": -122.0840575,
      "alt": 10.5
    },
    "coordinate": {
      "x": 234.5,
      "y": 350.2,
      "z": 0.0
    },
    "signature": "12345",
    "tracking": {
      "id": 0,
      "status": "active"
    }
  },
  "event": {
    "id": "event-12345",
    "type": "person-detected"
  },
  "videoPath": "file:///opt/nvidia/deepstream/deepstream/samples/streams/sample_720p.mp4"
}
```

### Example 2: Multiple People

```json
{
  "messageid": "a12bc3de-4567-8910-fghi-jklm11223344",
  "@timestamp": "2026-02-07T10:35:43.456Z",
  "object": [
    {
      "id": "0",
      "bbox": {"topleftx": 100, "toplefty": 200, "bottomrightx": 250, "bottomrighty": 600},
      "pose": {
        "num_joints": 17,
        "pose_type": 0,
        "joints": [/* 17 keypoints */]
      }
    },
    {
      "id": "1",
      "bbox": {"topleftx": 300, "toplefty": 180, "bottomrightx": 450, "bottomrighty": 620},
      "pose": {
        "num_joints": 17,
        "pose_type": 0,
        "joints": [/* 17 keypoints */]
      }
    }
  ]
}
```

### Example 3: Minimal Payload

```json
{
  "messageid": "simple-001",
  "@timestamp": "2026-02-07T10:35:44.789Z",
  "object": {
    "id": "0",
    "bbox": {
      "topleftx": 123.45,
      "toplefty": 234.56,
      "bottomrightx": 345.67,
      "bottomrighty": 678.90
    },
    "pose": {
      "num_joints": 17,
      "pose_type": 0,
      "joints": [
        {"x": 234.5, "y": 150.2, "z": 0.0, "confidence": 0.95},
        {"x": 245.3, "y": 145.7, "z": 0.0, "confidence": 0.88},
        {"x": 225.1, "y": 146.5, "z": 0.0, "confidence": 0.92},
        {"x": 255.8, "y": 148.2, "z": 0.0, "confidence": 0.76},
        {"x": 215.4, "y": 149.1, "z": 0.0, "confidence": 0.83},
        {"x": 270.2, "y": 195.6, "z": 0.0, "confidence": 0.95},
        {"x": 200.8, "y": 196.3, "z": 0.0, "confidence": 0.91},
        {"x": 295.4, "y": 250.1, "z": 0.0, "confidence": 0.89},
        {"x": 175.6, "y": 251.8, "z": 0.0, "confidence": 0.90},
        {"x": 310.2, "y": 305.4, "z": 0.0, "confidence": 0.82},
        {"x": 160.9, "y": 306.7, "z": 0.0, "confidence": 0.85},
        {"x": 255.7, "y": 350.2, "z": 0.0, "confidence": 0.93},
        {"x": 215.3, "y": 351.5, "z": 0.0, "confidence": 0.93},
        {"x": 265.4, "y": 475.8, "z": 0.0, "confidence": 0.89},
        {"x": 205.6, "y": 476.9, "z": 0.0, "confidence": 0.88},
        {"x": 270.1, "y": 590.3, "z": 0.0, "confidence": 0.81},
        {"x": 200.8, "y": 591.7, "z": 0.0, "confidence": 0.80}
      ]
    }
  }
}
```

---

## Configuration

### Message Converter Configuration

**File:** `configs/msgconv_config_pose.txt`

```ini
[sensor0]
enable=1
type=Camera
id=pose-camera-001
location=0.0;0.0;0.0
description=Pose Estimation Camera
coordinate=0.0;0.0;0.0

[place0]
enable=1
id=0
type=building/entrance
name=Main Entrance
location=0.0;0.0;0.0
coordinate=0.0;0.0;0.0
place-sub-field1=Area-A
place-sub-field2=Zone-1
place-sub-field3=Location-001

[analytics0]
enable=1
id=pose-analytics-001
description=YOLOv7 Pose Estimation with 17 COCO Keypoints
source=DeepStream-YOLOv7-Pose
version=1.0
```

### Sink Configuration

**In main config file:** `test5_pose_config.txt`

```ini
[sink1]
enable=1
type=6                                    # Message broker sink
msg-conv-config=msgconv_config_pose.txt   # Message converter config
msg-conv-payload-type=0                   # Full payload
msg-conv-msg2p-lib=/opt/nvidia/deepstream/deepstream/lib/libnvds_msgconv.so
msg-broker-proto-lib=/opt/nvidia/deepstream/deepstream/lib/libnvds_kafka_proto.so
msg-broker-conn-str=localhost;9092;deepstream_pose_topic
topic=deepstream_pose_topic
msg-broker-config=kafka_config.txt        # Optional broker-specific config
```

### Payload Types

```
0 = NVDS_MSG_PAYLOAD_DEEPSTREAM          # Full DeepStream schema
1 = NVDS_MSG_PAYLOAD_DEEPSTREAM_MINIMAL  # Minimal payload
2 = NVDS_MSG_PAYLOAD_CUSTOM              # Custom payload (requires custom library)
```

---

## Custom Payload Generation

### Option 1: Modify nvmsgconv Library

Create custom JSON generation:

```c
// File: custom_msgconv.c
#include "nvdsmeta_schema.h"
#include <jansson.h>

gchar* custom_payload_generate(NvDsEventMsgMeta *meta, gchar **payloadStr) {
    json_t *root = json_object();

    // Add basic fields
    json_object_set_new(root, "timestamp",
        json_string(meta->ts));

    // Add pose data
    if (meta->pose.num_joints > 0) {
        json_t *pose_obj = json_object();
        json_t *joints_array = json_array();

        for (int i = 0; i < meta->pose.num_joints; i++) {
            json_t *joint = json_object();
            json_object_set_new(joint, "x",
                json_real(meta->pose.joints[i].x));
            json_object_set_new(joint, "y",
                json_real(meta->pose.joints[i].y));
            json_object_set_new(joint, "confidence",
                json_real(meta->pose.joints[i].confidence));
            json_array_append_new(joints_array, joint);
        }

        json_object_set_new(pose_obj, "joints", joints_array);
        json_object_set_new(root, "pose", pose_obj);
    }

    // Convert to string
    *payloadStr = json_dumps(root, JSON_COMPACT);
    json_decref(root);

    return *payloadStr;
}
```

### Option 2: Post-Processing

Process messages after Kafka:

```python
from kafka import KafkaConsumer
import json

consumer = KafkaConsumer('deepstream_pose_topic')

for message in consumer:
    data = json.loads(message.value)

    # Extract pose data
    if 'object' in data and 'pose' in data['object']:
        pose = data['object']['pose']

        # Custom processing
        custom_data = {
            'timestamp': data['@timestamp'],
            'person_id': data['object']['id'],
            'keypoints': pose['joints'],
            'num_keypoints': pose['num_joints']
        }

        # Send to custom endpoint
        send_to_database(custom_data)
```

---

## Keypoint Mapping

### COCO 17-Keypoint Index

```python
KEYPOINT_NAMES = [
    "nose",           # 0
    "left_eye",       # 1
    "right_eye",      # 2
    "left_ear",       # 3
    "right_ear",      # 4
    "left_shoulder",  # 5
    "right_shoulder", # 6
    "left_elbow",     # 7
    "right_elbow",    # 8
    "left_wrist",     # 9
    "right_wrist",    # 10
    "left_hip",       # 11
    "right_hip",      # 12
    "left_knee",      # 13
    "right_knee",     # 14
    "left_ankle",     # 15
    "right_ankle"     # 16
]
```

### Access in JSON

```python
def get_keypoint_by_name(pose_data, keypoint_name):
    """Get keypoint coordinates by name"""
    index = KEYPOINT_NAMES.index(keypoint_name)
    return pose_data['joints'][index]

# Example usage
left_shoulder = get_keypoint_by_name(pose_data, "left_shoulder")
print(f"Left shoulder: x={left_shoulder['x']}, y={left_shoulder['y']}, conf={left_shoulder['confidence']}")
```

---

## Performance Considerations

### Message Size

- Average message size: ~5-8 KB per person
- 17 keypoints × 4 values × 8 bytes = ~544 bytes for pose data
- Additional metadata: ~4-5 KB
- Multiple people: linear increase

### Throughput

- Typical: 100-120 messages/second at 30 FPS
- Kafka can handle: 100,000+ messages/second
- Bottleneck is usually analytics, not messaging

### Optimization

1. **Reduce Payload Size:**
```ini
msg-conv-payload-type=1  # Use minimal payload
```

2. **Filter Low Confidence:**
```c
// Only send if confidence > threshold
if (meta->pose.joints[i].confidence > 0.7) {
    // Include in message
}
```

3. **Batch Messages:**
```ini
msg-broker-config=kafka_config.txt

# In kafka_config.txt:
batch.size=16384
linger.ms=10
```

---

## Troubleshooting

### Issue: Pose Data Missing in Messages

**Check:**
1. Verify pose extraction in test5 app
2. Ensure `meta->pose.joints` is allocated
3. Check `meta->pose.num_joints > 0`

**Debug:**
```c
g_print("Pose joints: %d\n", meta->pose.num_joints);
if (meta->pose.joints) {
    g_print("First joint: x=%.2f, y=%.2f\n",
        meta->pose.joints[0].x,
        meta->pose.joints[0].y);
}
```

### Issue: Malformed JSON

**Check:**
- Message converter config path
- nvmsgconv library version
- Schema version compatibility

**Validate:**
```bash
# Check message format
python3 -m json.tool < message.json
```

### Issue: Kafka Connection Failed

**Check:**
```bash
# Test Kafka connectivity
telnet localhost 9092

# List topics
kafka-topics.sh --list --bootstrap-server localhost:9092

# Consume messages
kafka-console-consumer.sh --bootstrap-server localhost:9092 \
  --topic deepstream_pose_topic --from-beginning
```

---

## References

- [DeepStream Plugin Manual](https://docs.nvidia.com/metropolis/deepstream/dev-guide/text/DS_plugin_gst-nvmsgbroker.html)
- [Message Converter API](https://docs.nvidia.com/metropolis/deepstream/dev-guide/text/DS_plugin_gst-nvmsgconv.html)
- [Kafka Integration Guide](https://docs.nvidia.com/metropolis/deepstream/dev-guide/text/DS_protocol_adapters.html)

---

## Appendix: Complete Example

### Producer (DeepStream Test5 App)

```c
// In generate_event_msg_meta()
NvDsEventMsgMeta *meta = g_malloc0(sizeof(NvDsEventMsgMeta));

// Set basic fields
meta->type = NVDS_EVENT_MOVING;
meta->sensorId = source_id;

// Set bounding box
meta->bbox.left = obj_meta->rect_params.left;
meta->bbox.top = obj_meta->rect_params.top;
meta->bbox.width = obj_meta->rect_params.width;
meta->bbox.height = obj_meta->rect_params.height;

// Set pose data
meta->pose.num_joints = 17;
meta->pose.pose_type = 0;
meta->pose.joints = g_malloc0(17 * sizeof(NvDsJoint));

for (int i = 0; i < 17; i++) {
    meta->pose.joints[i].x = /* scaled x */;
    meta->pose.joints[i].y = /* scaled y */;
    meta->pose.joints[i].z = 0.0;
    meta->pose.joints[i].confidence = /* confidence */;
}

// Attach to frame
NvDsUserMeta *user_meta = nvds_acquire_user_meta_from_pool(batch_meta);
user_meta->user_meta_data = (void*)meta;
user_meta->base_meta.meta_type = NVDS_EVENT_MSG_META;
user_meta->base_meta.copy_func = meta_copy_func;
user_meta->base_meta.release_func = meta_free_func;
nvds_add_user_meta_to_frame(frame_meta, user_meta);
```

### Consumer (Python)

```python
from kafka import KafkaConsumer
import json

consumer = KafkaConsumer(
    'deepstream_pose_topic',
    bootstrap_servers=['localhost:9092'],
    value_deserializer=lambda m: json.loads(m.decode('utf-8'))
)

for message in consumer:
    data = message.value

    # Extract pose
    if 'object' in data and 'pose' in data['object']:
        pose = data['object']['pose']
        print(f"Detected person with {pose['num_joints']} keypoints")

        # Process each keypoint
        for i, joint in enumerate(pose['joints']):
            print(f"  Keypoint {i}: ({joint['x']:.2f}, {joint['y']:.2f}) "
                  f"confidence={joint['confidence']:.3f}")
```

---

Last Updated: 2026-02-07
