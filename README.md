# Cifre

A Python / C++ Alpha-Zero implementation.
Cifre is Old French for "nought" or "zero".

Cifre uses the containerized [Triton Inference
Server](https://github.com/triton-inference-server/server) to make
predictions with the DL model. Docker must be installed on your system
as described [Installing Docker for NVIDIA
GPUs](https://docs.nvidia.com/ai-enterprise/deployment-guide-vmware/0.1.0/docker.html).
Once Docker is installed, you can retrieve and run it with the
following commands:

```bash
docker run --gpus=1 --rm --net=host -v ${CIFRE_WORKDIR}:/cifre nvcr.io/nvidia/tritonserver:24.01-py3 tritonserver --model-control-mode=explicit --model-repository=/cifre/models
```

Where `${CIFRE_WORKDIR}` is the full path to the Cifre workdir
specified with the *--workdir* flag when you run the `cifre`
executable. `${CIFRE_WORKDIR}/models` must exist before you run the
Docker command. By default you can use `/tmp/cifre` as the value of
`${CIFRE_WORKDIR}` and then you do not need to specify
*--workdir*. For example, if you used
`${CIFRE_WORKDIR}=/tmp/my/workdir` in the Docker command above, then
you would run Cifre as shown below.

```base
cifre --workdir=/tmp/my/workdir ...
```
