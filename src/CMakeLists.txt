cmake_minimum_required(VERSION 2.8.11)
project(mbyz_project)

# The demonstration uses C99 but it could just as easily be a C++ application
set (CMAKE_C_FLAGS "--std=c++03 ${CMAKE_C_FLAGS}")

# Assume we will use the built in trusted certificates. 
# Many embedded devices will need this.
option(use_sample_trusted_cert "Set flag in samples to use SDK's built-in CA as TrustedCerts" ON)

set(iothub_c_files
    MBYZ.cpp
)

# Conditionally use the SDK trusted certs in the samples (is set to true in cmake toolchain file)
if(${use_sample_trusted_cert})
    add_definitions(-DSET_TRUSTED_CERT_IN_SAMPLES)
    include_directories($ENV{WORK_ROOT}/azure-iot-sdk-c/certs)
    set(iothub_c_files 
        ${iothub_c_files} 
        $ENV{WORK_ROOT}/azure-iot-sdk-c/certs/certs.c)
endif()

# Set up the include and library paths
include_directories($ENV{TOOLCHAIN_PREFIX}/include/)
include_directories($ENV{TOOLCHAIN_PREFIX}/include/azureiot)
link_directories($ENV{TOOLCHAIN_PREFIX}/lib)

add_executable(mbyz ${iothub_c_files})

# Redundant in this case but demonstrates how to rename your output executable
set_target_properties(mbyz PROPERTIES OUTPUT_NAME "mbyz") 

# List the libraries required by the link step
target_link_libraries(mbyz iothub_client_mqtt_transport iothub_client umqtt aziotsharedutil parson pthread curl ssl crypto m )

