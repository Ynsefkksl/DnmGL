@echo off
setlocal enabledelayedexpansion

set compile_d3d12=0
set compile_vulkan=0

:parse_args
if /i "%~1"=="D3D12" set compile_d3d12=1
if /i "%~1"=="Vulkan" set compile_vulkan=1
if "%~1"=="" goto end_parse
shift
goto parse_args
:end_parse

if /i %compile_d3d12%==1 (
    if not exist "D3D12" mkdir D3D12
)

if /i %compile_vulkan%==1 (
    if not exist "Vulkan" mkdir Vulkan
)

for %%f in (*.slang) do (
    if /i %compile_d3d12%==1 (
        slangc %%f -o D3D12\%%~nf.dxil -target dxil -profile sm_6_3
    )
    
    if /i %compile_vulkan%==1 (       
        slangc %%f -o Vulkan\%%~nf.spv -target spirv -profile spirv_1_3 -O3 -fvk-t-shift 0 0 -fvk-u-shift 256 0 -fvk-b-shift 512 0 -fvk-s-shift 768 0
    )
)

endlocal