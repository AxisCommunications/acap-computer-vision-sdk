'''
  Copyright (C) 2020 Axis Communications AB, Lund, Sweden
 
  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at
 
      http://www.apache.org/licenses/LICENSE-2.0
 
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
'''

import numpy as np
import time
from grpc import insecure_channel as grpc_insecure_channel
from tensorflow_serving.apis import predict_pb2, prediction_service_pb2_grpc
from tensorflow.core.framework import types_pb2, tensor_shape_pb2, tensor_pb2
from tensorflow.core.protobuf import meta_graph_pb2


def make_tensor_proto(values):
    if not isinstance(values, (np.ndarray, np.generic)):
        values = np.array(values)
    if values.dtype == np.float64:
        values = values.astype(np.float32)
    shape = values.shape
    dtype = NP_TO_PB[values.dtype.type]
    dims = [tensor_shape_pb2.TensorShapeProto.Dim(size=size) for size in shape]
    tensor_shape_proto = tensor_shape_pb2.TensorShapeProto(dim=dims)
    return tensor_pb2.TensorProto(dtype=dtype, tensor_shape=tensor_shape_proto, tensor_content=values.tostring())


def make_ndarray(proto):
    shape = [d.size for d in proto.tensor_shape.dim]
    num_elements = np.prod(shape, dtype=np.int64)
    np_dtype = PB_TO_NP[proto.dtype]
    if proto.tensor_content:
        return np.frombuffer(proto.tensor_content, dtype=np_dtype).copy().reshape(shape)

    if proto.dtype == types_pb2.DT_HALF:
        if len(proto.half_val) == 1:
            tmp = np.array(proto.half_val[0], dtype=np.uint16)
            tmp.dtype = np_dtype
            return np.repeat(tmp, num_elements).reshape(shape)
        tmp = np.fromiter(proto.half_val, dtype=np.uint16)
        tmp.dtype = np_dtype
        return tmp.reshape(shape)

    if proto.dtype == types_pb2.DT_STRING:
        if len(proto.string_val) == 1:
            return np.repeat(np.array(proto.string_val[0], dtype=np_dtype), num_elements).reshape(shape)
        return np.array([x for x in proto.string_val], dtype=np_dtype).reshape(shape)

    if proto.dtype == types_pb2.DT_COMPLEX64 or np_dtype == types_pb2.DT_COMPLEX128:
        if proto.dtype == types_pb2.DT_COMPLEX64:
            proto_value = proto.scomplex_val
        else:
            proto_value = proto.dcomplex_val
        it = iter(proto_value)
        if len(proto_value) == 2:
            return np.repeat(np.array(complex(proto_value[0], proto_value[1]), dtype=np_dtype), num_elements).reshape(shape)
        return np.array([complex(x[0], x[1]) for x in zip(it, it)], dtype=np_dtype).reshape(shape)

    proto_value = None
    if proto.dtype == types_pb2.DT_FLOAT:
        proto_value = proto.float_val
    elif proto.dtype == types_pb2.DT_DOUBLE:
        proto_value = proto.double_val
    elif proto.dtype in [types_pb2.DT_INT32, types_pb2.DT_UINT8, types_pb2.DT_UINT16, types_pb2.DT_INT16, types_pb2.DT_INT8]:
        proto_value = proto.int_val
    elif proto.dtype == types_pb2.DT_INT64:
        proto_value = proto.int64_val
    elif proto.dtype == types_pb2.DT_BOOL:
        proto_value = proto.bool_val
    if proto_value is not None:
        if len(proto.float_val) == 1:
            return np.repeat(np.array(proto.float_val[0], dtype=np_dtype), num_elements).reshape(shape)
        return np.fromiter(proto.float_val, dtype=np_dtype).reshape(shape)

    raise TypeError("Unsupported type: %s" % proto.dtype)


def build_signature_def(inputs=None, outputs=None, method_name=None):
    signature_def = meta_graph_pb2.SignatureDef()
    if inputs is not None:
        for item in inputs:
            signature_def.inputs[item].CopyFrom(inputs[item])
    if outputs is not None:
        for item in outputs:
            signature_def.outputs[item].CopyFrom(outputs[item])
    if method_name is not None:
        signature_def.method_name = method_name
    return signature_def


PB_TO_NP = {
    types_pb2.DT_HALF: np.float16,
    types_pb2.DT_FLOAT: np.float32,
    types_pb2.DT_DOUBLE: np.float64,
    types_pb2.DT_INT32: np.int32,
    types_pb2.DT_UINT8: np.uint8,
    types_pb2.DT_UINT16: np.uint16,
    types_pb2.DT_UINT32: np.uint32,
    types_pb2.DT_UINT64: np.uint64,
    types_pb2.DT_INT16: np.int16,
    types_pb2.DT_INT8: np.int8,
    types_pb2.DT_STRING: np.object,
    types_pb2.DT_COMPLEX64: np.complex64,
    types_pb2.DT_COMPLEX128: np.complex128,
    types_pb2.DT_INT64: np.int64,
    types_pb2.DT_BOOL: np.bool,
}

NP_TO_PB = {
    np.float16: types_pb2.DT_HALF,
    np.float32: types_pb2.DT_FLOAT,
    np.float64: types_pb2.DT_DOUBLE,
    np.int32: types_pb2.DT_INT32,
    np.uint8: types_pb2.DT_UINT8,
    np.uint16: types_pb2.DT_UINT16,
    np.uint32: types_pb2.DT_UINT32,
    np.uint64: types_pb2.DT_UINT64,
    np.int16: types_pb2.DT_INT16,
    np.int8: types_pb2.DT_INT8,
    np.object: types_pb2.DT_STRING,
    np.complex64: types_pb2.DT_COMPLEX64,
    np.complex128: types_pb2.DT_COMPLEX128,
    np.int64: types_pb2.DT_INT64,
    np.bool_: types_pb2.DT_BOOL,
}

RPC_TIMEOUT = 3.0


class InferenceClient:
    def __init__(self, host, port=0):
        if port==0:
          #This will use unix socket domain
          channel = grpc_insecure_channel(host)
        else:
          channel = grpc_insecure_channel(host + ':' + str(port))
        self.stub = prediction_service_pb2_grpc.PredictionServiceStub(channel)

    
    def infer(self, inputs, model_name, model_version=None, outputs=[]):
        request = predict_pb2.PredictRequest()
        request.model_spec.name = model_name

        if model_version is not None:
            request.model_spec.version.value = model_version

        for input_name, input_data in inputs.items():
            request.inputs[input_name].CopyFrom(make_tensor_proto(np.stack(input_data, axis=0)))

        try:
            t0 = time.time()
            result = self.stub.Predict(request, RPC_TIMEOUT)
            t1 = time.time()
            print(f'Time for call to inference-server: {1000 * (t1 - t0):.0f} ms')
        except Exception as exc:
            print(exc)
            return False, {}
        if not outputs:
            output_data = {output_key: make_ndarray(tensor) for output_key, tensor in result.outputs.items()}
        else:
            output_data = {}
            for output_key in outputs:
                output_data[output_key] = make_ndarray(result.outputs[output_key])
        return True, output_data
