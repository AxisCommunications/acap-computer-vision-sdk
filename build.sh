docker build -f Dockerfile.armv7hf \
    --progress=plain \
    --build-arg http_proxy=http://wwwproxy.se.axis.com:3128 \
    --build-arg https_proxy=http://wwwproxy.se.axis.com:3128 \
    --build-arg UBUNTU_VERSION=20.04 \
    --build-arg NUMPY_BUILD_CORES=2 \
    --build-arg SCIPY_BUILD_CORES=2 \
    --build-arg OPENBLAS_BUILD_CORES=2 \
    --target sdk \
    -t acap-computer-vision-sdk:latest .
