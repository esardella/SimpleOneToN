/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   Simple_OneToN.h
 * Author: esardell
 *
 * Created on December 15, 2015, 11:55 AM
 */

#ifndef SIMPLE_ONETON_H
#define SIMPLE_ONETON_H

#include <vector>
#include <list>
#include "sample_defs.h"
#include "strings_defs.h"
#include "thread_defs.h"
#include "sample_params.h"
#include "mfxvideo.h"
#include "mfxvideo++.h"
#include "base_allocator.h"
#include "vaapi_allocator.h"
#include "vaapi_device.h"

#include "hw_device.h"
#include "transcode_utils.h"



#define MSDK_MAX_FILENAME_LEN 1024




struct ThreadContext;

class Decoder {    
public: 
    Decoder( );
    virtual ~Decoder();
    virtual mfxStatus Init(sInputParams* pParams, void* hdl, SafetySurfaceBuffer  *ExtBuffer);
    virtual mfxStatus Run();
    mfxVideoParam *    GetDecodeVideoParams() {return &m_mfxDecParams;}
    mfxExtOpaqueSurfaceAlloc * GetDecOpaqueAlloc() {return &m_DecOpaqueAlloc;} 
    mfxStatus Join(MFXVideoSession* pChildSession);
    mfxVideoParam                  m_mfxDecParams; 
    mfxExtOpaqueSurfaceAlloc       m_DecOpaqueAlloc;
      
      
protected: 
    
    void *                          m_hdl; 
    std::auto_ptr<MFXVideoSession>  m_pmfxSession; 
    std::auto_ptr<MFXVideoDECODE>   m_pmfxDEC; 
    BitstreamProcessor             *m_pBSProcessor; 
    
    SafetySurfaceBuffer             *m_pBuffer; 
    FileBitstreamProcessor          *m_BitstreamReader; 
    
    mfxU32                         m_nProcessedFramesNum;
    mfxU32                         m_AsyncDepth; 
    mfxBitstream                   *m_pmfxBS; 
    
    std::vector<mfxExtBuffer*>     m_DecExtParams; 
   
     
    typedef std::vector<mfxFrameSurface1 *> SurfPointersArray; 
    SurfPointersArray    m_pSurfaceDecPool;
   
     
    mfxStatus DecodeOneFrame(ExtendedSurface *pExtSurface);
    mfxStatus DecodeLastFrame(ExtendedSurface *pExtSurface);
    void NoMoreFramesSignal(ExtendedSurface &DecExtSurface);
    mfxFrameSurface1*  GetFreeSurface();
     
 private: 
    
};




class Encoder { 
    
public:
    Encoder();
    virtual ~Encoder();
    
    virtual mfxStatus Init(sInputParams* pParams,  void* hdl,   SafetySurfaceBuffer  *ExtBuffer);
    virtual mfxStatus Run();
    void SetParent(Decoder * obj) {m_Decoder = obj;} 
    
protected: 
    
       bool                            m_bIsVPP;   
       void *                          m_hdl; 
       std::auto_ptr<MFXVideoSession>  m_pmfxEncSession; 
       std::auto_ptr<MFXVideoENCODE>   m_pmfxENC; 
       std::auto_ptr<MFXVideoVPP>      m_pmfxVPP;
       std::auto_ptr<ExtendedBSStore>  m_pBSStore; 
       
       BitstreamProcessor              *m_pBSProcessor; 
       SafetySurfaceBuffer             *m_pBuffer; 
       mfxVideoParam                    m_mfxEncParams; 
       mfxVideoParam                    m_mfxVPPParams; 
       std::vector<mfxExtBuffer*>       m_EncExtParams; 
       std::vector<mfxExtBuffer*>       m_VPPExtParams; 
       mfxExtOpaqueSurfaceAlloc         m_EncOpaqueAlloc;
       mfxExtOpaqueSurfaceAlloc         m_VPPOpaqueAlloc;
         
       FileBitstreamProcessor          *m_BitstreamWriter;
       
         
       mfxU32                           m_AsyncDepth;
       
       Decoder*                         m_Decoder;  // ptr to parent
       
       typedef std::vector<mfxFrameSurface1 *> SurfPointersArray; 
       SurfPointersArray                m_pSurfaceEncPool;
       
       typedef std::list<ExtendedBS *> BSList; 
       BSList  m_BSPool;
         
       mfxStatus PutBS();
       mfxStatus EncodeOneFrame(ExtendedSurface *pExtSurface, mfxBitstream *pBS);
       mfxStatus VPPOneFrame(ExtendedSurface *pSurfaceIn, ExtendedSurface *pExtSurface);
       mfxFrameSurface1* GetFreeSurface();
       mfxStatus AllocateSufficientBuffer(mfxBitstream *pBS);
        
private: 
    
};



class Transcoder 
{
public: 
    Transcoder();
    virtual ~Transcoder(); 
    
    virtual mfxStatus Init(int argc, char* argv[]); 
    virtual mfxStatus Run(); 
    std::vector<SafetySurfaceBuffer *>                  m_pBufferArray; 
    
protected: 
    
    std::vector<ThreadContext *>                        m_pSessionArray;   
    std::vector<MSDKThread *>                           m_HDLArray; 
    std::vector<sInputParams>                           m_InputParamsArray; 
   
    std::auto_ptr<mfxAllocatorParams>                   m_pAllocParams;
    std::vector<FileBitstreamProcessor*>                m_pExtBSProcArray; 
    std::auto_ptr<CHWDevice>                            m_hwdev;
    CmdProcessor                                        m_parser; 
    Decoder *                                           m_Decoder; 
    Encoder *                                           m_Encoder;     
    mfxF64                                              m_StartTime;
    
    mfxStatus  CreateSafteyBuffers();
private:
    
    
};
struct ThreadContext 
 {     
     std::auto_ptr<Decoder> pDecoder; 
     std::auto_ptr<Encoder> pEncoder;
     mfxStatus  transcodingSts; 
 };

#endif /* SIMPLE_ONETON_H */