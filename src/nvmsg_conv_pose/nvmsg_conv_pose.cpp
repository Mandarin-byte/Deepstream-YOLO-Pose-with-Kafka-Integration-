/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * Custom nvmsgconv library for DeepStream YOLOv7 Pose Estimation
 * Handles conversion of NvDsEventMsgMeta with pose keypoints to JSON
 */

#include <json-glib/json-glib.h>
#include <uuid.h>
#include <stdlib.h>
#include <fstream>
#include <sstream>
#include <cstring>
#include <vector>
#include <iostream>
#include "nvmsgconv.h"
#include "nvds_msgapi.h"
#include "deepstream_schema.h"

#define NVDS_MSGCONV_JSON_STR_LEN 2048

using namespace std;

static void
generate_uuid (gchar * uuid_str)
{
  uuid_t uuid;
  uuid_generate_random (uuid);
  uuid_unparse_lower (uuid, uuid_str);
}

static JsonObject *
generate_pose_object (NvDsEventMsgMeta *meta) {
  JsonObject *jObj = json_object_new ();

  // Add pose metadata
  if (meta->pose.num_joints > 0 && meta->pose.joints) {
    JsonObject *poseObj = json_object_new ();

    // Add num_joints
    json_object_set_int_member (poseObj, "num_joints", meta->pose.num_joints);

    // Add pose_type
    json_object_set_int_member (poseObj, "pose_type", meta->pose.pose_type);

    // Create joints array
    JsonArray *jointsArray = json_array_new ();

    for (guint i = 0; i < meta->pose.num_joints; i++) {
      JsonObject *jointObj = json_object_new ();

      json_object_set_double_member (jointObj, "x", meta->pose.joints[i].x);
      json_object_set_double_member (jointObj, "y", meta->pose.joints[i].y);
      json_object_set_double_member (jointObj, "z", meta->pose.joints[i].z);
      json_object_set_double_member (jointObj, "confidence", meta->pose.joints[i].confidence);

      json_array_add_object_element (jointsArray, jointObj);
    }

    json_object_set_array_member (poseObj, "joints", jointsArray);
    json_object_set_object_member (jObj, "pose", poseObj);
  }

  return jObj;
}

static JsonObject *
generate_object_object (NvDsEventMsgMeta *meta) {
  JsonObject *jObj = json_object_new ();
  gchar tracking_id[64];

  // Object ID
  if (meta->objectId && meta->objectId[0] != '\0') {
    json_object_set_string_member (jObj, "id", meta->objectId);
  }

  // Tracking ID
  g_snprintf (tracking_id, 64, "%lu", meta->trackingId);
  json_object_set_string_member (jObj, "trackingId", tracking_id);

  // Bounding box
  JsonObject *bboxObj = json_object_new ();
  json_object_set_double_member (bboxObj, "topleftx", meta->bbox.left);
  json_object_set_double_member (bboxObj, "toplefty", meta->bbox.top);
  json_object_set_double_member (bboxObj, "bottomrightx",
                                  meta->bbox.left + meta->bbox.width);
  json_object_set_double_member (bboxObj, "bottomrighty",
                                  meta->bbox.top + meta->bbox.height);
  json_object_set_object_member (jObj, "bbox", bboxObj);

  // Add pose data
  JsonObject *poseObj = generate_pose_object (meta);
  if (poseObj) {
    JsonObject *extractedPose = json_object_get_object_member (poseObj, "pose");
    if (extractedPose) {
      json_object_set_object_member (jObj, "pose",
                                       json_object_ref (extractedPose));
    }
    json_object_unref (poseObj);
  }

  return jObj;
}

static JsonObject *
generate_sensor_object (NvDsEventMsgMeta *meta, NvDsPayload *payload) {
  JsonObject *jObj = json_object_new ();
  gchar sensor_id[64];

  if (meta->sensorStr && meta->sensorStr[0] != '\0') {
    json_object_set_string_member (jObj, "id", meta->sensorStr);
  } else {
    g_snprintf (sensor_id, 64, "sensor-%d", meta->sensorId);
    json_object_set_string_member (jObj, "id", sensor_id);
  }

  if (payload->sensorStr && payload->sensorStr[0] != '\0') {
    json_object_set_string_member (jObj, "type", payload->sensorStr);
    json_object_set_string_member (jObj, "description", payload->sensorStr);
  }

  if (meta->coordinate && meta->coordinate[0] != '\0') {
    gdouble lat, lon, alt;
    sscanf (meta->coordinate, "%lf;%lf;%lf", &lat, &lon, &alt);

    JsonObject *geoObj = json_object_new ();
    json_object_set_double_member (geoObj, "lat", lat);
    json_object_set_double_member (geoObj, "lon", lon);
    json_object_set_double_member (geoObj, "alt", alt);
    json_object_set_object_member (jObj, "coordinate", geoObj);
  }

  return jObj;
}

static JsonObject *
generate_place_object (NvDsEventMsgMeta *meta, NvDsPayload *payload) {
  JsonObject *jObj = json_object_new ();
  gchar place_id[64];

  g_snprintf (place_id, 64, "place-%d", meta->placeId);
  json_object_set_string_member (jObj, "id", place_id);

  if (payload->placeStr && payload->placeStr[0] != '\0') {
    json_object_set_string_member (jObj, "type", payload->placeStr);
    json_object_set_string_member (jObj, "name", payload->placeStr);
  }

  if (meta->location && meta->location[0] != '\0') {
    gdouble lat, lon, alt;
    sscanf (meta->location, "%lf;%lf;%lf", &lat, &lon, &alt);

    JsonObject *geoObj = json_object_new ();
    json_object_set_double_member (geoObj, "lat", lat);
    json_object_set_double_member (geoObj, "lon", lon);
    json_object_set_double_member (geoObj, "alt", alt);
    json_object_set_object_member (jObj, "coordinate", geoObj);
  }

  return jObj;
}

static JsonObject *
generate_analytics_object (NvDsEventMsgMeta *meta, NvDsPayload *payload) {
  JsonObject *jObj = json_object_new ();
  gchar analytics_id[64];

  g_snprintf (analytics_id, 64, "analytics-%d", meta->moduleId);
  json_object_set_string_member (jObj, "id", analytics_id);

  if (payload->analyticsStr && payload->analyticsStr[0] != '\0') {
    json_object_set_string_member (jObj, "description", payload->analyticsStr);
    json_object_set_string_member (jObj, "source", payload->analyticsStr);
  }

  json_object_set_string_member (jObj, "version", "1.0");

  return jObj;
}

static JsonObject *
generate_event_object (NvDsEventMsgMeta *meta, NvDsPayload *payload) {
  JsonObject *jObj = json_object_new ();
  gchar event_id[64];

  generate_uuid (event_id);
  json_object_set_string_member (jObj, "id", event_id);

  // Timestamp
  if (meta->ts && meta->ts[0] != '\0') {
    json_object_set_string_member (jObj, "timestamp", meta->ts);
  }

  // Object
  JsonObject *objectObj = generate_object_object (meta);
  json_object_set_object_member (jObj, "object", objectObj);

  // Sensor
  JsonObject *sensorObj = generate_sensor_object (meta, payload);
  json_object_set_object_member (jObj, "sensor", sensorObj);

  // Place
  JsonObject *placeObj = generate_place_object (meta, payload);
  json_object_set_object_member (jObj, "place", placeObj);

  // Analytics
  JsonObject *analyticsObj = generate_analytics_object (meta, payload);
  json_object_set_object_member (jObj, "analytics", analyticsObj);

  return jObj;
}

extern "C" {

gchar *
nvds_msgconv_pose_generate_json (NvDsEvent *events,
                                   guint size,
                                   NvDsPayload *payload) {
  JsonNode *rootNode = json_node_new (JSON_NODE_OBJECT);
  JsonObject *rootObj = json_object_new ();
  json_node_set_object (rootNode, rootObj);

  JsonArray *eventsArray = json_array_new ();

  for (guint i = 0; i < size; i++) {
    NvDsEventMsgMeta *meta = events[i].metadata;
    JsonObject *eventObj = generate_event_object (meta, payload);
    json_array_add_object_element (eventsArray, eventObj);
  }

  json_object_set_array_member (rootObj, "events", eventsArray);
  json_object_set_string_member (rootObj, "messageid", payload->payloadId);
  json_object_set_string_member (rootObj, "@timestamp", "");

  JsonGenerator *generator = json_generator_new ();
  json_generator_set_root (generator, rootNode);
  json_generator_set_pretty (generator, TRUE);

  gchar *message = json_generator_to_data (generator, NULL);

  g_object_unref (generator);
  json_node_free (rootNode);

  return message;
}

gpointer
nvds_msgconv_pose_create_payload (NvDsEvent *events,
                                    guint size,
                                    NvDsPayload **privObj) {
  gchar *message = nvds_msgconv_pose_generate_json (events, size, *privObj);
  return message;
}

NvDsPayloadType
nvds_msgconv_pose_get_payload_type (void) {
  return NVDS_PAYLOAD_CUSTOM;
}

// Main entry point for nvmsgconv
NvDsMsgApiHandle
nvds_msgconv_pose_init (void) {
  NvDsPayload *payload = (NvDsPayload *) g_malloc0 (sizeof (NvDsPayload));

  // Initialize UUID
  payload->payloadId = (gchar *) g_malloc0 (37);
  generate_uuid (payload->payloadId);

  // Set default strings
  payload->sensorStr = g_strdup ("Pose-Camera");
  payload->placeStr = g_strdup ("Pose-Location");
  payload->analyticsStr = g_strdup ("YOLOv7-Pose-Analytics");

  return (NvDsMsgApiHandle) payload;
}

void
nvds_msgconv_pose_finish (NvDsMsgApiHandle handle) {
  NvDsPayload *payload = (NvDsPayload *) handle;

  if (payload->payloadId)
    g_free (payload->payloadId);
  if (payload->sensorStr)
    g_free (payload->sensorStr);
  if (payload->placeStr)
    g_free (payload->placeStr);
  if (payload->analyticsStr)
    g_free (payload->analyticsStr);

  g_free (payload);
}

// Export function table for nvmsgconv
NvDsMsgApiHandle nvds_msgapi_connect (char *connection_str, nvds_msgapi_connect_cb_t connect_cb, char *config_path) {
  return nvds_msgconv_pose_init ();
}

NvDsMsgApiErrorType nvds_msgapi_send (NvDsMsgApiHandle h_ptr, char *topic, const uint8_t *payload, size_t nbuf) {
  return NVDS_MSGAPI_OK;
}

NvDsMsgApiErrorType nvds_msgapi_send_async (NvDsMsgApiHandle h_ptr, char *topic, const uint8_t *payload, size_t nbuf,
    nvds_msgapi_send_cb_t send_callback, void *user_ptr) {
  return NVDS_MSGAPI_OK;
}

void nvds_msgapi_do_work (NvDsMsgApiHandle h_ptr) {
}

NvDsMsgApiErrorType nvds_msgapi_disconnect (NvDsMsgApiHandle h_ptr) {
  nvds_msgconv_pose_finish (h_ptr);
  return NVDS_MSGAPI_OK;
}

char *nvds_msgapi_getversion (void) {
  return (char *)"1.0";
}

char *nvds_msgapi_get_protocol_name (void) {
  return (char *)"POSE_JSON";
}

NvDsMsgApiErrorType nvds_msgapi_connection_signature (char *broker_str, char *cfg, char *output_str, int max_len) {
  snprintf (output_str, max_len, "pose_msgconv");
  return NVDS_MSGAPI_OK;
}

} // extern "C"
