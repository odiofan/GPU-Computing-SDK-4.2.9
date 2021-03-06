/*
 * Copyright 1993-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NOTICE TO USER:
 *
 * This source code is subject to NVIDIA ownership rights under U.S. and
 * international Copyright laws.
 *
 * NVIDIA MAKES NO REPRESENTATION ABOUT THE SUITABILITY OF THIS SOURCE
 * CODE FOR ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR
 * IMPLIED WARRANTY OF ANY KIND.  NVIDIA DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOURCE CODE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.
 * IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
 * OR PERFORMANCE OF THIS SOURCE CODE.
 *
 * U.S. Government End Users.  This source code is a "commercial item" as
 * that term is defined at 48 C.F.R. 2.101 (OCT 1995), consisting  of
 * "commercial computer software" and "commercial computer software
 * documentation" as such terms are used in 48 C.F.R. 12.212 (SEPT 1995)
 * and is provided to the U.S. Government only as a commercial end item.
 * Consistent with 48 C.F.R.12.212 and 48 C.F.R. 227.7202-1 through
 * 227.7202-4 (JUNE 1995), all U.S. Government End Users acquire the
 * source code with only those rights set forth herein.
 */

#include <npp.h>
#include <ImagesCPU.h>
#include <ImagesNPP.h>
#include <ImageIO.h>
#include <Exceptions.h>
#include <string.h>

#include <string>
#include <fstream>
#include <iostream>

#include <shrQATest.h>

#ifdef WIN32
#define STRCASECMP  _stricmp
#define STRNCASECMP _strnicmp
#else
#define STRCASECMP  strcasecmp
#define STRNCASECMP strncasecmp
#endif

bool g_bQATest = false;
int  g_nDevice = -1;

//////////////////////////////////////////////////////////////////////////////
//! Find the path for a file assuming that
//! files are either in data/ or in ../../../src/<executable_name>/data/.
//!
//! @return the path if succeeded, otherwise 0
//! @param filename         name of the file
//! @param executable_path  optional absolute path of the executable
//////////////////////////////////////////////////////////////////////////////
char*
findFilePath(const char* filename, const char* executable_path) 
{
    // search in data/
    if (filename == 0)
        return 0;
    size_t filename_len = strlen(filename);
    const char data_folder[] = "../../data/";
    size_t data_folder_len = strlen(data_folder);
    char* file_path = 
      (char*) malloc( sizeof(char) * (data_folder_len + filename_len + 1));
    strcpy(file_path, data_folder);
    strcat(file_path, filename);
	size_t file_path_len = strlen(file_path);
	file_path[file_path_len] = '\0';
    std::fstream fh0(file_path, std::fstream::in);
    if (fh0.good())
        return file_path;
    free( file_path);

    // search in ../../../src/<executable_name>/data/
    if (executable_path == 0)
        return 0;
    size_t executable_path_len = strlen(executable_path);
    const char* exe;
    for (exe = executable_path + executable_path_len - 1; 
         exe >= executable_path; --exe)
        if (*exe == '/' || *exe == '\\')
            break;
    if (exe < executable_path)
        exe = executable_path;
    else
        ++exe;
    size_t executable_len = strlen(exe);
    size_t executable_dir_len = executable_path_len - executable_len;
    const char projects_relative_path[] = "../../../src/";
    size_t projects_relative_path_len = strlen(projects_relative_path);
    file_path = 
      (char*) malloc( sizeof(char) * (executable_path_len +
         projects_relative_path_len + 1 + data_folder_len + filename_len + 1));
    strncpy(file_path, executable_path, executable_dir_len);
    file_path[executable_dir_len] = '\0';
    strcat(file_path, projects_relative_path);
    strcat(file_path, exe);
    file_path_len = strlen(file_path);
    if (*(file_path + file_path_len - 1) == 'e' &&
        *(file_path + file_path_len - 2) == 'x' &&
        *(file_path + file_path_len - 3) == 'e' &&
        *(file_path + file_path_len - 4) == '.') {
        *(file_path + file_path_len - 4) = '/';
        *(file_path + file_path_len - 3) = '\0';
    }
    else {
        *(file_path + file_path_len - 0) = '/';
        *(file_path + file_path_len + 1) = '\0';
    }
    strcat(file_path, data_folder);
    strcat(file_path, filename);
	file_path_len = strlen(file_path);
	file_path[file_path_len] = '\0';
	std::fstream fh1(file_path, std::fstream::in);
    if (fh1.good())
        return file_path;
    free( file_path);
    return 0;
}

inline void cudaSafeCallNoSync( cudaError err, const char *file, const int line )
{
    if( cudaSuccess != err) {
        std::cerr << file << "(" << line << ")" << " : cudaSafeCallNoSync() Runtime API error : ";
	std::cerr << cudaGetErrorString(err) << std::endl;
        exit(-1);
    }
}

inline int cudaDeviceInit()
{
    int deviceCount;
    cudaSafeCallNoSync(cudaGetDeviceCount(&deviceCount), __FILE__, __LINE__);
    if (deviceCount == 0) {
        std::cerr << "CUDA error: no devices supporting CUDA." << std::endl;
        exit(-1);
    }
    int dev = g_nDevice;
    if (dev < 0) 
        dev = 0;
    if (dev > deviceCount-1) {
        std::cerr << std::endl << ">> %d CUDA capable GPU device(s) detected. <<" << deviceCount << std::endl;
        std::cerr <<">> cutilDeviceInit (-device=" << dev << ") is not a valid GPU device. <<" << std::endl << std::endl;
        return -dev;
    }  else {
        cudaDeviceProp deviceProp;
        cudaGetDeviceProperties(&deviceProp, dev);
        std::cerr << "cudaSetDevice GPU" << dev << " = " << deviceProp.name << std::endl; 
    }
    cudaSafeCallNoSync(cudaSetDevice(dev), __FILE__, __LINE__);

    return dev;
}

void parseCommandLineArguments(int argc, char *argv[])
{
    if (argc >= 2) {
        for (int i=1; i < argc; i++) {
            if (!STRCASECMP(argv[i], "-qatest")   || !STRCASECMP(argv[i], "--qatest") ||
                !STRCASECMP(argv[i], "-noprompt") || !STRCASECMP(argv[i], "--noprompt")) 
            {
                g_bQATest = true;
            }

            if (!STRNCASECMP(argv[i], "-device", 7)) {
                g_nDevice = atoi(&argv[i][8]);
            } else if (!STRNCASECMP(argv[i], "--device", 8)) {
                g_nDevice = atoi(&argv[i][9]);
            }
            if (g_nDevice != -1) {
                cudaDeviceInit();
            }
        }
    }
}

void printfNPPinfo(int argc, char *argv[])
{
    const char *sComputeCap[] = {
       "No CUDA Capable Device Found",
       "Compute 1.0", "Compute 1.1", "Compute 1.2",  "Compute 1.3",
       "Compute 2.0", "Compute 2.1", "Compute 3.0", NULL
    };

    const NppLibraryVersion * libVer   = nppGetLibVersion();
    NppGpuComputeCapability computeCap = nppGetGpuComputeCapability();

    printf("NPP Library Version %d.%d.%d\n", libVer->major, libVer->minor, libVer->build);
    if (computeCap != 0 && g_nDevice == -1) {
        printf("%s using GPU <%s> with %d SM(s) with", argv[0], nppGetGpuName(), nppGetGpuNumSMs());
	if (computeCap > 0) { 
	    printf(" %s\n", sComputeCap[computeCap]);
	} else {
	    printf(" Unknown Compute Capabilities\n");
        }
    } else {
        printf("%s\n", sComputeCap[computeCap]);
    }
}

int main(int argc, char* argv[])
{
    shrQAStart(argc, argv);

    try
    {
        std::string sFilename;
        char *filePath = findFilePath("Lena.pgm", argv[0]);
        if (filePath) {
            sFilename = filePath;
        } else {
            printf("Error unable to find Lena.pgm\n");
            shrQAFinishExit(argc, (const char **)argv, QA_FAILED);
        }
	// Parse the command line arguments for proper configuration
        parseCommandLineArguments(argc, argv);

        printfNPPinfo(argc, argv);

        if (g_bQATest == false && (g_nDevice == -1) && argc > 1) {
            sFilename = argv[1];
        }

        // if we specify the filename at the command line, then we only test sFilename.
        int file_errors = 0;
        std::ifstream infile(sFilename.data(), std::ifstream::in);
        if (infile.good()) {
            std::cout << "histEqualizationNPP opened: <" << sFilename.data() << "> successfully!" << std::endl;
            file_errors = 0;
			infile.close();
        } else {
            std::cout << "histEqualizationNPP unable to open: <" << sFilename.data() << ">" << std::endl;
            file_errors++;
			infile.close();
        }
        if (file_errors > 0) {
            shrQAFinishExit(argc, (const char **)argv, QA_FAILED);
        }

        std::string dstFileName = sFilename;
        
        std::string::size_type dot = dstFileName.rfind('.');
        if (dot != std::string::npos) dstFileName = dstFileName.substr(0, dot);
        dstFileName += "_histEqualization.pgm";

        if (argc >= 3 && !g_bQATest)
            dstFileName = argv[2];

        npp::ImageCPU_8u_C1 oHostSrc;
        npp::loadImage(sFilename, oHostSrc);
        npp::ImageNPP_8u_C1 oDeviceSrc(oHostSrc);

        //
        // allocate arrays for histogram and levels
        //

        const int binCount = 256;
        const int levelCount = binCount + 1; // levels array has one more element

        Npp32s * histDevice = 0;
        Npp32s * levelsDevice = 0;
            
        NPP_CHECK_CUDA(cudaMalloc((void **)&histDevice,   binCount   * sizeof(Npp32s)));
        NPP_CHECK_CUDA(cudaMalloc((void **)&levelsDevice, levelCount * sizeof(Npp32s)));

        //
        // compute histogram
        //

        NppiSize oSizeROI = {oDeviceSrc.width(), oDeviceSrc.height()}; // full image
                // create device scratch buffer for nppiHistogram
        int nDeviceBufferSize;
        nppiHistogramEvenGetBufferSize_8u_C1R(oSizeROI, levelCount ,&nDeviceBufferSize);
        Npp8u * pDeviceBuffer;
        NPP_CHECK_CUDA(cudaMalloc((void **)&pDeviceBuffer, nDeviceBufferSize));
        
                // compute levels values on host
        Npp32s levelsHost[levelCount];
        NPP_CHECK_NPP(nppiEvenLevelsHost_32s(levelsHost, levelCount, 0, binCount));
                // compute the histogram
        NPP_CHECK_NPP(nppiHistogramEven_8u_C1R(oDeviceSrc.data(), oDeviceSrc.pitch(), oSizeROI, 
                                               histDevice, levelCount, 0, binCount, 
                                               pDeviceBuffer));
                // copy histogram and levels to host memory
        Npp32s histHost[binCount];
        NPP_CHECK_CUDA(cudaMemcpy(histHost, histDevice, binCount * sizeof(Npp32s), cudaMemcpyDeviceToHost));

        Npp32s  lutHost[binCount + 1];

                // fill LUT
        {
            Npp32s * pHostHistogram = histHost;
            Npp32s totalSum = 0;
            for (; pHostHistogram < histHost + binCount; ++pHostHistogram)
                totalSum += *pHostHistogram;

            NPP_ASSERT(totalSum == oSizeROI.width * oSizeROI.height);

            if (totalSum == 0) 
                totalSum = 1;
            float multiplier = 1.0f / float(totalSum) * 0xFF;

            Npp32s runningSum = 0;
            Npp32s * pLookupTable = lutHost;
            for (pHostHistogram = histHost; pHostHistogram < histHost + binCount; ++pHostHistogram)
            {
                *pLookupTable = (Npp32s)(runningSum * multiplier + 0.5f);
                pLookupTable++;
                runningSum += *pHostHistogram;
            }

            lutHost[binCount] = 0xFF; // last element is always 1
        }

        //
        // apply LUT transformation to the image
        //
                // Create a device image for the result.
        npp::ImageNPP_8u_C1 oDeviceDst(oDeviceSrc.size());
        NPP_CHECK_NPP(nppiLUT_Linear_8u_C1R(oDeviceSrc.data(), oDeviceSrc.pitch(), 
                                            oDeviceDst.data(), oDeviceDst.pitch(), 
                                            oSizeROI, 
                                            lutHost, // value and level arrays are in host memory
                                            levelsHost, 
                                            binCount+1));

                // copy the result image back into the storage that contained the 
                // input image
        npp::ImageCPU_8u_C1 oHostDst(oDeviceDst.size());
        oDeviceDst.copyTo(oHostDst.data(), oHostDst.pitch());

                // save the result
        npp::saveImage(dstFileName.c_str(), oHostDst);

        std::cout << "Saved image file " << dstFileName << std::endl;
		shrQAFinishExit(argc, (const char **)argv, QA_PASSED);
    }
    catch (npp::Exception & rException)
    {
        std::cerr << "Program error! The following exception occurred: \n";
        std::cerr << rException << std::endl;
        std::cerr << "Aborting." << std::endl;
		shrQAFinishExit(argc, (const char **)argv, QA_FAILED);
    }
    catch (...)
    {
        std::cerr << "Program error! An unknow type of exception occurred. \n";
        std::cerr << "Aborting." << std::endl;
 		shrQAFinishExit(argc, (const char **)argv, QA_FAILED);
    }
    
    return 0;
}

