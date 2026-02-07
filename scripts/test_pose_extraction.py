#!/usr/bin/env python3
import sys
sys.path.append("/opt/nvidia/deepstream/deepstream/lib")
import pyds
import gi
gi.require_version("Gst", "1.0")
from gi.repository import Gst, GLib
from ctypes import sizeof, c_float
import argparse

# Test configuration
SOURCE = "file:///opt/nvidia/deepstream/deepstream/samples/streams/sample_720p.mp4"
INFER_CONFIG = "/home/ai4m/Developer/DeepStream-Yolo-Pose/config_infer_primary_yoloV7_pose.txt"
STREAMMUX_WIDTH = 1920
STREAMMUX_HEIGHT = 1080

frame_count = 0
pose_detected = False

def test_pose_probe(pad, info, user_data):
    global frame_count, pose_detected

    buf = info.get_buffer()
    batch_meta = pyds.gst_buffer_get_nvds_batch_meta(hash(buf))

    l_frame = batch_meta.frame_meta_list
    while l_frame:
        try:
            frame_meta = pyds.NvDsFrameMeta.cast(l_frame.data)
        except StopIteration:
            break

        frame_count += 1

        l_obj = frame_meta.obj_meta_list
        while l_obj:
            try:
                obj_meta = pyds.NvDsObjectMeta.cast(l_obj.data)
            except StopIteration:
                break

            # Print bbox info
            print(f"\n=== Frame {frame_count} - Object Detected ===")
            print(f"BBox: left={obj_meta.rect_params.left:.2f}, top={obj_meta.rect_params.top:.2f}, "
                  f"width={obj_meta.rect_params.width:.2f}, height={obj_meta.rect_params.height:.2f}")
            print(f"Confidence: {obj_meta.confidence:.3f}")
            print(f"Tracking ID: {obj_meta.object_id}")

            # Check for pose keypoints in mask_params
            if obj_meta.mask_params.data and obj_meta.mask_params.size > 0:
                pose_detected = True
                num_joints = int(obj_meta.mask_params.size / (sizeof(c_float) * 3))
                print(f"Pose Keypoints Detected: {num_joints} joints")

                # Calculate scaling
                gain = min(obj_meta.mask_params.width / STREAMMUX_WIDTH,
                          obj_meta.mask_params.height / STREAMMUX_HEIGHT)
                pad_x = (obj_meta.mask_params.width - STREAMMUX_WIDTH * gain) * 0.5
                pad_y = (obj_meta.mask_params.height - STREAMMUX_HEIGHT * gain) * 0.5

                # Get mask data
                data = obj_meta.mask_params.get_mask_array()

                # Print first 5 joints as sample
                print("Sample Joints (first 5):")
                for i in range(min(5, num_joints)):
                    xc = (data[i * 3 + 0] - pad_x) / gain
                    yc = (data[i * 3 + 1] - pad_y) / gain
                    confidence = data[i * 3 + 2]
                    print(f"  Joint {i}: x={xc:.2f}, y={yc:.2f}, confidence={confidence:.3f}")

                print(f"✓ POSE DATA AVAILABLE FOR MESSAGE BROKER!")
            else:
                print("✗ No pose keypoints found in mask_params")

            try:
                l_obj = l_obj.next
            except StopIteration:
                break

        try:
            l_frame = l_frame.next
        except StopIteration:
            break

    # Stop after processing a few frames
    if frame_count >= 10:
        print(f"\n\n{'='*60}")
        print(f"TEST COMPLETE: Processed {frame_count} frames")
        if pose_detected:
            print("✓ SUCCESS: Pose keypoints detected and ready for message broker!")
        else:
            print("✗ FAIL: No pose keypoints detected")
        print(f"{'='*60}\n")
        GLib.idle_add(user_data.quit)

    return Gst.PadProbeReturn.OK

def main():
    Gst.init(None)
    loop = GLib.MainLoop()
    pipeline = Gst.Pipeline()

    # Create elements
    source = Gst.ElementFactory.make("filesrc", "source")
    h264parser = Gst.ElementFactory.make("h264parse", "h264-parser")
    decoder = Gst.ElementFactory.make("nvv4l2decoder", "decoder")
    streammux = Gst.ElementFactory.make("nvstreammux", "streammux")
    pgie = Gst.ElementFactory.make("nvinfer", "pgie")
    nvvidconv = Gst.ElementFactory.make("nvvideoconvert", "nvvideoconvert")
    nvosd = Gst.ElementFactory.make("nvdsosd", "nvosd")
    sink = Gst.ElementFactory.make("fakesink", "fakesink")

    if not all([source, h264parser, decoder, streammux, pgie, nvvidconv, nvosd, sink]):
        print("ERROR: Failed to create elements")
        return -1

    pipeline.add(source)
    pipeline.add(h264parser)
    pipeline.add(decoder)
    pipeline.add(streammux)
    pipeline.add(pgie)
    pipeline.add(nvvidconv)
    pipeline.add(nvosd)
    pipeline.add(sink)

    # Set properties
    source.set_property("location", SOURCE.replace("file://", ""))
    streammux.set_property("batch-size", 1)
    streammux.set_property("width", STREAMMUX_WIDTH)
    streammux.set_property("height", STREAMMUX_HEIGHT)
    streammux.set_property("batched-push-timeout", 40000)
    pgie.set_property("config-file-path", INFER_CONFIG)
    sink.set_property("sync", False)

    # Link elements
    source.link(h264parser)
    h264parser.link(decoder)

    # Link decoder to streammux
    sinkpad = streammux.get_request_pad("sink_0")
    srcpad = decoder.get_static_pad("src")
    srcpad.link(sinkpad)

    streammux.link(pgie)
    pgie.link(nvvidconv)
    nvvidconv.link(nvosd)
    nvosd.link(sink)

    # Add probe
    osd_sink_pad = nvosd.get_static_pad("sink")
    osd_sink_pad.add_probe(Gst.PadProbeType.BUFFER, test_pose_probe, loop)

    # Start pipeline
    print("\n" + "="*60)
    print("TESTING POSE KEYPOINT EXTRACTION")
    print("="*60 + "\n")

    pipeline.set_state(Gst.State.PLAYING)

    try:
        loop.run()
    except:
        pass

    pipeline.set_state(Gst.State.NULL)
    return 0

if __name__ == "__main__":
    sys.exit(main())
