/***************************************************************************
                                nvd_kernel.h
                             -------------------
                               W. Michael Brown

  Utilities for dealing with CUDA Driver kernels

 __________________________________________________________________________
    This file is part of the Geryon Unified Coprocessor Library (UCL)
 __________________________________________________________________________

    begin                : Tue Feb 9 2010
    copyright            : (C) 2010 by W. Michael Brown
    email                : brownw@ornl.gov
 ***************************************************************************/

/* -----------------------------------------------------------------------
   Copyright (2010) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under 
   the Simplified BSD License.
   ----------------------------------------------------------------------- */
 
#ifndef NVD_KERNEL
#define NVD_KERNEL

#include "nvd_device.h"
#include <fstream>

namespace ucl_cudadr {

class UCL_Texture;
    
/// Class storing 1 or more kernel functions from a single string or file
class UCL_Program {
 public:
  inline UCL_Program(UCL_Device &device) {}
  inline ~UCL_Program() {}

  /// Initialize the program with a device
  inline void init(UCL_Device &device) { }

  /// Clear any data associated with program
  /** \note Must call init() after each clear **/
  inline void clear() { }

  /// Load a program from a file and compile with flags
  inline int load(const char *filename, const char *flags="",
                  std::string *log=NULL) {
    std::ifstream in(filename);
    if (!in || in.is_open()==false) {
      #ifndef UCL_NO_EXIT 
      std::cerr << "UCL Error: Could not open kernel file: " 
                << filename << std::endl;
      exit(1);
      #endif
      return UCL_FILE_NOT_FOUND;
    }
  
    std::string program((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    in.close();
    return load_string(program.c_str(),flags,log);
  }
  
  /// Load a program from a string and compile with flags
  inline int load_string(const char *program, const char *flags="",
                         std::string *log=NULL) {
    if (std::string(flags)=="BINARY")
      return load_binary(program);
    const unsigned int num_opts=2;
    CUjit_option options[num_opts];
    void *values[num_opts];

    // set up size of compilation log buffer
    options[0] = CU_JIT_INFO_LOG_BUFFER_SIZE_BYTES;
    values[0] = (void *)(int)10240;
    // set up pointer to the compilation log buffer
    options[1] = CU_JIT_INFO_LOG_BUFFER;
    char clog[10240];
    values[1] = clog;

    CUresult err=cuModuleLoadDataEx(&_module,program,num_opts,
                                    options,(void **)values);
                                        
    if (log!=NULL)
      *log=std::string(clog);
      
    if (err != CUDA_SUCCESS) {
      #ifndef UCL_NO_EXIT                                                 
      std::cerr << std::endl
                << "----------------------------------------------------------\n"
                << " UCL Error: Error compiling PTX Program...\n"
                << "----------------------------------------------------------\n";
      std::cerr << log << std::endl;
      #endif
      return UCL_COMPILE_ERROR;
    }
    
    return UCL_SUCCESS;
  }                                      
                              
  /// Load a precompiled program from a file
  inline int load_binary(const char *filename) {
    CUmodule _module;
    CUresult err = cuModuleLoad(&_module,filename);
    if (err==301) {
      #ifndef UCL_NO_EXIT 
      std::cerr << "UCL Error: Could not open binary kernel file: " 
                << filename << std::endl;
      exit(1);
      #endif
      return UCL_FILE_NOT_FOUND;
    } else if (err!=CUDA_SUCCESS) {
      #ifndef UCL_NO_EXIT 
      std::cerr << "UCL Error: Error loading binary kernel file: " 
                << filename << std::endl;
      exit(1);
      #endif
      return UCL_FILE_NOT_FOUND;
    }
    //int ucl_error=UCL_SUCCESS;
    //if (err==301)
    //  return UCL_FILE_NOT_FOUND;
    //else if (err!=CUDA_SUCCESS)
    //  return UCL_ERROR;
    return UCL_SUCCESS;
  }
   
  friend class UCL_Kernel;
 private:
  CUmodule _module;
  friend class UCL_Texture;
};

/// Class for dealing with OpenCL kernels
class UCL_Kernel {
 public:
  UCL_Kernel() : _dimensions(1), _num_args(0), _param_size(0) 
    { _num_blocks[0]=0; }
  
  UCL_Kernel(UCL_Program &program, const char *function) : 
    _dimensions(1), _num_args(0), _param_size(0) 
    { _num_blocks[0]=0; set_function(program,function); }
  
  ~UCL_Kernel() {}

  /// Clear any function associated with the kernel
  inline void clear() { }

  /// Get the kernel function from a program
  /** \ret UCL_ERROR_FLAG (UCL_SUCCESS, UCL_FILE_NOT_FOUND, UCL_ERROR) **/
  inline int set_function(UCL_Program &program, const char *function) {
    CUresult err=cuModuleGetFunction(&_kernel,program._module,function);
    if (err!=CUDA_SUCCESS) {
      #ifndef UCL_NO_EXIT
      std::cerr << "UCL Error: Could not find function: " << function
                << " in program.\n";
      exit(1);
      #endif
      return UCL_FUNCTION_NOT_FOUND;
    }
    return UCL_SUCCESS;
  }

  /// Set the kernel argument.
  /** If not a device pointer, this must be repeated each time the argument
    * changes 
    * \note To set kernel parameter i (i>0), parameter i-1 must be set **/
  template <class dtype>
  inline void set_arg(const unsigned index, dtype *arg) {
    if (index==_num_args)
      add_arg(arg);
    else if (index<_num_args)
      CU_SAFE_CALL(cuParamSetv(_kernel, _offsets[index], arg, sizeof(dtype)));
    else
      assert(0==1); // Must add kernel parameters in sequential order 
  }
 
  /// Add a kernel argument.
  inline void add_arg(const CUdeviceptr* const arg) {
    void* ptr = (void*)(size_t)(*arg);
    _param_size = (_param_size + __alignof(ptr) - 1) & ~(__alignof(ptr) - 1);
    CU_SAFE_CALL(cuParamSetv(_kernel, _param_size, &ptr, sizeof(ptr)));
    _offsets.push_back(_param_size);
    _param_size+=sizeof(ptr);
    _num_args++;
  }

  /// Add a kernel argument.
  template <class dtype>
  inline void add_arg(const dtype* const arg) {
    _param_size = (_param_size+__alignof(dtype)-1) & ~(__alignof(dtype)-1);
    CU_SAFE_CALL(cuParamSetv(_kernel,_param_size,(void*)arg,sizeof(dtype)));
    _offsets.push_back(_param_size);
    _param_size+=sizeof(dtype);
    _num_args++;
  }

  /// Set the number of thread blocks and the number of threads in each block
  /** \note This should be called after all arguments have been added **/
  inline void set_size(const size_t num_blocks, const size_t block_size) { 
    _dimensions=1; 
    _num_blocks[0]=num_blocks; 
    _num_blocks[1]=1; 
    CU_SAFE_CALL(cuFuncSetBlockShape(_kernel,block_size,1,1));
  }

  /// Set the number of thread blocks and the number of threads in each block
  inline void set_size(const size_t num_blocks_x, const size_t num_blocks_y,
                       const size_t block_size_x, const size_t block_size_y) { 
    _dimensions=2; 
    _num_blocks[0]=num_blocks_x; 
    _num_blocks[1]=num_blocks_y; 
    CU_SAFE_CALL(cuFuncSetBlockShape(_kernel,block_size_x,block_size_y,1));
  }
  
  /// Set the number of thread blocks and the number of threads in each block
  inline void set_size(const size_t num_blocks_x, const size_t num_blocks_y,
                       const size_t block_size_x, 
                       const size_t block_size_y, const size_t block_size_z) {
    _dimensions=2; 
    _num_blocks[0]=num_blocks_x; 
    _num_blocks[1]=num_blocks_y; 
    CU_SAFE_CALL(cuFuncSetBlockShape(_kernel,block_size_x,block_size_y,
                                       block_size_z));
  }

  /// Run the kernel in the default command queue
  inline void run() {
    CU_SAFE_CALL(cuParamSetSize(_kernel,_param_size));
    CU_SAFE_CALL(cuLaunchGridAsync(_kernel,_num_blocks[0],_num_blocks[1],0));
  }
  
  /// Run the kernel in the specified command queue
  inline void run(command_queue &cq) {
    CU_SAFE_CALL(cuParamSetSize(_kernel,_param_size));
    CU_SAFE_CALL(cuLaunchGridAsync(_kernel,_num_blocks[0],_num_blocks[1],cq));
  }
  
  /// Clear any arguments associated with the kernel
  inline void clear_args() { _num_args=0; _offsets.clear(); _param_size=0; }

  #include "ucl_arg_kludge.h"

 private:
  CUfunction _kernel;
  unsigned _dimensions;
  unsigned _num_blocks[2];
  unsigned _num_args;
  std::vector<unsigned> _offsets;
  unsigned _param_size;
  friend class UCL_Texture;
};

} // namespace

#endif

