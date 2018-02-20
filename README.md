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
  - `falconn` , cpu lsh
  - `CudaSift` , Cuda version of Sift with detection, extraction, matching
  - `libpca` , cpu pca training
  - `Eigen` , Dense data structure
  - `vlfeat` , cpu gmm training
  - `Cuda`

### Installation

Besides the libraries contained in this repo, you may first install `OpenCV`, `falconn`, `Eigen`, `Cuda`. 
Build and run:

```sh
$ cd build
$ make
$ cd ..
$ ./gpu_fv
```

### Structure and Development

  - `src` , source codes
  - `build` , build products
  - `params` , PCA and GMM parameters
  - `res` , running results

Modify `main.cpp` for any tests and developments. The compiling and running logic is controlled with MACRO in `main.cpp`.
