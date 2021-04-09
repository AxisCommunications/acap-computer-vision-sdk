# acap-computer-vision-sdk
This repository holds the Dockerfiles that create the ACAP Computer Vision SDK images. These images bundles computer vision libraries and packages that are compiled for the AXIS camera platforms.
The Dockerfiles can be used as a reference on how the image is configured, or to rebuild select components with parameters that better fit your application.

## Contents
* [OpenCV 4.5.1](https://github.com/opencv/opencv) with VDO
  * A computer vision library with functionality that covers many different fields within computer vision. 
The VDO integration allows accessing the camera's video streams through the OpenCV VideoCapture class. Compiled with OpenBLAS.
* Python
  * A Python 3.8 installation to allow easy prototyping and development of applications.
* Python packages
  * [NumPy](https://github.com/numpy/numpy) - Compiled with OpenBLAS.
  * [SciPy](https://github.com/scipy/scipy) - Compiled with OpenBLAS.
  * [PyTesseract](https://github.com/madmaze/pytesseract) - A Python interface to the Tesseract OCR engine.
  * [A TensorFlow Serving inference client](./tfserving/tf_proto_utils.py#L123) - Allows interfacing with a model server using the TensorFlow Serving prediction gRPC API.  
* [Tesseract](https://github.com/tesseract-ocr/tesseract)
  * An OCR engine developed by Google.
* [OpenBLAS](https://github.com/xianyi/OpenBLAS)
  * A library with optimized linear algebra operations which can accelerate many applications. 
