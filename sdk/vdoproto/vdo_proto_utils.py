import grpc
import sys
import numpy as np
import cv2
import videocapture_pb2 as vcap
import videocapture_pb2_grpc as vcap_grpc

RPC_TIMEOUT = 300
class VideoCaptureClient:
    def __init__(self, socket, stream_width, stream_height, stream_framerate):
        self.stream_width = stream_width
        self.stream_height = stream_height
        self.stream_framerate = stream_framerate
        self.grpc_socket = socket
        self.capture_client = None
        self.stream_id = None
        self.setup_grpc_channel()
        self.create_video_stream()

    def setup_grpc_channel(self):
        print("Setting up GRPC channel")
        grpc_channel = grpc.insecure_channel(self.grpc_socket)
        try:
            grpc.channel_ready_future(grpc_channel).result(timeout=RPC_TIMEOUT)
        except grpc.FutureTimeoutError:
            print("Error connecting to gRPC server: Timed out")
            sys.exit(1)
        except Exception as e:
            print(f"Error connecting to gRPC server: {e}")
            sys.exit(1)

        self.capture_client = vcap_grpc.VideoCaptureStub(grpc_channel)

    def create_video_stream(self):
        print("Setting up video stream request")
        stream_request = vcap.NewStreamRequest(
            settings=vcap.StreamSettings(
                format=vcap.VDO_FORMAT_YUV,
                width=self.stream_width,
                height=self.stream_height,
                framerate=self.stream_framerate,
                timestamp_type=vcap.VDO_TIMESTAMP_UTC
            )
        )
        print("Requesting video stream")
        try:
            response = self.capture_client.NewStream(stream_request)
            self.stream_id = response.stream_id
        except Exception as e:
            print(f'Could not create stream: {e}')
            sys.exit(1)

    def get_frame(self):
        frame_request = vcap.GetFrameRequest(
            frame_reference=0,  # Latest frame
            stream_id=self.stream_id
        )
        try:
            response = self.capture_client.GetFrame(frame_request)
        except Exception as e:
            print(f"Could not download latest frame: {e}")
            return None

        # Convert YUV image to RGB
        data = response.data
        yuv_image_buffer = np.frombuffer(data, dtype='uint8').reshape(self.stream_height + self.stream_height // 2, self.stream_width)
        rgb_image = cv2.cvtColor(yuv_image_buffer, cv2.COLOR_YUV2RGB_NV12)
        return rgb_image

    def __del__(self):
        print("Cleaning up video stream")
        if self.capture_client is not None and self.stream_id is not None:
            try:
                request = vcap.DeleteStreamRequest(stream_id=self.stream_id)
                self.capture_client.DeleteStream(request)
                print('Deleted the stream')
            except Exception as e:
                print(f'Could not delete stream: {e}')
