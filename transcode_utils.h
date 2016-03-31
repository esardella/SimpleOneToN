//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2011 - 2014 Intel Corporation. All Rights Reserved.
//
//
//*/

#ifndef __TRANSCODE_UTILS_H__
#define __TRANSCODE_UTILS_H__

#if defined(_WIN32) || defined(_WIN64)
#include <process.h>
#pragma warning(disable : 4201)
#include <d3d9.h>
#include <dxva2api.h>
#endif

#include <vector>
#include <map>
//#include "transcoder.h" 
#include "sample_defs.h"
 
struct D3DAllocatorParams;

#pragma warning(disable: 4127) // constant expression


    struct sInputParams;

    msdk_tick GetTick();
    mfxF64 GetTime(msdk_tick start);

    void PrintHelp(const msdk_char *strAppName, const msdk_char *strErrorMessage, ...);
    void PrintInfo(mfxU32 session_number, sInputParams* pParams, mfxVersion *pVer);

    bool PrintDllInfo(msdk_char *buf, mfxU32 buf_size, sInputParams* pParams);


    template <class T, bool isSingle>
    class s_ptr
    {
    public:
        s_ptr():m_ptr(0)
        {
        };
        ~s_ptr()
        {
            reset(0);
        }
        T* get()
        {
            return m_ptr;
        }
        T* pop()
        {
            T* ptr = m_ptr;
            m_ptr = 0;
            return ptr;
        }
        void reset(T* ptr)
        {
            if (m_ptr)
            {
                if (isSingle)
                    delete m_ptr;
                else
                    delete[] m_ptr;
            }
            m_ptr = ptr;
        }
    protected:
        T* m_ptr;
    };

    class CmdProcessor
    {
    public:
        CmdProcessor();
        virtual ~CmdProcessor();
        mfxStatus ParseCmdLine(int argc, msdk_char *argv[]);
        bool GetNextSessionParams(sInputParams &InputParams);
        FILE*     GetPerformanceFile() {return m_PerfFILE;};
        void      PrintParFileName();
        

    protected:
        mfxStatus ParseParFile(FILE* file);
        mfxStatus TokenizeLine(msdk_char *pLine, mfxU32 length);
        mfxStatus ParseParamsForOneSession(mfxU32 argc, msdk_char *argv[]);
        mfxStatus ParseOption__set(msdk_char* strCodecType, msdk_char* strPluginPath);
        sPluginParams ParsePluginParameter(msdk_char* strPluginPath);
        mfxStatus VerifyAndCorrectInputParams(sInputParams &InputParams);
        mfxU32                                       m_SessionParamId;
        std::vector<sInputParams> m_SessionArray;
        std::map<mfxU32, sPluginParams>              m_decoderPlugins;
        std::map<mfxU32, sPluginParams>              m_encoderPlugins;
        FILE                                         *m_PerfFILE;
        msdk_char                                    *m_parName;
        mfxU32                                        m_nTimeout;
    private:
        DISALLOW_COPY_AND_ASSIGN(CmdProcessor);

    };
    
    
  
enum PipelineMode
{
    Native = 0, 
    Sink,
    Source     
};

struct __sInputParams
    {
        // session parameters
        bool         bIsJoin;
        mfxPriority  priority;
        // common parameters
        mfxIMPL libType;  // Type of used mediaSDK library
        bool   bIsPerf;   // special performance mode. Use pre-allocated bitstreams, output

        mfxU32 EncodeId; // type of output coded video
        mfxU32 DecodeId; // type of input coded video

        msdk_char  strSrcFile[MSDK_MAX_FILENAME_LEN]; // source bitstream file
        msdk_char  strDstFile[MSDK_MAX_FILENAME_LEN]; // destination bitstream file

        // specific encode parameters
        mfxU16 nTargetUsage;
        mfxF64 dFrameRate;
        mfxU32 nBitRate;
        mfxU16 nQuality; // quality parameter for JPEG encoder
        mfxU16 nDstWidth;  // destination picture width, specified if resizing required
        mfxU16 nDstHeight; // destination picture height, specified if resizing required

        bool bEnableDeinterlacing;

        mfxU16 nAsyncDepth; // asyncronous queue

        PipelineMode eMode;

        mfxU32 MaxFrameNumber; // maximum frames fro transcoding

        mfxU16 nSlices; // number of slices for encoder initialization
        mfxU16 nMaxSliceSize; //maximum size of slice

        // MVC Specific Options
        bool   bIsMVC; // true if Multi-View-Codec is in use
        mfxU32 numViews; // number of views for Multi-View-Codec

        mfxU16 nRotationAngle; // if specified, enables rotation plugin in mfx pipeline
        msdk_char strVPPPluginDLLPath[MSDK_MAX_FILENAME_LEN]; // plugin dll path and name

        sPluginParams decoderPluginParams;
        sPluginParams encoderPluginParams;

        mfxU32 nTimeout; // how long transcoding works in seconds
        mfxU32 nFPS; // limit transcoding to the number of frames per second

        bool bLABRC; // use look ahead bitrate control algorithm
        mfxU16 nLADepth; // depth of the look ahead bitrate control  algorithm
        bool bEnableExtLA;
        bool bEnableBPyramid;

        bool bOpenCL;
    };

    struct sInputParams: public __sInputParams
    {
        sInputParams();
        void Reset();
    };

    
 struct PreEncAuxBuffer
 {
     mfxEncodeCtrl     encCtrl; 
     mfxU16            Locked; 
     mfxENCInput       encInput; 
     mfxENCOutput      encOutput; 
 };   
    
 struct ExtendedSurface 
 {
     mfxFrameSurface1   *pSurface; 
     PreEncAuxBuffer    *pCtrl; 
     mfxSyncPoint       Syncp;
 };
 
  struct ExtendedBS
    {
        ExtendedBS(): IsFree(true), Syncp(NULL), pCtrl(NULL)
        {
            MSDK_ZERO_MEMORY(Bitstream);
        };
        bool IsFree;
        mfxBitstream Bitstream;
        mfxSyncPoint Syncp;
        PreEncAuxBuffer* pCtrl;
    };
 
 
 class ExtendedBSStore
    {
    public:
        explicit ExtendedBSStore(mfxU32 size)
        {
            m_pExtBS.resize(size);
        };
        virtual ~ExtendedBSStore()
        {
            for (mfxU32 i=0; i < m_pExtBS.size(); i++)
                MSDK_SAFE_DELETE_ARRAY(m_pExtBS[i].Bitstream.Data);
            m_pExtBS.clear();

        };
        ExtendedBS* GetNext()
        {
            for (mfxU32 i=0; i < m_pExtBS.size(); i++)
            {
                if (m_pExtBS[i].IsFree)
                {
                    m_pExtBS[i].IsFree = false;
                    return &m_pExtBS[i];
                }
            }
            return NULL;
        };
        void Release(ExtendedBS* pBS)
        {
            for (mfxU32 i=0; i < m_pExtBS.size(); i++)
            {
                if (&m_pExtBS[i] == pBS)
                {
                    m_pExtBS[i].IsFree = true;
                    return;
                }
            }
            return;
        };
    protected:
        std::vector<ExtendedBS> m_pExtBS;

    private:
        DISALLOW_COPY_AND_ASSIGN(ExtendedBSStore);
    };
 

 class BitstreamProcessor
 {
 public: 
     BitstreamProcessor() {}; 
     virtual ~BitstreamProcessor() {}; 
     virtual mfxStatus PrepareBitstream() = 0; 
     virtual mfxStatus GetInputBitstream(mfxBitstream **pBitstream) = 0; 
     virtual mfxStatus ProcessOutputBitstream(mfxBitstream *pBitstream) = 0; 
 };
 
 class FileBitstreamProcessor : public BitstreamProcessor
 {
 public: 
     FileBitstreamProcessor();
     virtual ~FileBitstreamProcessor();
     virtual mfxStatus Init(msdk_char *pStrSrcFile, msdk_char *pStrDstFile);
     virtual mfxStatus PrepareBitstream() {return MFX_ERR_NONE; }
     virtual mfxStatus GetInputBitstream(mfxBitstream **pBitstream);
     virtual mfxStatus ProcessOutputBitstream(mfxBitstream *pBitstream);

 protected: 
     std::auto_ptr<CSmplBitstreamReader> m_pFileReader; 
     std::auto_ptr<CSmplBitstreamWriter> m_pFileWriter; 
     mfxBitstream   m_Bitstream;
     
 private: 
     DISALLOW_COPY_AND_ASSIGN(FileBitstreamProcessor);
 };  
 
 

 class SafetySurfaceBuffer
 {
 public: 
     struct SurfaceDescriptor 
     {
         ExtendedSurface    ExtSurface; 
         mfxU32             Locked; 
     };
     SafetySurfaceBuffer(SafetySurfaceBuffer *pNext);
     virtual ~SafetySurfaceBuffer(); 
     
     void       AddSurface(ExtendedSurface Surf);
     mfxStatus  GetSurface(ExtendedSurface &Surf);
     mfxStatus  ReleaseSurface(mfxFrameSurface1 *pSurf);
     
     SafetySurfaceBuffer        *m_pNext; 
     
 protected: 
       
     MSDKMutex                                              m_mutex; 
     std::list<SafetySurfaceBuffer::SurfaceDescriptor>        m_SList; 
     
 };
 

 

 
    
    
    
    
    

#endif //__TRANSCODE_UTILS_H__

