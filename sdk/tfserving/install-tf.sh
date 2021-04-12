#!/bin/sh -e

TF_DIR=$1
TFS_DIR=$2
INSTALL_PREFIX=$3
PY_INSTALL_DIR=$INSTALL_PREFIX/usr/lib/python3/site-packages

install -d "$PY_INSTALL_DIR"

PROTO_FILES="$TF_DIR/tensorflow/core/example/*.proto
             $TF_DIR/tensorflow/core/framework/*.proto \
             $TF_DIR/tensorflow/core/protobuf/*.proto \
             $TFS_DIR/tensorflow_serving/apis/*.proto \
            "

PROTO_FILES_GRPC="$TFS_DIR/tensorflow_serving/apis/predict.proto \
                  $TFS_DIR/tensorflow_serving/apis/prediction_service.proto \
                 "

python3 -m grpc_tools.protoc -I "$TF_DIR" -I "$TFS_DIR" --python_out="$PY_INSTALL_DIR" $PROTO_FILES

python3 -m grpc_tools.protoc -I "$TF_DIR" -I "$TFS_DIR" --grpc_python_out="$PY_INSTALL_DIR" $PROTO_FILES_GRPC

find "$PY_INSTALL_DIR" -type d -mindepth 1 -exec touch {}/__init__.py \;
