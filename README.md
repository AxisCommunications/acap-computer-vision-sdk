# acap-computer-vision-sdk
This repository holds the Dockerfiles that create the ACAP Computer Vision SDK images. These images bundles computer vision libraries and packages that are compiled for the AXIS camera platforms.
The Dockerfiles can be used as a reference on how the SDK images are configured, or to rebuild select components with parameters that better fit your application.

The Computer Vision SDK image packages are located under this `/axis` directory. The directory of a package, e.g., `/axis/opencv`, contain
the files needed for the applications as seen from the root of the application container. Thus, merging e.g., `/axis/opencv` with the root `/` of your
container will add the package correctly. This is what is done with the `COPY` commands in the example Dockerfile below.

Dependencies between packages currently need to be handled manually. That is, e.g., `python-numpy` does not include `python`. Rather, both
packages will have to be added to the application container to use the NumPy package.

The SDK comes in two different flavours: `runtime` and `devel`. The `devel`-tagged image contains the SDK build environment and the full packages, including e.g., headers, to allow
building and linking against the packages. The `runtime`-tagged image attempts to only retain the subset of files needed to run packages, which
produces a significantly smaller image. The available tags are `latest-<ARCHITECTURE>`, `latest-<ARCHITECTURE>-<runtime/devel>`, `<VERSION_TAG>-<ARCHITECTURE>` and 
`<VERSION_TAG>-<ARCHITECTURE>-<runtime/devel>`. The images that do not specify `runtime` or `devel` are set to be the smaller `runtime` images. The `latest`-tagged images
are built from the master branch, while the `<VERSION_TAG>`-tagged images are built per release.

## Instructions
1. Select a base image suitable for your camera platform, e.g., `arm32v7/ubuntu:20.04` for running Ubuntu 20.04 natively on the ARTPEC-7 platform.
2. Copy the packages needed for your application from the CV SDK, e.g., for an application running OpenCV in Python, the copied packages would include
OpenCV, Python, NumPy (OpenCV-Python dependency) and OpenBLAS (optimized math functions).


Thus, the Dockerfile for your application could be set up as:
```sh
FROM axisecp/acap-computer-vision-sdk:latest-armv7hf AS cv-sdk
FROM arm32v7/ubuntu:20.04

# Add the CV packages
COPY --from=cv-sdk /axis/opencv /
COPY --from=cv-sdk /axis/python /
COPY --from=cv-sdk /axis/python-numpy /
COPY --from=cv-sdk /axis/openblas /

# Add your application files     
COPY app /app
WORKDIR /app
CMD ["python3", "some_computer_vision_script.py"]
``` 

## Contents
* `/axis/opencv`: [OpenCV 4.5.1](https://github.com/opencv/opencv) with [VDO](https://www.axis.com/products/online-manual/s00004#t10157890)
  * A computer vision library with functionality that covers many different fields within computer vision. 
The VDO integration allows accessing the camera's video streams through the OpenCV VideoCapture class. Compiled with OpenBLAS.
* `/axis/python`: Python
  * A Python 3.8 installation to allow easy prototyping and development of applications.
* Python packages
  * `/axis/python-numpy`: [NumPy](https://github.com/numpy/numpy) - Compiled with OpenBLAS.
  * `/axis/python-scipy`: [SciPy](https://github.com/scipy/scipy) - Compiled with OpenBLAS.
  * `/axis/python-pytesseract`: [PyTesseract](https://github.com/madmaze/pytesseract) - A Python interface to the Tesseract OCR engine.
  * `/axis/python-tfserving`: [A TensorFlow Serving inference client](./tfserving/tf_proto_utils.py#L123) - Allows interfacing with a model server using the TensorFlow Serving prediction gRPC API.  
* `/axis/tesseract`: [Tesseract](https://github.com/tesseract-ocr/tesseract)
  * An OCR engine developed by Google.
* `/axis/openblas`: [OpenBLAS](https://github.com/xianyi/OpenBLAS)
  * A library with optimized linear algebra operations which can accelerate many applications. 
