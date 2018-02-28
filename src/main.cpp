#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <fstream>
#include <sstream>
#include <ctime>
#include <set>
#include <iterator>
#include <dirent.h>
#include <cstring>
#include <stddef.h>
#include <sys/time.h>
#include <random>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "cudaSift.h"
#include "cudaImage.h"
#include "cuda_files.h"
#include "pca.h"
#include "falconn/lsh_nn_table.h"
#include "Eigen/Dense"
extern "C" {
#include "vl/generic.h"
#include "vl/gmm.h"
#include "vl/fisher.h"
#include "vl/mathop.h"
}

using namespace std;
using namespace falconn;
using namespace Eigen;

#define NUM_CLUSTERS 32 
#define SIZE 5248 //82 * 2 * 32
#define ROWS 200
#define TYPE float
#define DST_DIM 80
#define NUM_HASH_TABLES 20
#define NUM_HASH_BITS 24

#define LSH
//#define TRAIN
//#define FEATURE_CHECK

int querysizefactor, nn_num;

double wallclock (void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

void topNNN(double* values, int* ids, int num, double value, int id){
        int count = 0;
        while (count != num && value < values[count]){
                if(count != 0){
                        values[count-1] = values[count];
                        ids[count-1] = ids[count];
                }
                values[count] = value;
                ids[count] = id;
                count++;
        }
}

int ImproveHomography(SiftData &data, float *homography, int numLoops, float minScore, float maxAmbiguity, float thresh)
{
#ifdef MANAGEDMEM
  SiftPoint *mpts = data.m_data;
#else
  if (data.h_data==NULL)
    return 0;
  SiftPoint *mpts = data.h_data;
#endif
  float limit = thresh*thresh;
  int numPts = data.numPts;
  cv::Mat M(8, 8, CV_64FC1);
  cv::Mat A(8, 1, CV_64FC1), X(8, 1, CV_64FC1);
  double Y[8];
  for (int i=0;i<8;i++)
    A.at<double>(i, 0) = homography[i] / homography[8];
  for (int loop=0;loop<numLoops;loop++) {
    M = cv::Scalar(0.0);
    X = cv::Scalar(0.0);
    for (int i=0;i<numPts;i++) {
      SiftPoint &pt = mpts[i];
      if (pt.score<minScore || pt.ambiguity>maxAmbiguity)
        continue;
      float den = A.at<double>(6)*pt.xpos + A.at<double>(7)*pt.ypos + 1.0f;
      float dx = (A.at<double>(0)*pt.xpos + A.at<double>(1)*pt.ypos + A.at<double>(2)) / den - pt.match_xpos;
      float dy = (A.at<double>(3)*pt.xpos + A.at<double>(4)*pt.ypos + A.at<double>(5)) / den - pt.match_ypos;
      float err = dx*dx + dy*dy;
      float wei = limit / (err + limit);
      Y[0] = pt.xpos;
      Y[1] = pt.ypos;
      Y[2] = 1.0;
      Y[3] = Y[4] = Y[5] = 0.0;
      Y[6] = - pt.xpos * pt.match_xpos;
      Y[7] = - pt.ypos * pt.match_xpos;
      for (int c=0;c<8;c++)
        for (int r=0;r<8;r++)
          M.at<double>(r,c) += (Y[c] * Y[r] * wei);
      X += (cv::Mat(8,1,CV_64FC1,Y) * pt.match_xpos * wei);
      Y[0] = Y[1] = Y[2] = 0.0;
      Y[3] = pt.xpos;
      Y[4] = pt.ypos;
      Y[5] = 1.0;
      Y[6] = - pt.xpos * pt.match_ypos;
      Y[7] = - pt.ypos * pt.match_ypos;
      for (int c=0;c<8;c++)
        for (int r=0;r<8;r++)
          M.at<double>(r,c) += (Y[c] * Y[r] * wei);
      X += (cv::Mat(8,1,CV_64FC1,Y) * pt.match_ypos * wei);
    }
    cv::solve(M, X, A, cv::DECOMP_CHOLESKY);
  }
  int numfit = 0;
  for (int i=0;i<numPts;i++) {
    SiftPoint &pt = mpts[i];
    float den = A.at<double>(6)*pt.xpos + A.at<double>(7)*pt.ypos + 1.0;
    float dx = (A.at<double>(0)*pt.xpos + A.at<double>(1)*pt.ypos + A.at<double>(2)) / den - pt.match_xpos;
    float dy = (A.at<double>(3)*pt.xpos + A.at<double>(4)*pt.ypos + A.at<double>(5)) / den - pt.match_ypos;
    float err = dx*dx + dy*dy;
    if (err<limit)
      numfit++;
    pt.match_error = sqrt(err);
  }
  for (int i=0;i<8;i++)
    homography[i] = A.at<double>(i);
  homography[8] = 1.0f;
  return numfit;
}

int sift_gpu(char *image, float **siftres, float **siftframe, SiftData &siftData, int &w, int &h, bool online)
{
  cout << "sift gpu: " << image << endl;
  cv::Mat img;
  CudaImage cimg;
  int numPts;
  double start, finish, durationgmm;

  img = cv::imread(image, CV_LOAD_IMAGE_COLOR);
  if(online) cv::resize(img, img, cv::Size(), 1.0/querysizefactor, 1.0/querysizefactor);
  cv::cvtColor(img, img, CV_BGR2GRAY);
  img.convertTo(img, CV_32FC1);
  start = wallclock();
  w = img.cols;
  h = img.rows;
  std::cout << "Image size = (" << w << "," << h << ")" << std::endl;

  cimg.Allocate(w, h, iAlignUp(w, 128), false, NULL, (float*)img.data);
  cimg.Download();

  float initBlur = 1.0f;
  float thresh = 2.0f;
  InitSiftData(siftData, 1000, true, true);
  ExtractSift(siftData, cimg, 5, initBlur, thresh, 0.0f, false);

  numPts = siftData.numPts;
  *siftres = (float *)malloc(sizeof(float)*128*numPts);
  *siftframe = (float *)malloc(sizeof(float)*2*numPts);
  float* curRes = *siftres;
  float* curframe = *siftframe;
  SiftPoint* p = siftData.h_data;

  for(int i = 0; i < numPts; i++) {
    memcpy(curRes, p->data, 128*sizeof(float));
    curRes+=128;

    *curframe++ = p->xpos/w - 0.5;
    *curframe++ = p->ypos/h - 0.5;
    p++;
  }

  if(!online) FreeSiftData(siftData);

  finish = wallclock();
  durationgmm = (double)(finish - start);
  cout << "sift res " << numPts << " with time: " << durationgmm << endl;
  return numPts;
}

void onlineProcessing(char *image, SiftData &siftData, float *priors, float *means, float *covariances, vector<float> &enc_vec, float *projection, float *projectionCenter, bool online)
{
  double start, finish;
  double durationsift, durationgmm;
  int siftResult;
  //cout << "Process " << image << endl;
  float *siftresg;
  float *siftframe;
  int height, width;

  siftResult = sift_gpu(image, &siftresg, &siftframe, siftData, width, height, online);

  start = wallclock();

  float *dest = (float *)malloc(siftResult*82*sizeof(float));
  gpu_pca_mm(projection, projectionCenter, siftresg, dest, siftResult, DST_DIM);

  finish = wallclock();
  durationgmm = (double)(finish - start);
  cout << "pca encoding time: " << durationgmm << endl;
  start = wallclock();

  float enc[SIZE] = {0};
  gpu_gmm_1(covariances, priors, means, NULL, NUM_CLUSTERS, 82, siftResult, (82/2.0)*log(2.0*VL_PI), enc, NULL, dest);

  ///////////WARNING: add the other NOOP
  float sum = 0.0;
  for (int i = 0; i < SIZE; i++)
  {
    sum += enc[i] * enc[i];
  }
  for (int i = 0; i < SIZE; i++)
  {
    //WARNING: didn't use the max operation
    enc[i] /= sqrt(sum);
  }
  sum = 0.0;
  for (int i = 0; i < SIZE; i++)
  {
    sum += enc[i] * enc[i];
  }
  for (int i = 0; i < SIZE; i++)
  {
    //WARNING: didn't use the max operation
    enc[i] /= sqrt(sum);
  }
  
  enc_vec = vector<float>(enc, enc+SIZE);

  finish = wallclock();
  durationgmm = (double)(finish - start);
  cout << "fisher encoding time: " << durationgmm << endl;

  free(dest);
  free(siftresg);
  free(siftframe);
}

void test(float* priors, float* means, float* covariances, float* projection, float* projectionCenter, vector<float> train[], int trainSize)
{
    double dist, min;
    int correct = 0;
    double values[5];
    int ids[5];
    double start, finish, duration;
    SiftData tData[100];
    vector<float> test;
#ifdef LSH  
    vector<DenseVector<float>> lsh;
    for(int i = 0; i < trainSize; i++) {
      DenseVector<float> FV(SIZE);
      for(int j = 0; j < SIZE; j++) FV[j] = train[i][j];
      lsh.push_back(FV);
    }

    LSHConstructionParameters params = get_default_parameters<DenseVector<float>>((int_fast64_t)lsh.size(), (int_fast32_t)SIZE, DistanceFunction::EuclideanSquared, true);
    params.l = 32;
    params.k = 1;
    //cout << "lsh family: " << params.lsh_family << endl;
    cout << "hash tables: " << params.l << endl;
    cout << "hash functions: " << params.k << endl;

    auto tablet = construct_table<DenseVector<float>>(lsh, params);
    auto table = tablet->construct_query_object(100);
    table->set_num_probes(100);
    cout << "lsh tables preparing done" << endl;
#endif

    vector<char *> test_list;
    const char *test_path = "data/crop";
    DIR *d = opendir(test_path);
    struct dirent *cur_dir;
    while ((cur_dir = readdir(d)) != NULL)
    {
      if ((strcmp(cur_dir->d_name, ".") != 0) && (strcmp(cur_dir->d_name, "..") != 0))
      {
        char *file = new char[256];
        sprintf(file, "%s/%s", test_path, cur_dir->d_name);
        if (strstr(file, "jpg") != NULL)
        {
          test_list.push_back(file);
        }
      }
    }
    cout << endl << "--------------------------testing " << test_list.size() << " images----------------------" <<endl << endl;
    closedir(d);

    for (int i = 0; i < test_list.size(); i++)
    {
      onlineProcessing(test_list[i], tData[i], priors, means, covariances, test, projection, projectionCenter, true);
      start = wallclock();
#ifdef LSH      
      vector<int> result;
      DenseVector<float> t(SIZE);
      for(int j = 0; j < SIZE; j++) t[j] = test[j];
      table->find_k_nearest_neighbors(t, nn_num, &result);
      for(int idx = 0; idx < result.size(); idx++) {
        cout << result[idx] << " ";
        if(result[idx] == i) {
          correct++;
          cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!>>" << endl;
          break;
        }
      } 
#else
      for(int j = 0; j < nn_num; j++) {
          values[j] = 100000000 - j;
          ids[j] = -1;
      }
    
      for(int j = 0; j < trainSize; j++) {
        dist = norm(test, train[j], cv::NORM_L2);
     	topNNN(values, ids, nn_num, dist, j);
      }

      for(int j = nn_num - 1; j >=0; --j) {
	cout << ids[j] << " ";
	if(ids[j] == i) {
	  correct++;
	  cout << "==================================================>>" << endl;
          break;
        }
      }
#endif
      finish = wallclock();
      duration = (double)(finish - start);
      cout << "NNN searching time: " << duration << endl;
#if 0
      start = wallclock();

          MatchSiftData(rData[i], tData[i]);
          float homography[9];
          int numMatches;
          FindHomography(rData[i], homography, &numMatches, 10000, 0.00f, 0.80f, 5.0);
          int numFit = ImproveHomography(rData[i], homography, 5, 0.00f, 0.80f, 3.0);
          cout << "Number of original features: " <<  rData[i].numPts << " " << tData[i].numPts << endl;
          cout << "Matching features: " << numFit << " " << numMatches << " " << 100.0f*numFit/std::min(rData[i].numPts, tData[i].numPts) << "% " << endl;

      finish = wallclock();
      duration = (double)(finish - start);
      cout << "postprocessing time: " << duration << endl << endl;
#endif
      FreeSiftData(tData[i]);
    }
    cout << "correct: " <<correct <<endl;
}

bool mycompare(char* x, char* y) {
  //  cout<<"Compare "<<x<<", "<<y<<endl;
  if(strcmp(x, y)<=0) return 1;
  else return 0;
}

int main(int argc, char *argv[])
{
  if (argc < 3) {
    cout << "Usage: " << argv[0] << " size[s/m/l] NN#[1/2/3/4/5]" << endl;
    return 1;
  }

  if (argv[1][0] == 's') querysizefactor = 4;
  else if (argv[1][0] == 'm') querysizefactor = 2;
  else querysizefactor = 1;

  nn_num = argv[2][0] - '0';
  if (nn_num < 1 || nn_num > 5) nn_num = 5;

  clock_t start, finish;
  double duration;
  int numData, numBOV;

  float *means, *covariances, *priors, *posteriors;
  vl_size dimension = 82;

  gpu_init();
  InitCuda(0);

  float *sift_res;
  float *sift_frame;

  //The list of image file names
  //TODO: separate train and test list (train and validation sets comprise the training set
  vector<char *> whole_list;
  const char *home = "data/bk_train"; 
  DIR *d = opendir(home);
  struct dirent *cur_dir;  
  vector<char *> paths;
  while ((cur_dir = readdir(d)) != NULL)
  {
    if ((strcmp(cur_dir->d_name, ".") != 0) && (strcmp(cur_dir->d_name, "..") != 0))
    {
      //  char szTempDir[MAX_PATHH] = { 0 };
      //	  cout<<"This path "<<cur_dir->d_name<<endl;
      //	  char temppath[MAX_PATHH] = { 0 };
      char *temppath = new char[256];
      sprintf(temppath, "%s/%s", home, cur_dir->d_name);
      //	  cout<<"Temp "<<temppath<<", "<<strstr(temppath,"jpg")<<endl;

      paths.push_back(temppath);
    }
  }
  sort(paths.begin(), paths.end(), mycompare);

  for (int i = 0; i < paths.size(); i++)
  {
    DIR *subd = opendir(paths[i]);
    struct dirent *cur_subdir;
    while ((cur_subdir = readdir(subd)) != NULL)
    {
      if ((strcmp(cur_subdir->d_name, ".") != 0) && (strcmp(cur_subdir->d_name, "..") != 0))
      {
        char *file = new char[256];
        sprintf(file, "%s/%s", paths[i], cur_subdir->d_name);
        if (strstr(file, "jpg") != NULL)
        {
          whole_list.push_back(file);
          if(whole_list.size() == 1000) break;
        }
      }
    }
    closedir(subd);
    //break;
  }
    cout << endl << "---------------------in total " << whole_list.size() << " images------------------------" << endl << endl;
  closedir(d);

  float *final_res = (float *)malloc(ROWS * whole_list.size() * 128 * sizeof(float));
  float *final_frame = (float *)malloc(ROWS * whole_list.size() * 128 * sizeof(float));

  double start_time;

#ifdef TRAIN
    //////////////////train encoder ////////////////
    //////// STEP 0: obtain sample image descriptors
    std::set<int>::iterator iter;
    start_time = wallclock();

    for (int i = 0; i != whole_list.size(); ++i)
    {
      char imagename[256], imagesizename[256];
      int height, width;
      float *pca_desc;
      //get descriptors
      cout << "Train file " << i << ": " << whole_list[i] << endl;
      //    if(count == 2)
      SiftData siftData;
      int pre_size = sift_gpu(whole_list[i], &sift_res, &sift_frame, siftData, width, height, false);
      cout << "pre size: " << pre_size <<endl;

#ifdef FEATURE_CHECK
      if(pre_size < ROWS) remove(whole_list[i]);
      continue;
#endif

      srand(1);
      std::set<int> indices;
      for (int it = 0; it < ROWS; it++)
      {
        int rand_t = rand() % pre_size;
        std::pair<std::set<int>::iterator, bool> ret = indices.insert(rand_t);
        while (ret.second == false)
        {
          rand_t = rand() % pre_size;
          ret = indices.insert(rand_t);
        }
      }
      std::set<int>::iterator iter;
      int it = 0;
      for (iter = indices.begin(); iter != indices.end(); iter++)
      {
        for (int k = 0; k < 128; k++)
        {
          //	cout<<"It is "<<*iter<<endl;
          final_res[(ROWS * i + it) * 128 + k] = sift_res[(*iter) * 128 + k];
        }
        for (int k = 0; k < 2; k++)
        {
          final_frame[(ROWS * i + it) * 2 + k] = sift_frame[(*iter) * 2 + k];
        }

        it++;
      }
      free(sift_res);
      free(sift_frame);
    }
    /////////////STEP 1: PCA
    const int num_variables = 128;
    const int num_records = ROWS * whole_list.size();

    stats::pca pca(num_variables);
    //  pca.set_do_bootstrap(true, 100);

    float *projectionCenter = (float *)malloc(128 * sizeof(float));
    float *projection = (float *)malloc(128 * 80 * sizeof(float));

    int j = 0;
    for (int i = 0; i < num_records; ++i)
    {
      vector<double> record(num_variables);
      for (auto value = record.begin(); value != record.end(); ++value)
      {
        *value = final_res[j];
        j++;
      }
      pca.add_record(record);
    }
    pca.solve_for_vlfeat();

    const auto means1 = pca.get_mean_values();
    for (int i = 0; i < 128; i++)
    {
      projectionCenter[i] = means1[i];
    }
    for (int i = 0; i < 80; i++)
    {
      const auto eigenv = pca.get_eigenvector(i);
      for (j = 0; j < 128; j++)
      {
        projection[i * 128 + j] = eigenv[j];
      }
    }
///////////////////////////STEP 2  (optional): geometrically augment the features
//////////////////////////////////////////
    int pre_size = num_records;
    float *dest = (float *)malloc(pre_size * (DST_DIM + 2) * sizeof(float));

    gpu_pca_mm(projection, projectionCenter, final_res, dest, pre_size, DST_DIM);
    for (int i = 0; i < pre_size; i++)
    {
      //    float halfwidth = ((float)width)/2;
      dest[i * (DST_DIM + 2) + DST_DIM] = final_frame[i * 2];
      dest[i * (DST_DIM + 2) + DST_DIM + 1] = final_frame[i * 2 + 1];
    }
    free(final_frame);
    free(final_res);

    cout << "End of step 2" << endl;
    //////////////////////STEP 3  learn a GMM vocabulary
    //////////////////////////
    numData = pre_size;
    //vl_twister
    VlRand *rand;
    rand = vl_get_rand();
    vl_rand_seed(rand, 1);

    VlGMM *gmm = vl_gmm_new(VL_TYPE_FLOAT, dimension, NUM_CLUSTERS);
    ///////////////////////WARNING: should set these parameters
    vl_gmm_set_initialization(gmm, VlGMMKMeans);
    //Compute V
    double denom = pre_size - 1;
    double xbar[82], V[82];
    for (int i = 0; i < dimension; i++)
    {
      xbar[i] = 0.0;
      for (int j = 0; j < numData; j++)
      {
        xbar[i] += (double)dest[j * dimension + i];
      }
      xbar[i] /= (double)pre_size;
    }
    for (int i = 0; i < dimension; i++)
    {
      double absx = 0.0;
      for (int j = 0; j < numData; j++)
      {
        double tempx = (double)dest[j * dimension + i] - xbar[i];
        absx += abs(tempx) * abs(tempx);
      }
      V[i] = absx / denom;
    }

    //Get max(V)
    double maxNum = V[0];
    for (int i = 1; i < dimension; i++)
    {
      if (V[i] > maxNum)
      {
        maxNum = V[i];
      }
    }
    maxNum = maxNum * 0.0001;
    vl_gmm_set_covariance_lower_bound(gmm, (double)maxNum);
    cout << "Lower bound " << maxNum << endl;
    vl_gmm_set_verbosity(gmm, 1);
    vl_gmm_set_max_num_iterations(gmm, 100);
    printf("vl_gmm: maxNumIterations = %d\n", vl_gmm_get_max_num_iterations(gmm));
    printf("vl_gmm: numRepetitions = %d\n", vl_gmm_get_num_repetitions(gmm));
    printf("vl_gmm: data type = %s\n", vl_get_type_name(vl_gmm_get_data_type(gmm)));
    printf("vl_gmm: data dimension = %d\n", dimension);
    printf("vl_gmm: num. data points = %d\n", numData);
    printf("vl_gmm: num. Gaussian modes = %d\n", NUM_CLUSTERS);
    printf("vl_gmm: lower bound on covariance = [");
    printf(" %f %f ... %f]\n",
           vl_gmm_get_covariance_lower_bounds(gmm)[0],
           vl_gmm_get_covariance_lower_bounds(gmm)[1],
           vl_gmm_get_covariance_lower_bounds(gmm)[dimension - 1]);
    //  VlGMM *gmm = vl_gmm_new(VL_TYPE_FLOAT, dimension, numComponents);

    double gmmres = vl_gmm_cluster(gmm, dest, numData);
    free(dest);
    cout << "GMM ending cluster." << endl;

    priors = (TYPE *)vl_gmm_get_priors(gmm);
    means = (TYPE *)vl_gmm_get_means(gmm);
    covariances = (TYPE *)vl_gmm_get_covariances(gmm);
    cout << "End of encoder " << endl;
    cout << "Training time " << wallclock() - start_time << endl;
    ///////////////END train encoer//////////

    ofstream out1("params/priors", ios::out | ios::binary);
    out1.write((char *)priors, sizeof(float) * NUM_CLUSTERS);
    out1.close();

    ofstream out2("params/means", ios::out | ios::binary);
    out2.write((char *)means, sizeof(float) * dimension * NUM_CLUSTERS);
    out2.close();

    ofstream out3("params/covariances", ios::out | ios::binary);
    out3.write((char *)covariances, sizeof(float) * dimension * NUM_CLUSTERS);
    out3.close();

    ofstream out4("params/projection", ios::out | ios::binary);
    out4.write((char *)projection, sizeof(float) * 80 * 128);
    out4.close();

    ofstream out5("params/projectionCenter", ios::out | ios::binary);
    out5.write((char *)projectionCenter, sizeof(float) * 128);
    out5.close();
#else
    priors = (TYPE *)vl_malloc(sizeof(float) * NUM_CLUSTERS);
    means = (TYPE *)vl_malloc(sizeof(float) * dimension * NUM_CLUSTERS);
    covariances = (TYPE *)vl_malloc(sizeof(float) * dimension * NUM_CLUSTERS);
    float *projection = (float *)malloc(128 * sizeof(float) * 80);
    float *projectionCenter = (float *)malloc(128 * sizeof(float));

    ifstream in1("params/priors", ios::in | ios::binary);
    in1.read((char *)priors, sizeof(float) * NUM_CLUSTERS);
    in1.close();

    ifstream in2("params/means", ios::in | ios::binary);
    in2.read((char *)means, sizeof(float) * dimension * NUM_CLUSTERS);
    in2.close();

    ifstream in3("params/covariances", ios::in | ios::binary);
    in3.read((char *)covariances, sizeof(float) * dimension * NUM_CLUSTERS);
    in3.close();

    ifstream in4("params/projection", ios::in | ios::binary);
    in4.read((char *)projection, sizeof(float) * 128 * 80);
    in4.close();

    ifstream in5("params/projectionCenter", ios::in | ios::binary);
    in5.read((char *)projectionCenter, sizeof(float) * 128);
    in5.close();
#endif
    gpu_copy(covariances, priors, means, NUM_CLUSTERS, dimension);

    vector<float> train[whole_list.size()];
    //Encode train files
    for (int i = 0; i < whole_list.size(); i++)
    {
      cout << i << ": ";
      SiftData tmp;
      onlineProcessing(whole_list[i], tmp, priors, means, covariances, train[i], projection, projectionCenter, false);
    }
    cout << endl << "---------------------in total " << whole_list.size() << " reference images------------------------" << endl << endl;

    test(priors, means, covariances, projection, projectionCenter, train, whole_list.size());

    gpu_free();
    free(projection);
    free(projectionCenter);
    free(priors);
    free(means);
    free(covariances);

  return 0;
}
