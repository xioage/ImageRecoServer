FROM alantrrs/cuda-opencv

RUN apt-get update && apt-get install -y --no-install-recommends \
        cuda-samples-$CUDA_PKG_VERSION && \
        rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

RUN git clone https://xioage:uNt2av6md85vuzM4Gzfa@bitbucket.org/cloudridar/gpuimagereco.git && \
        cd gpuimagereco && git submodule init && git submodule update

WORKDIR /workspace/gpuimagereco

RUN cd lib/cudasift && sed -i 's/executable/library/g' CMakeLists.txt && \
        cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release . && make && cd .. && cd ..

RUN cd build && make && cd ..

ENV mode s
ENV size l
ENV nn_num 5
ENV port 51717
ENV queryNum 1

CMD ["sh", "-c", "./gpu_fv ${mode} ${size} ${nn_num} ${port} ${queryNum}"]
