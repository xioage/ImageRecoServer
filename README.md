## Image Recognition Pipeline with GPU

This project contains a Cuda version of a complete image recognition pipeline, with:
  - GPU feature extraction, 
  - GPU PCA, 
  - GPU GMM & FV, 
  - CPU LSH, 
  - GPU Feature Matching, 
  - and CPU Pose Calculation.

### Dependencies

  - `OpenCV` , tested with ver 3.3
  - `Cuda` , tested with ver 8.0
  - `falconn` , cpu lsh
  - `CudaSift` , Cuda version of Sift with detection, extraction, matching
  - `libpca` , cpu pca training
  - `Eigen` , Dense data structure
  - `vlfeat` , cpu gmm training

### Installation

Besides the libraries contained in this repo, you may first install `OpenCV` and `Cuda`. 
Build and run:

```sh
$ cd build
$ make
$ cd ..
$ ./gpu_fv
```

### Structure and Development

  - `src` , source codes
  - `lib` , dependencies
  - `data` , data files
  - `build` , build products
  - `params` , PCA and GMM parameters

Modify `main.cpp` for any tests and developments. The compiling and running logic is controlled with MACRO in `main.cpp`.
