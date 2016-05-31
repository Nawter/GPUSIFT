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



////////////////////////////////////////////////////////////////////////////
#if !defined(SIFTGPU_STATIC) && !defined(SIFTGPU_DLL_RUNTIME)
// SIFTGPU_STATIC comes from compiler
#define SIFTGPU_DLL_RUNTIME
// Load at runtime if the above macro defined
// comment the macro above to use static linking
#endif



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
void getHumanReadableTime(float start)
{
    float all = ((float)getMilliSecond() - start)/1000 ;
    float allMs =  all * 1000.0f;
    int allS = (int)(allMs / 1000);
    int milliseconds = (((int)allMs) % 1000);
    int minutes = allS / 60;
    int seconds = allS % 60;
    std::cout
        <<"**************Welcome**************************\n"
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
            if(!ext.compare(".pgm") || !ext.compare(".ppm"))
            {
                images.push_back(image);
            }
            else
            {
                //cout << "WARNING:incorrect file no pgm nor ppm in listFilesDirectory:\n"<<image.c_str();
                //cout <<endl;
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
    char * argw[] = {"-fo", "-1",  "-v", "0","-t","0.03","-cuda","0","-maxd","3200"};

    string directory=argv[1]; //"../../../Rendimiento/Imagenes/Imagenes_2";
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
    if(sift->CreateContextGL() != SiftGPU::SIFTGPU_FULL_SUPPORTED) return 0;
    //start=(float)getMilliSecond();
    for(unsigned int i=0; i < images.size(); i++)
    {
        const char * imgPath = images[i].c_str();
        sift->RunSIFT(imgPath);
        sift->RunSIFT(num2, &keys2[0], 0);
        string siftStr = detections[i];
        const char * siftPath =siftStr.c_str();
        sift->SaveSIFT(siftPath);
    }
    //getHumanReadableTime(start);
}

