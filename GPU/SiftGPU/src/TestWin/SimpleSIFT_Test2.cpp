////////////////////////////////////////////////////////////////////////////
//    File:        SimpleSIFT.cpp
//    Author:      Changchang Wu
//    Description : A simple example shows how to use SiftGPU and SiftMatchGPU
//
//
//
//    Copyright (c) 2007 University of North Carolina at Chapel Hill
//    All Rights Reserved
//
//    Permission to use, copy, modify and distribute this software and its
//    documentation for educational, research and non-profit purposes, without
//    fee, and without a written agreement is hereby granted, provided that the
//    above copyright notice and the following paragraph appear in all copies.
//
//    The University of North Carolina at Chapel Hill make no representations
//    about the suitability of this software for any purpose. It is provided
//    'as is' without express or implied warranty.
//
//    Please send BUG REPORTS to ccwu@cs.unc.edu
//
////////////////////////////////////////////////////////////////////////////


#include <stdlib.h>
#include <vector>
#include <iostream>
#include <sys/time.h>
#include <fstream>
#include <string>
#include <math.h>
#include <stdio.h>
#include <dirent.h>
using namespace std;
//using std::vector;
//using std::iostream;


////////////////////////////////////////////////////////////////////////////
#if !defined(SIFTGPU_STATIC) && !defined(SIFTGPU_DLL_RUNTIME) 
// SIFTGPU_STATIC comes from compiler
#define SIFTGPU_DLL_RUNTIME
// Load at runtime if the above macro defined
// comment the macro above to use static linking
#endif

////////////////////////////////////////////////////////////////////////////
// define REMOTE_SIFTGPU to run computation in multi-process (Or remote) mode
// in order to run on a remote machine, you need to start the server manually
// This mode allows you use Multi-GPUs by creating multiple servers
// #define REMOTE_SIFTGPU
// #define REMOTE_SERVER        NULL
// #define REMOTE_SERVER_PORT   7777


///////////////////////////////////////////////////////////////////////////
//#define DEBUG_SIFTGPU  //define this to use the debug version in windows

#ifdef _WIN32
    #ifdef SIFTGPU_DLL_RUNTIME
        #define WIN32_LEAN_AND_MEAN
        #include <windows.h>
        #define FREE_MYLIB FreeLibrary
        #define GET_MYPROC GetProcAddress
    #else
        //define this to get dll import definition for win32
        #define SIFTGPU_DLL
        #ifdef _DEBUG 
            #pragma comment(lib, "../../lib/siftgpu_d.lib")
        #else
            #pragma comment(lib, "../../lib/siftgpu.lib")
        #endif
    #endif
#else
    #ifdef SIFTGPU_DLL_RUNTIME
        #include <dlfcn.h>
        #define FREE_MYLIB dlclose
        #define GET_MYPROC dlsym
    #endif
#endif

#include "../SiftGPU/SiftGPU.h"

int getMilliSecond()
{
    static int started = 0;
    static struct timeval tstart;
    if(started == 0)
    {
        gettimeofday(&tstart, NULL);
        started = 1;
        return 0;
    }else
    {
        struct timeval now;
        gettimeofday(&now, NULL) ;
        return (now.tv_usec - tstart.tv_usec + (now.tv_sec - tstart.tv_sec) * 1000000)/1000;
    }
}
void getHumanReadableTime(float start){
    float all = ((float)getMilliSecond() - start)/1000 ;
    float allMs =  all * 1000.0f;
    int allS = (int)(allMs / 1000);
    int milliseconds = (((int)allMs) % 1000);
    int minutes = allS / 60;
    int seconds = allS % 60;
    std::cout
        <<"\n\n**************Welcome**************************\n"
        <<"[Tiempo total ]:\t\t" << allMs<< "ms\n"
        <<"[millisegundos]:\t\t" << milliseconds<< "ms\n"
        <<"[segundos]:\t\t" << seconds   << "s\n"
        <<"[minutos]:\t\t" << minutes  << "m\n"
        << std::endl;
}
int listFilesDirectory(vector<string>& images, const string& directory)
{
    DIR *dir;
    class dirent *ent;
    dir = opendir(directory.c_str());
    if(!dir)
    {
        std::cout << "ERROR:could not find directory in listFilesDirectory:\n"<<directory.c_str();
        return -1;
    }
    while((ent = readdir(dir))!= NULL )
    {
        if( ent->d_type == DT_REG)
        {
            string image(ent->d_name);
            string ext = image.substr(image.rfind("."));
            if(!ext.compare(".pgm") || !ext.compare(".jpg"))
            {
                images.push_back(image);
            }
            else
            {
                //cout << "WARNING:incorrect file no pgm nor ppm in listFilesDirectory:\n"<<image.c_str();
                cout <<endl;
            }
        }
    }
    closedir(dir);
    return 0;
}
int main(int argc, char * argv[])
{
#ifdef SIFTGPU_DLL_RUNTIME
    #ifdef _WIN32
        #ifdef _DEBUG
            HMODULE  hsiftgpu = LoadLibrary("siftgpu_d.dll");
        #else
            HMODULE  hsiftgpu = LoadLibrary("siftgpu.dll");
        #endif
    #else
        void * hsiftgpu = dlopen("../bin/libsiftgpu.so", RTLD_LAZY);
    #endif

    if(hsiftgpu == NULL) return 0;

    #ifdef REMOTE_SIFTGPU
        ComboSiftGPU* (*pCreateRemoteSiftGPU) (int, char*) = NULL;
        pCreateRemoteSiftGPU = (ComboSiftGPU* (*) (int, char*)) GET_MYPROC(hsiftgpu, "CreateRemoteSiftGPU");
        ComboSiftGPU * combo = pCreateRemoteSiftGPU(REMOTE_SERVER_PORT, REMOTE_SERVER);
        SiftGPU* sift = combo;
        SiftMatchGPU* matcher = combo;
    #else
        SiftGPU* (*pCreateNewSiftGPU)(int) = NULL;
        SiftMatchGPU* (*pCreateNewSiftMatchGPU)(int) = NULL;
        pCreateNewSiftGPU = (SiftGPU* (*) (int)) GET_MYPROC(hsiftgpu, "CreateNewSiftGPU");
        pCreateNewSiftMatchGPU = (SiftMatchGPU* (*)(int)) GET_MYPROC(hsiftgpu, "CreateNewSiftMatchGPU");
        SiftGPU* sift = pCreateNewSiftGPU(1);
        SiftMatchGPU* matcher = pCreateNewSiftMatchGPU(4096);
    #endif

#elif defined(REMOTE_SIFTGPU)
    ComboSiftGPU * combo = CreateRemoteSiftGPU(REMOTE_SERVER_PORT, REMOTE_SERVER);
    SiftGPU* sift = combo;
    SiftMatchGPU* matcher = combo;
#else
    //this will use overloaded new operators
    SiftGPU  *sift = new SiftGPU;
    SiftMatchGPU *matcher = new SiftMatchGPU(4096);
#endif
    vector<float > descriptors1(1), descriptors2(1);
    vector<SiftGPU::SiftKeypoint> keys1(1), keys2(1);
    int num1 = 0, num2 = 0;
    float start,all;
    //process parameters
    //The following parameters are default in V340
    //-m,       up to 2 orientations for each feature (change to single orientation by using -m 1)
    //-s        enable subpixel subscale (disable by using -s 0)


    //char * argv[] = {"-fo", "-1",  "-v", "1"};//
    char * argw[] = {"-fo", "-1",  "-v", "1","-t","0.03","-cuda","0","-maxd","768"};
    //-fo -1    staring from -1 octave
    //-v 1      only print out # feature and overall time
    //-loweo    add a (.5, .5) offset
    //-tc <num> set a soft limit to number of detected features

    //NEW:  parameters for  GPU-selection
    //1. CUDA.                   Use parameter "-cuda", "[device_id]"
    //2. OpenGL.				 Use "-Display", "display_name" to select monitor/GPU (XLIB/GLUT)
    //   		                 on windows the display name would be something like \\.\DISPLAY4

    //////////////////////////////////////////////////////////////////////////////////////
    //You use CUDA for nVidia graphic cards by specifying
    //-cuda   : cuda implementation (fastest for smaller images)
    //          CUDA-implementation allows you to create multiple instances for multiple threads
    //          Checkout src\TestWin\MultiThreadSIFT
    /////////////////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////Two Important Parameters///////////////////////////
    // First, texture reallocation happens when image size increases, and too many
    // reallocation may lead to allocatoin failure.  You should be careful when using
    // siftgpu on a set of images with VARYING imag sizes. It is recommended that you
    // preset the allocation size to the largest width and largest height by using function
    // AllocationPyramid or prameter '-p' (e.g. "-p", "1024x768").

    // Second, there is a parameter you may not be aware of: the allowed maximum working
    // dimension. All the SIFT octaves that needs a larger texture size will be skipped.
    // The default prameter is 2560 for the unpacked implementation and 3200 for the packed.
    // Those two default parameter is tuned to for 768MB of graphic memory. You should adjust
    // it for your own GPU memory. You can also use this to keep/skip the small featuers.
    // To change this, call function SetMaxDimension or use parameter "-maxd".
    //
    // NEW: by default SiftGPU will try to fit the cap of GPU memory, and reduce the working
    // dimension so as to not allocate too much. This feature can be disabled by -nomc
    //////////////////////////////////////////////////////////////////////////////////////
    //sift->AllocatePyramid(640, 480);
    string directory="../../../Rendimiento/Imagenes/ImagenesJPG";    
    vector<string> images;
    vector<string> detections;
    const string slash = "/";
    if(listFilesDirectory(images, directory))
    {
        cout << "ERROR:finish Main"<<"\n" <<endl;
        return -1;
    }
    for(unsigned int i=0; i < images.size(); i++)
    {
        int lastindex = images[i].find_last_of(".");
        string imagesPath = directory + slash + images[i];
        string outputPath = images[i].substr(images[i].rfind("/")+1,lastindex).append(".sift");
        string detectsPath = directory + slash + outputPath;
        detections.push_back(detectsPath);
        images[i] = imagesPath;
    }
    int argd = sizeof(argw)/sizeof(char*);
    sift->ParseParam(argd, argw);
    ///////////////////////////////////////////////////////////////////////
    //Only the following parameters can be changed after initialization (by calling ParseParam).
    //-dw, -ofix, -ofix-not, -fo, -unn, -maxd, -b
    //to change other parameters at runtime, you need to first unload the dynamically loaded libaray
    //reload the libarary, then create a new siftgpu instance


    //Create a context for computation, and SiftGPU will be initialized automatically
    //The same context can be used by SiftMatchGPU
    if(sift->CreateContextGL() != SiftGPU::SIFTGPU_FULL_SUPPORTED) return 0;

    //Test 2
    start=(float)getMilliSecond();
    for(unsigned int i=0; i < images.size(); i++)
    {
        const char * imgPath = images[i].c_str();
       if(sift->RunSIFT(imgPath))
        {
            num1 = sift->GetFeatureNum();
            keys1.resize(num1);
            descriptors1.resize(128*num1);
            sift->GetFeatureVector(&keys1[0], &descriptors1[0]);
            string siftStr = detections[i];
            const char * siftPath =siftStr.c_str();
            sift->SaveSIFT(siftPath);
        }
    }
    getHumanReadableTime(start);
}










