#!/usr/bin/env python3
"""
Simple Kafka Consumer to monitor DeepStream pose messages
"""
import json
import sys

try:
    from kafka import KafkaConsumer
except ImportError:
    print("Installing kafka-python...")
    import subprocess
    subprocess.check_call([sys.executable, "-m", "pip", "install", "kafka-python"])
    from kafka import KafkaConsumer

def main():
    print("="*70)
    print("KAFKA CONSUMER - Monitoring DeepStream Pose Messages")
    print("="*70)
    print(f"Connecting to Kafka broker at localhost:9092")
    print(f"Topic: deepstream_pose_topic")
    print("="*70 + "\n")

    try:
        consumer = KafkaConsumer(
            'deepstream_pose_topic',
            bootstrap_servers=['localhost:9092'],
            auto_offset_reset='earliest',
            enable_auto_commit=True,
            group_id='pose-consumer-group',
            value_deserializer=lambda x: x.decode('utf-8')
        )

        message_count = 0
        for message in consumer:
            message_count += 1
            print(f"\n{'='*70}")
            print(f"MESSAGE #{message_count} - Offset: {message.offset}")
            print(f"{'='*70}")

            try:
                # Try to parse as JSON
                data = json.loads(message.value)
                print(json.dumps(data, indent=2))

                # Check for pose data
                if 'object' in data and 'pose' in data['object']:
                    pose = data['object']['pose']
                    print(f"\n✓ POSE DATA DETECTED!")
                    print(f"  Num Joints: {pose.get('num_joints', 'N/A')}")
                    print(f"  Pose Type: {pose.get('pose_type', 'N/A')}")
                    if 'joints' in pose and len(pose['joints']) > 0:
                        print(f"  First 3 Joints:")
                        for i, joint in enumerate(pose['joints'][:3]):
                            print(f"    Joint {i}: x={joint.get('x', 0):.2f}, "
                                  f"y={joint.get('y', 0):.2f}, "
                                  f"confidence={joint.get('confidence', 0):.3f}")
                        print(f"  ✓✓✓ SUCCESS: Pose keypoints are in the message! ✓✓✓")
                else:
                    print("\n⚠ No pose data found in this message")

            except json.JSONDecodeError:
                # Not JSON, print raw
                print(f"Raw message:\n{message.value}")

            print(f"{'='*70}\n")

            # Stop after 10 messages for demo
            if message_count >= 10:
                print(f"\n{'='*70}")
                print(f"Received {message_count} messages. Stopping consumer.")
                print(f"{'='*70}\n")
                break

    except KeyboardInterrupt:
        print("\nConsumer stopped by user")
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    main()
