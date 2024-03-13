# Cifre

A Python / C++ Alpha-Zero implementation. Cifre is Old French for
"nought" or "zero".

Cifre uses the containerized [Triton Inference
Server](https://github.com/triton-inference-server/server) to make
predictions with the DL model. Docker must be installed on your system
as described [Installing Docker for NVIDIA
GPUs](https://docs.nvidia.com/ai-enterprise/deployment-guide-vmware/0.1.0/docker.html).
Once Docker is installed, you can run Triton with the following
commands:

```bash
mkdir -p ${CIFRE_REPODIR}
docker run --gpus=1 --rm --net=host -v ${CIFRE_REPODIR}:/cifre/models nvcr.io/nvidia/tritonserver:24.01-py3 tritonserver --model-control-mode=explicit --model-repository=/cifre/models
```

Where `${CIFRE_REPODIR}` is the full path to the model repository
directory specified with the *--repodir* flag when you run the
`cifre_train` executable. `${CIFRE_REPODIR}` must exist before you run
the Docker command. For example, use the following commands to start
Triton and train a model for the *celcath* game on an 8x8 board, using
`/tmp/cifre/models` to hold the Triton repository, and
`/tmp/cifre/train` to hold the training data.

```base
mkdir -p /tmp/cifre/models /tmp/cifre/train
docker run --gpus=1 --rm --net=host -v /tmp/cifre/models:/cifre/models nvcr.io/nvidia/tritonserver:24.01-py3 tritonserver --model-control-mode=explicit --model-repository=/cifre/models
cd python
./cifre_train.py --repo-dir=/tmp/cifre/models --train-dir=/tmp/cifre/train --game=celcath --game-shape=8,8
```

## Build

There is a build issue in one of the dependent cmake builds that
requires a clean build. To perform a clean build do the following:

```base
rm -fr build
cd build
cmake ..
make -j
```

Then disable the problematic part of the build:

```base
cmake -DBUILD_TRITON_CLIENT=OFF ..
```

Then you can perform incremental builds and/or re-run cmake to change
build options, and perform as many builds as required. For example:

```base
cmake -DASAN=ON -DCMAKE_BUILD_TYPE=Debug ..
make -j install
```
