#ifndef OPENCL_TUTORIAL_SOURCE_DIR
#define OPENCL_TUTORIAL_SOURCE_DIR "."
#endif

#define CL_TARGET_OPENCL_VERSION 120

#include <CL/cl.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

template <typename T>
class UniqueClHandle {
 public:
  using ReleaseFn = cl_int (*)(T);

  UniqueClHandle() = default;
  UniqueClHandle(T value, ReleaseFn release) : value_(value), release_(release) {}

  ~UniqueClHandle() {
    reset();
  }

  UniqueClHandle(const UniqueClHandle&) = delete;
  UniqueClHandle& operator=(const UniqueClHandle&) = delete;

  UniqueClHandle(UniqueClHandle&& other) noexcept : value_(other.value_), release_(other.release_) {
    other.value_   = nullptr;
    other.release_ = nullptr;
  }

  UniqueClHandle& operator=(UniqueClHandle&& other) noexcept {
    if (this != &other) {
      reset();
      value_        = other.value_;
      release_      = other.release_;
      other.value_  = nullptr;
      other.release_ = nullptr;
    }
    return *this;
  }

  T get() const {
    return value_;
  }

  void reset(T value = nullptr, ReleaseFn release = nullptr) {
    if (value_ != nullptr && release_ != nullptr) {
      release_(value_);
    }
    value_   = value;
    release_ = release;
  }

 private:
  T value_            = nullptr;
  ReleaseFn release_  = nullptr;
};

struct AppOptions {
  std::string kernelPath = std::string(OPENCL_TUTORIAL_SOURCE_DIR) + "/vector_add.cl";
  std::size_t elementCount = 1024;
  std::size_t localSize    = 64;
};

struct PlatformSelection {
  cl_platform_id platform = nullptr;
  cl_device_id device     = nullptr;
  cl_device_type type     = 0;
};

struct StageTiming {
  std::string name;
  long long microseconds = 0;
};

struct ErrorStats {
  float maxAbsError   = 0.0f;
  double meanAbsError = 0.0;
};

void checkStatus(cl_int status, const std::string& stage) {
  if (status != CL_SUCCESS) {
    throw std::runtime_error(stage + " failed with status " + std::to_string(status));
  }
}

template <typename Fn>
auto measureStage(std::vector<StageTiming>& timings, const std::string& name, Fn&& fn) {
  const auto start = Clock::now();

  if constexpr (std::is_void_v<std::invoke_result_t<Fn&>>) {
    fn();
    const auto stop = Clock::now();
    timings.push_back(
        {name, std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count()});
  } else {
    auto result = fn();
    const auto stop = Clock::now();
    timings.push_back(
        {name, std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count()});
    return result;
  }
}

std::string readTextFile(const std::string& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("failed to open file: " + path);
  }

  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::string trimTrailingNull(std::string value) {
  if (!value.empty() && value.back() == '\0') {
    value.pop_back();
  }
  return value;
}

std::string getPlatformInfoString(cl_platform_id platform, cl_platform_info param) {
  std::size_t size = 0;
  checkStatus(clGetPlatformInfo(platform, param, 0, nullptr, &size), "clGetPlatformInfo(size)");

  std::string value(size, '\0');
  checkStatus(clGetPlatformInfo(platform, param, value.size(), value.data(), nullptr),
              "clGetPlatformInfo(value)");
  return trimTrailingNull(value);
}

std::string getDeviceInfoString(cl_device_id device, cl_device_info param) {
  std::size_t size = 0;
  checkStatus(clGetDeviceInfo(device, param, 0, nullptr, &size), "clGetDeviceInfo(size)");

  std::string value(size, '\0');
  checkStatus(clGetDeviceInfo(device, param, value.size(), value.data(), nullptr),
              "clGetDeviceInfo(value)");
  return trimTrailingNull(value);
}

const char* deviceTypeName(cl_device_type type) {
  if ((type & CL_DEVICE_TYPE_GPU) != 0) {
    return "GPU";
  }
  if ((type & CL_DEVICE_TYPE_CPU) != 0) {
    return "CPU";
  }
  if ((type & CL_DEVICE_TYPE_ACCELERATOR) != 0) {
    return "ACCELERATOR";
  }
  if ((type & CL_DEVICE_TYPE_DEFAULT) != 0) {
    return "DEFAULT";
  }
  return "UNKNOWN";
}

bool trySelectDevice(cl_platform_id platform, cl_device_type desiredType, PlatformSelection* out) {
  cl_uint count = 0;
  cl_int status = clGetDeviceIDs(platform, desiredType, 0, nullptr, &count);
  if (status == CL_DEVICE_NOT_FOUND || count == 0) {
    return false;
  }
  checkStatus(status, "clGetDeviceIDs(count)");

  std::vector<cl_device_id> devices(count);
  checkStatus(clGetDeviceIDs(platform, desiredType, count, devices.data(), nullptr),
              "clGetDeviceIDs(list)");

  out->platform = platform;
  out->device   = devices.front();
  out->type     = desiredType;
  return true;
}

PlatformSelection selectPlatformAndDevice(const std::vector<cl_platform_id>& platforms) {
  const cl_device_type priorities[] = {
      CL_DEVICE_TYPE_GPU,
      CL_DEVICE_TYPE_DEFAULT,
      CL_DEVICE_TYPE_CPU,
  };

  for (cl_device_type desiredType : priorities) {
    for (cl_platform_id platform : platforms) {
      PlatformSelection selection;
      if (trySelectDevice(platform, desiredType, &selection)) {
        return selection;
      }
    }
  }

  throw std::runtime_error("no usable OpenCL device was found");
}

std::size_t roundUp(std::size_t value, std::size_t block) {
  if (block == 0) {
    throw std::runtime_error("local size must be greater than zero");
  }
  return ((value + block - 1) / block) * block;
}

std::string getBuildLog(cl_program program, cl_device_id device) {
  std::size_t size = 0;
  checkStatus(clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &size),
              "clGetProgramBuildInfo(size)");

  std::string log(size, '\0');
  checkStatus(clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log.size(), log.data(),
                                    nullptr),
              "clGetProgramBuildInfo(value)");
  return trimTrailingNull(log);
}

std::vector<float> makeInputData(std::size_t count, float scale, float bias) {
  std::vector<float> values(count);
  for (std::size_t i = 0; i < count; ++i) {
    const float centered = static_cast<float>((static_cast<int>(i % 23) - 11)) / scale;
    values[i]            = centered + bias;
  }
  return values;
}

ErrorStats computeErrorStats(const std::vector<float>& actual, const std::vector<float>& reference) {
  ErrorStats stats;
  double sumAbsError = 0.0;
  for (std::size_t i = 0; i < actual.size(); ++i) {
    const float absError = std::fabs(actual[i] - reference[i]);
    stats.maxAbsError = std::max(stats.maxAbsError, absError);
    sumAbsError += absError;
  }
  if (!actual.empty()) {
    stats.meanAbsError = sumAbsError / static_cast<double>(actual.size());
  }
  return stats;
}

long long sumStageTimes(const std::vector<StageTiming>& timings) {
  long long total = 0;
  for (const StageTiming& timing : timings) {
    total += timing.microseconds;
  }
  return total;
}

void printTimeBreakdown(const std::string& title, const std::vector<StageTiming>& timings) {
  std::cout << "\n" << title << "\n";
  std::cout << std::left << std::setw(28) << "stage" << std::right << std::setw(12) << "time(us)"
            << "\n";
  std::cout << std::string(40, '-') << "\n";
  for (const StageTiming& timing : timings) {
    std::cout << std::left << std::setw(28) << timing.name << std::right << std::setw(12)
              << timing.microseconds << "\n";
  }
}

void printUsage(const char* programName) {
  std::cout << "Usage: " << programName
            << " [--kernel <path>] [--count <N>] [--local <N>]\n"
            << "  --kernel <path>  Path to vector_add.cl.\n"
            << "  --count <N>      Number of vector elements. Default: 1024.\n"
            << "  --local <N>      Local work-group size. Default: 64.\n";
}

std::size_t parsePositiveSize(const std::string& value, const char* optionName) {
  std::size_t parsed = 0;
  try {
    parsed = static_cast<std::size_t>(std::stoull(value));
  } catch (const std::exception&) {
    throw std::runtime_error(std::string("invalid value for ") + optionName + ": " + value);
  }

  if (parsed == 0) {
    throw std::runtime_error(std::string(optionName) + " must be greater than zero");
  }
  return parsed;
}

AppOptions parseCommandLine(int argc, char** argv) {
  AppOptions options;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      printUsage(argv[0]);
      std::exit(0);
    }
    if (arg == "--kernel") {
      if (i + 1 >= argc) {
        throw std::runtime_error("--kernel requires a value");
      }
      options.kernelPath = argv[++i];
      continue;
    }
    if (arg == "--count") {
      if (i + 1 >= argc) {
        throw std::runtime_error("--count requires a value");
      }
      options.elementCount = parsePositiveSize(argv[++i], "--count");
      continue;
    }
    if (arg == "--local") {
      if (i + 1 >= argc) {
        throw std::runtime_error("--local requires a value");
      }
      options.localSize = parsePositiveSize(argv[++i], "--local");
      continue;
    }

    throw std::runtime_error("unknown option: " + arg);
  }

  if (options.elementCount >
      static_cast<std::size_t>(std::numeric_limits<unsigned int>::max())) {
    throw std::runtime_error("element count exceeds OpenCL kernel argument range");
  }

  return options;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const AppOptions options = parseCommandLine(argc, argv);
    std::vector<StageTiming> openclTimings;
    std::vector<StageTiming> cpuTimings;

    const std::string source = measureStage(
        openclTimings, "read_kernel_source", [&] { return readTextFile(options.kernelPath); });
    if (source.empty()) {
      throw std::runtime_error("kernel source is empty: " + options.kernelPath);
    }

    cl_uint platformCount = 0;
    measureStage(openclTimings, "enumerate_platforms", [&] {
      checkStatus(clGetPlatformIDs(0, nullptr, &platformCount), "clGetPlatformIDs(count)");
    });
    if (platformCount == 0) {
      throw std::runtime_error("no OpenCL platform was found");
    }

    std::vector<cl_platform_id> platforms(platformCount);
    measureStage(openclTimings, "load_platform_list", [&] {
      checkStatus(clGetPlatformIDs(platformCount, platforms.data(), nullptr), "clGetPlatformIDs(list)");
    });

    const PlatformSelection selection =
        measureStage(openclTimings, "select_device", [&] { return selectPlatformAndDevice(platforms); });
    std::cout << "OpenCL vector add example\n";
    std::cout << "kernel_path : " << options.kernelPath << "\n";
    std::cout << "platform    : "
              << getPlatformInfoString(selection.platform, CL_PLATFORM_NAME) << "\n";
    std::cout << "device      : " << getDeviceInfoString(selection.device, CL_DEVICE_NAME) << "\n";
    std::cout << "device_type : " << deviceTypeName(selection.type) << "\n";
    std::cout << "device_ver  : " << getDeviceInfoString(selection.device, CL_DEVICE_VERSION) << "\n";

    cl_int status = CL_SUCCESS;
    UniqueClHandle<cl_context> context = measureStage(openclTimings, "create_context", [&] {
      UniqueClHandle<cl_context> handle(
          clCreateContext(nullptr, 1, &selection.device, nullptr, nullptr, &status),
          &clReleaseContext);
      checkStatus(status, "clCreateContext");
      return handle;
    });

    UniqueClHandle<cl_command_queue> queue = measureStage(openclTimings, "create_queue", [&] {
      UniqueClHandle<cl_command_queue> handle(
          clCreateCommandQueue(context.get(), selection.device, 0, &status),
          &clReleaseCommandQueue);
      checkStatus(status, "clCreateCommandQueue");
      return handle;
    });

    const char* sourcePtr   = source.c_str();
    const std::size_t bytes = source.size();
    UniqueClHandle<cl_program> program = measureStage(openclTimings, "create_program", [&] {
      UniqueClHandle<cl_program> handle(
          clCreateProgramWithSource(context.get(), 1, &sourcePtr, &bytes, &status),
          &clReleaseProgram);
      checkStatus(status, "clCreateProgramWithSource");
      return handle;
    });

    measureStage(openclTimings, "build_program", [&] {
      status = clBuildProgram(program.get(), 1, &selection.device, nullptr, nullptr, nullptr);
      if (status != CL_SUCCESS) {
        std::cerr << "Build Log\n" << getBuildLog(program.get(), selection.device) << "\n";
        checkStatus(status, "clBuildProgram");
      }
    });

    UniqueClHandle<cl_kernel> kernel = measureStage(openclTimings, "create_kernel", [&] {
      UniqueClHandle<cl_kernel> handle(clCreateKernel(program.get(), "vector_add", &status),
                                       &clReleaseKernel);
      checkStatus(status, "clCreateKernel");
      return handle;
    });

    std::size_t maxWorkGroupSize = 0;
    measureStage(openclTimings, "query_work_group", [&] {
      checkStatus(
          clGetKernelWorkGroupInfo(kernel.get(), selection.device, CL_KERNEL_WORK_GROUP_SIZE,
                                   sizeof(maxWorkGroupSize), &maxWorkGroupSize, nullptr),
          "clGetKernelWorkGroupInfo");
    });

    const std::size_t localSize =
        std::min(options.localSize, std::max<std::size_t>(1, maxWorkGroupSize));
    const std::size_t globalSize = roundUp(options.elementCount, localSize);
    std::cout << "shape       : " << options.elementCount << " elements\n";
    std::cout << "global_size : " << globalSize << "\n";
    std::cout << "local_size  : " << localSize << "\n";

    const std::vector<float> inputA = makeInputData(options.elementCount, 5.0f, 0.25f);
    const std::vector<float> inputB = makeInputData(options.elementCount, 7.0f, -0.50f);
    std::vector<float> output(options.elementCount, std::numeric_limits<float>::quiet_NaN());
    std::vector<float> reference(options.elementCount, 0.0f);

    measureStage(cpuTimings, "cpu_vector_add", [&] {
      for (std::size_t i = 0; i < options.elementCount; ++i) {
        reference[i] = inputA[i] + inputB[i];
      }
    });

    const std::size_t bufferSize = options.elementCount * sizeof(float);
    UniqueClHandle<cl_mem> bufferA = measureStage(openclTimings, "create_buffer_a", [&] {
      UniqueClHandle<cl_mem> handle(
          clCreateBuffer(context.get(), CL_MEM_READ_ONLY, bufferSize, nullptr, &status),
          &clReleaseMemObject);
      checkStatus(status, "clCreateBuffer(inputA)");
      return handle;
    });
    UniqueClHandle<cl_mem> bufferB = measureStage(openclTimings, "create_buffer_b", [&] {
      UniqueClHandle<cl_mem> handle(
          clCreateBuffer(context.get(), CL_MEM_READ_ONLY, bufferSize, nullptr, &status),
          &clReleaseMemObject);
      checkStatus(status, "clCreateBuffer(inputB)");
      return handle;
    });
    UniqueClHandle<cl_mem> bufferC = measureStage(openclTimings, "create_buffer_c", [&] {
      UniqueClHandle<cl_mem> handle(
          clCreateBuffer(context.get(), CL_MEM_WRITE_ONLY, bufferSize, nullptr, &status),
          &clReleaseMemObject);
      checkStatus(status, "clCreateBuffer(output)");
      return handle;
    });

    measureStage(openclTimings, "write_input_a", [&] {
      checkStatus(clEnqueueWriteBuffer(queue.get(), bufferA.get(), CL_TRUE, 0, bufferSize,
                                       inputA.data(), 0, nullptr, nullptr),
                  "clEnqueueWriteBuffer(inputA)");
    });
    measureStage(openclTimings, "write_input_b", [&] {
      checkStatus(clEnqueueWriteBuffer(queue.get(), bufferB.get(), CL_TRUE, 0, bufferSize,
                                       inputB.data(), 0, nullptr, nullptr),
                  "clEnqueueWriteBuffer(inputB)");
    });

    const cl_uint elementCount = static_cast<cl_uint>(options.elementCount);
    cl_mem bufferAHandle       = bufferA.get();
    cl_mem bufferBHandle       = bufferB.get();
    cl_mem bufferCHandle       = bufferC.get();
    measureStage(openclTimings, "set_kernel_args", [&] {
      checkStatus(clSetKernelArg(kernel.get(), 0, sizeof(bufferAHandle), &bufferAHandle),
                  "clSetKernelArg(0)");
      checkStatus(clSetKernelArg(kernel.get(), 1, sizeof(bufferBHandle), &bufferBHandle),
                  "clSetKernelArg(1)");
      checkStatus(clSetKernelArg(kernel.get(), 2, sizeof(bufferCHandle), &bufferCHandle),
                  "clSetKernelArg(2)");
      checkStatus(clSetKernelArg(kernel.get(), 3, sizeof(elementCount), &elementCount),
                  "clSetKernelArg(3)");
    });

    measureStage(openclTimings, "enqueue_kernel", [&] {
      checkStatus(clEnqueueNDRangeKernel(queue.get(), kernel.get(), 1, nullptr, &globalSize,
                                         &localSize, 0, nullptr, nullptr),
                  "clEnqueueNDRangeKernel");
      checkStatus(clFinish(queue.get()), "clFinish");
    });

    measureStage(openclTimings, "read_output", [&] {
      checkStatus(clEnqueueReadBuffer(queue.get(), bufferC.get(), CL_TRUE, 0, bufferSize,
                                      output.data(), 0, nullptr, nullptr),
                  "clEnqueueReadBuffer(output)");
    });

    const ErrorStats errorStats = computeErrorStats(output, reference);
    const long long cpuBaselineUs    = sumStageTimes(cpuTimings);
    const long long openclTotalUs    = sumStageTimes(openclTimings);
    long long openclKernelOnlyUs     = 0;
    for (const StageTiming& timing : openclTimings) {
      if (timing.name == "enqueue_kernel") {
        openclKernelOnlyUs = timing.microseconds;
        break;
      }
    }

    const double kernelSpeedup =
        openclKernelOnlyUs > 0 ? static_cast<double>(cpuBaselineUs) / openclKernelOnlyUs : 0.0;
    const double endToEndSpeedup =
        openclTotalUs > 0 ? static_cast<double>(cpuBaselineUs) / openclTotalUs : 0.0;

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "max_abs_err : " << errorStats.maxAbsError << "\n";
    std::cout << "mean_abs_err: " << errorStats.meanAbsError << "\n";
    std::cout << "validation  : "
              << (errorStats.maxAbsError <= 1e-6f ? "PASS" : "CHECK_OUTPUT") << "\n";

    const std::size_t sampleCount = std::min<std::size_t>(8, output.size());
    std::cout << "output[0:" << sampleCount << "] :";
    for (std::size_t i = 0; i < sampleCount; ++i) {
      std::cout << " " << output[i];
    }
    std::cout << "\n";

    printTimeBreakdown("CPU Baseline", cpuTimings);
    printTimeBreakdown("OpenCL Time Breakdown", openclTimings);

    std::cout << "\nAccuracy Comparison\n";
    std::cout << "cpu_reference        : fp32 vector add\n";
    std::cout << "opencl_vs_cpu_maxerr : " << errorStats.maxAbsError << "\n";
    std::cout << "opencl_vs_cpu_meanerr: " << errorStats.meanAbsError << "\n";

    std::cout << "\nPerformance Comparison\n";
    std::cout << "cpu_baseline_us        : " << cpuBaselineUs << "\n";
    std::cout << "opencl_kernel_us       : " << openclKernelOnlyUs << "\n";
    std::cout << "opencl_end_to_end_us   : " << openclTotalUs << "\n";
    std::cout << "kernel_speedup_vs_cpu  : " << kernelSpeedup << "x\n";
    std::cout << "end_to_end_speedup_cpu : " << endToEndSpeedup << "x\n";

    return 0;
  } catch (const std::exception& error) {
    std::cerr << "OpenCL vector add failed: " << error.what() << "\n";
    return 1;
  }
}
