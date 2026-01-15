#!/bin/bash

compile_d3d12=0
compile_vulkan=0

for arg in "$@"; do
    if [ "${arg,,}" = "d3d12" ]; then
        compile_d3d12=1
    elif [ "${arg,,}" = "vulkan" ]; then
        compile_vulkan=1
    fi
done

if [ $compile_d3d12 -eq 1 ]; then
    mkdir -p D3D12
fi

if [ $compile_vulkan -eq 1 ]; then
    mkdir -p Vulkan
fi

for file in *.slang; do
    [ -e "$file" ] || continue
    
    filename="${file%.slang}"
    
    if [ $compile_d3d12 -eq 1 ]; then
        slangc "$file" -o "D3D12/${filename}.dxil" -target dxil -profile sm_6_3
    fi
    
    if [ $compile_vulkan -eq 1 ]; then
        slangc "$file" -o "Vulkan/${filename}.spv" -target spirv -profile spirv_1_3 -O3 \
            -fvk-t-shift 0 0 -fvk-u-shift 256 0 -fvk-b-shift 512 0 -fvk-s-shift 768 0
    fi
done