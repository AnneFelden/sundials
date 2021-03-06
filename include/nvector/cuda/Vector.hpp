/*
 * -----------------------------------------------------------------
 * Programmer(s): Slaven Peles @ LLNL
 * -----------------------------------------------------------------
 * LLNS Copyright Start
 * Copyright (c) 2014, Lawrence Livermore National Security
 * This work was performed under the auspices of the U.S. Department
 * of Energy by Lawrence Livermore National Laboratory in part under
 * Contract W-7405-Eng-48 and in part under Contract DE-AC52-07NA27344.
 * Produced at the Lawrence Livermore National Laboratory.
 * All rights reserved.
 * For details, see the LICENSE file.
 * LLNS Copyright End
 * -----------------------------------------------------------------
 */


/**
 * Vector class
 *
 * Manages vector data layout for CUDA implementation of N_Vector.
 *
 */

#ifndef _NVECTOR_CUDA_HPP_
#define _NVECTOR_CUDA_HPP_

#include <cstdlib>
#include <iostream>

#include <cuda_runtime.h>
#include "ThreadPartitioning.hpp"

#include <sundials/sundials_config.h>
#include <sundials/sundials_mpi.h>

#if SUNDIALS_MPI_ENABLED
#include <nvector/nvector_mpicuda.h>
#else
#include <nvector/nvector_cuda.h>
#endif

namespace suncudavec
{

template <typename T, typename I>
class Vector : public _N_VectorContent_Cuda
{
public:
  Vector(I N)
  : size_(N),
    mem_size_(N*sizeof(T)),
    ownPartitioning_(true)
  {
    // Set partitioning
    partStream_ = new StreamPartitioning<T, I>(N, 256);
    partReduce_ = new ReducePartitioning<T, I>(N, 256);

    allocate();
  }

  Vector(SUNMPI_Comm comm, I N, I Nglobal)
  : size_(N),
    mem_size_(N*sizeof(T)),
    global_size_(Nglobal),
    comm_(comm)
  {
    // Set partitioning
    partStream_ = new StreamPartitioning<T, I>(N, 256);
    partReduce_ = new ReducePartitioning<T, I>(N, 256);

    allocate();
  }

  /// Copy constructor does not copy values
  explicit Vector(const Vector& v)
  : size_(v.size()),
    mem_size_(size_*sizeof(T)),
    global_size_(v.global_size_),
    partStream_(v.partStream_),
    partReduce_(v.partReduce_),
    ownPartitioning_(false),
    comm_(v.comm_)
  {
    allocate();
  }

  ~Vector()
  {
    if (ownPartitioning_)
    {
      delete partReduce_;
      delete partStream_;
    }
    clear();
  }


  void allocate()
  {
    cudaError_t err;
    h_vec_ = static_cast<T*>(malloc(mem_size_));
    if(h_vec_ == NULL)
      std::cerr << "Failed to allocate host vector!\n";
    err = cudaMalloc((void**) &d_vec_, mem_size_);
    if(err != cudaSuccess)
      std::cerr << "Failed to allocate device vector (error code " << err << ")!\n";
  }

  void clear()
  {
    free(h_vec_);
    cudaError_t err = cudaFree(d_vec_);
    if(err != cudaSuccess)
      std::cerr << "Failed to free device vector (error code " << err << ")!\n";
  }

  int size() const
  {
    return size_;
  }

  int sizeGlobal() const
  {
    return global_size_;
  }

  SUNMPI_Comm comm()
  {
    return comm_;
  }

  T* host()
  {
    return h_vec_;
  }

  const T* host() const
  {
    return h_vec_;
  }

  T* device()
  {
    return d_vec_;
  }

  const T* device() const
  {
    return d_vec_;
  }

  void copyToDev()
  {
    cudaError_t err = cudaMemcpy(d_vec_, h_vec_, mem_size_, cudaMemcpyHostToDevice);
    if(err != cudaSuccess)
      std::cerr << "Failed to copy vector from host to device (error code " << err << ")!\n";
  }

  void copyFromDev()
  {
    cudaError_t err = cudaMemcpy(h_vec_, d_vec_, mem_size_, cudaMemcpyDeviceToHost);
    if(err != cudaSuccess)
      std::cerr << "Failed to copy vector from device to host (error code " << err << ")!\n";
  }

  ThreadPartitioning<T, I>& partStream() const
  {
    return *partStream_;
  }

  ThreadPartitioning<T, I>& partReduce() const
  {
    return *partReduce_;
  }

private:
  I size_;
  I mem_size_;
  I global_size_;
  T* h_vec_;
  T* d_vec_;
  ThreadPartitioning<T, I>* partStream_;
  ThreadPartitioning<T, I>* partReduce_;
  bool ownPartitioning_;
  SUNMPI_Comm comm_;
};



// Extract Vector from N_Vector
template <typename T, typename I>
inline Vector<T, I>* extract(N_Vector v)
{
  return static_cast<Vector<T, I>*>(v->content);
}

// Get Vector device data
template <typename T, typename I>
inline T* getDevData(N_Vector v)
{
  Vector<T,I>* vp = static_cast<Vector<T, I>*>(v->content);
  return vp->device();
}

// Get Vector host data
template <typename T, typename I>
inline T* getHostData(N_Vector v)
{
  Vector<T,I>* vp = static_cast<Vector<T, I>*>(v->content);
  return vp->host();
}

// Get Vector length
template <typename T, typename I>
inline I getSize(N_Vector v)
{
  Vector<T,I>* vp = static_cast<Vector<T, I>*>(v->content);
  return vp->size();
}

// Get Vector length
template <typename T, typename I>
inline I getGlobalSize(N_Vector v)
{
  Vector<T,I>* vp = static_cast<Vector<T, I>*>(v->content);
  return vp->sizeGlobal();
}

// Get MPI communicator
template <typename T, typename I>
inline SUNMPI_Comm getMPIComm(N_Vector v)
{
  Vector<T,I>* vp = static_cast<Vector<T, I>*>(v->content);
  return vp->comm();
}


} // namespace suncudavec




#endif // _NVECTOR_CUDA_HPP_
