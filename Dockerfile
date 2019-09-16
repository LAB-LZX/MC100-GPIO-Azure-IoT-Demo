# MC100-GPIO-Azure-IoT-Demo
# MBYZ

FROM imx6:latest
USER builder
WORKDIR /home/builder

# Copy a directory from the host containing the files to build setting ownership at the same time
ADD --chown=builder:builder src src

# Sanity check
RUN ls -al src

# Switch to application directory
WORKDIR src

# Create and switch to cmake directory
RUN mkdir -p cmake
WORKDIR cmake

# Generate the makefiles with the same toolchain file and build
RUN cmake -DCMAKE_TOOLCHAIN_FILE=${WORK_ROOT}/azure-iot-sdk-c/cmake/toolchain.cmake ..
RUN make

# There should be an executable called mbyz
RUN ls -al mbyz

