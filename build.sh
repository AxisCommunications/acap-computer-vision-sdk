pushd sdk
DOCKER_BUILDKIT=1 \
docker build -f Dockerfile.armv7hf \
    --build-arg NUMPY_BUILD_CORES=2 \
    --build-arg SCIPY_BUILD_CORES=4 \
    --build-arg OPENBLAS_BUILD_CORES=2 \
    -t axisecp/acap-computer-vision-sdk:latest-armv7hf-ubuntu20.04 .
popd
