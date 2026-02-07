#!/bin/bash

# DeepStream YOLOv7 Pose Installation Script
# This script helps set up the complete environment

set -e

echo "========================================="
echo "DeepStream YOLOv7 Pose Installation"
echo "========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get the directory where the script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo "Installation directory: $SCRIPT_DIR"
echo ""

# Step 1: Check if running inside DeepStream container
echo "Step 1: Checking environment..."
if [ ! -d "/opt/nvidia/deepstream" ]; then
    echo -e "${RED}ERROR: DeepStream SDK not found!${NC}"
    echo "Please run this script inside a DeepStream container"
    echo ""
    echo "To start the container:"
    echo "  podman run -it --gpus all -v $(dirname $SCRIPT_DIR):/workspace nvcr.io/nvidia/deepstream:7.0-triton-multiarch"
    exit 1
fi
echo -e "${GREEN}✓ DeepStream SDK found${NC}"

# Step 2: Check for model file
echo ""
echo "Step 2: Checking for YOLOv7 model..."
if [ ! -f "$SCRIPT_DIR/models/yolov7-w6-pose.onnx" ]; then
    echo -e "${YELLOW}⚠ Model file not found${NC}"
    echo "Downloading YOLOv7-w6-pose model..."
    mkdir -p "$SCRIPT_DIR/models"
    cd "$SCRIPT_DIR/models"
    wget https://github.com/WongKinYiu/yolov7/releases/download/v0.1/yolov7-w6-pose.onnx
    echo -e "${GREEN}✓ Model downloaded${NC}"
else
    echo -e "${GREEN}✓ Model file found${NC}"
fi

# Step 3: Build custom parsing library
echo ""
echo "Step 3: Building custom pose parsing library..."

# Set CUDA version (auto-detect)
if [ -d "/usr/local/cuda-12.2" ]; then
    export CUDA_VER=12.2
elif [ -d "/usr/local/cuda-12" ]; then
    export CUDA_VER=12.0
elif [ -d "/usr/local/cuda-11" ]; then
    export CUDA_VER=11.0
else
    export CUDA_VER=$(ls /usr/local/ | grep -oP 'cuda-\K[0-9.]+' | head -1)
fi
echo "Using CUDA version: $CUDA_VER"

cd "$SCRIPT_DIR/src/nvdsinfer_custom_impl_Yolo_pose"
make clean
make CUDA_VER=$CUDA_VER
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Parsing library built successfully${NC}"
else
    echo -e "${RED}ERROR: Failed to build parsing library${NC}"
    exit 1
fi

# Step 3b: Build custom nvmsgconv library (SKIPPED FOR NOW - DS 7.0 API incompatibility)
# echo ""
# echo "Step 3b: Building custom message converter library..."
# cd "$SCRIPT_DIR/src/nvmsg_conv_pose"
# make clean
# make
# if [ $? -eq 0 ]; then
#     echo -e "${GREEN}✓ Message converter library built successfully${NC}"
# else
#     echo -e "${RED}ERROR: Failed to build message converter library${NC}"
#     exit 1
# fi
echo ""
echo "Step 3b: Skipping custom message converter (using default DeepStream msgconv)"

# Step 4: Build modified test5 app
echo ""
echo "Step 4: Building modified DeepStream test5 application..."

# Backup original file
if [ ! -f "/opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test5/deepstream_test5_app_main.c.original" ]; then
    echo "Backing up original test5 app..."
    cp /opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test5/deepstream_test5_app_main.c \
       /opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test5/deepstream_test5_app_main.c.original
fi

# Copy modified version
cp "$SCRIPT_DIR/src/deepstream_test5_app_main.c" \
   /opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test5/

# Build
cd /opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test5
make clean
make CUDA_VER=$CUDA_VER
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Test5 application built successfully${NC}"
else
    echo -e "${RED}ERROR: Failed to build test5 application${NC}"
    exit 1
fi

# Step 5: Update configuration paths
echo ""
echo "Step 5: Updating configuration file paths..."

# Update inference config
sed -i "s|onnx-file=.*|onnx-file=$SCRIPT_DIR/models/yolov7-w6-pose.onnx|" \
    "$SCRIPT_DIR/configs/config_infer_primary_yoloV7_pose.txt"
sed -i "s|model-engine-file=.*|model-engine-file=$SCRIPT_DIR/models/yolov7-w6-pose_b1_gpu0_fp32.engine|" \
    "$SCRIPT_DIR/configs/config_infer_primary_yoloV7_pose.txt"
sed -i "s|labelfile-path=.*|labelfile-path=$SCRIPT_DIR/configs/labels.txt|" \
    "$SCRIPT_DIR/configs/config_infer_primary_yoloV7_pose.txt"
sed -i "s|custom-lib-path=.*|custom-lib-path=$SCRIPT_DIR/src/nvdsinfer_custom_impl_Yolo_pose/libnvdsinfer_custom_impl_Yolo_pose.so|" \
    "$SCRIPT_DIR/configs/config_infer_primary_yoloV7_pose.txt"

# Update test5 configs
sed -i "s|config-file=.*|config-file=$SCRIPT_DIR/configs/config_infer_primary_yoloV7_pose.txt|" \
    "$SCRIPT_DIR/configs/test5_video_output.txt"
sed -i "s|output-file=.*|output-file=$SCRIPT_DIR/output/pose_output.mp4|" \
    "$SCRIPT_DIR/configs/test5_video_output.txt"

# Update test5 Kafka config
sed -i "s|config-file=.*|config-file=$SCRIPT_DIR/configs/config_infer_primary_yoloV7_pose.txt|" \
    "$SCRIPT_DIR/configs/test5_pose_config.txt"
sed -i "s|msg-conv-config=.*|msg-conv-config=$SCRIPT_DIR/configs/msgconv_config_pose.txt|" \
    "$SCRIPT_DIR/configs/test5_pose_config.txt"

# Remove msg-conv-msg2p-lib if present (using default DeepStream msgconv for now)
sed -i "/msg-conv-msg2p-lib/d" "$SCRIPT_DIR/configs/test5_pose_config.txt" 2>/dev/null || true

echo -e "${GREEN}✓ Configuration files updated${NC}"

# Step 6: Create output directory
echo ""
echo "Step 6: Creating output directory..."
mkdir -p "$SCRIPT_DIR/output"
echo -e "${GREEN}✓ Output directory created${NC}"

# Installation complete
echo ""
echo "========================================="
echo -e "${GREEN}Installation Complete!${NC}"
echo "========================================="
echo ""
echo "To run the application:"
echo ""
echo "1. Video output only:"
echo "   /opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test5/deepstream-test5-app \\"
echo "     -c $SCRIPT_DIR/configs/test5_video_output.txt"
echo ""
echo "2. With Kafka message broker:"
echo "   /opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test5/deepstream-test5-app \\"
echo "     -c $SCRIPT_DIR/configs/test5_pose_config.txt"
echo ""
echo "3. Monitor Kafka messages:"
echo "   python3 $SCRIPT_DIR/scripts/kafka_consumer.py"
echo ""
echo "For more information, see:"
echo "  - README.md"
echo "  - docs/DEVELOPER_GUIDE.md"
echo ""
