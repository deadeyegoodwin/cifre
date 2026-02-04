# Cifre

A Python / C++ Alpha-Zero
implementation. [Cifre](https://en.wiktionary.org/wiki/cifre) is a
Middle English word meaning the mathematical symbol representing the
number zero.

To run the training application, cifre_train.py, it is recommended
that you use anaconda. The following command creates an appropriate
environment called `cifre` and activates it.

```bash
conda env create -f environment.yml
conda activate cifre
```

Cifre uses the containerized [Triton Inference
Server](https://github.com/triton-inference-server/server) to make
predictions with the DL model. Docker must be installed on your system
as described [Installing Docker for NVIDIA
GPUs](https://docs.nvidia.com/ai-enterprise/deployment-guide-vmware/0.1.0/docker.html).
Once Docker is installed, you can run Triton with the following
commands:

```bash
mkdir -p ${CIFRE_REPODIR}
docker run --gpus=1 --rm --net=host -v ${CIFRE_REPODIR}:/cifre/models nvcr.io/nvidia/tritonserver:25.01-py3 tritonserver --model-control-mode=explicit --model-repository=/cifre/models
```

Where `${CIFRE_REPODIR}` is the full path to the model repository
directory specified with the *--repodir* flag when you run the
`cifre_train` executable. `${CIFRE_REPODIR}` must exist before you run
the Docker command. For example, use the following commands to start
Triton and train a model for the *connect4* game on a 6x7 board, using
`/tmp/cifre/models` to hold the Triton repository, and
`/tmp/cifre/train` to hold the training data.

```bash
# Launch triton in bash shell; triton log will be written to stdout/stderr
mkdir -p /tmp/cifre/models /tmp/cifre/train
docker run --gpus=1 --rm --net=host -v /tmp/cifre/models:/cifre/models nvcr.io/nvidia/tritonserver:25.01-py3 tritonserver --model-control-mode=explicit --model-repository=/cifre/models

# In another shell activate conda as described above and then perform following
# steps. The game-specific invocation of cifre_train.py and more information is
# found in the game links below.
cd python
rm -fr /tmp/cifre/models/*
rm -fr /tmp/cifre/train/*
./cifre_train.py --repo-dir=/tmp/cifre/models --train-dir=/tmp/cifre/train --game=connect4 --epoch-count=16 --iteration-count=4 --game-count=100
```

## Games

* [Connect 4](src/connect4/README.md)
* [Othello](src/othello/README.md)
* [Gettysburg](src/gettysburg/README.md)

## Using TensorRT

You can instruct Cifre to generate a TensorRT version of your model
which will typically result in much higher performance than the
default ONNX version of the model.

As a one-time setup you must install the TensorRT docker image that
corresponds to the version of Triton Server that you are using. For
example, if you use the 25.01-py3 version of Triton, you would do the
following to fetch the appropriate TensorRT docker image.

```bash
docker pull nvcr.io/nvidia/tensorrt:25.01-py3
```

Then, when training with cifre_train.py, add the command-line argument
*--model-kind=tensorrt:25.01*.

## Build

There is a build issue in one of the dependent cmake builds that
requires a clean build. To perform a clean build do the following:

```bash
rm -fr build
mkdir build
cd build
cmake ..
make -j
```

Then disable the problematic part of the build:

```bash
cmake -DBUILD_TRITON_CLIENT=OFF ..
```

Then use the following to install a full release build. Artifacts are
installed into `install/`.

```bash
make -j install
```

You can then perform incremental builds and/or re-run cmake to change
build options. For example:

```bash
cmake -DASAN=ON -DCMAKE_BUILD_TYPE=Debug ..
make -j install
```
