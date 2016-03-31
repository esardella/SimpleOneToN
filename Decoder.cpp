#include "transcoder.h"
//#include "transcode_utils.h"



Decoder::Decoder():     m_nProcessedFramesNum(0),
                        m_pmfxBS(NULL),   
                        m_pmfxSession(NULL),
                        m_pmfxDEC(NULL)
{
 
 
  MSDK_ZERO_MEMORY(m_mfxDecParams);
  MSDK_ZERO_MEMORY(m_DecOpaqueAlloc);
  
 
}

Decoder::~Decoder()
{
  
    if(m_pmfxDEC.get())
        m_pmfxDEC.reset(); 
    
     
    
    
}

mfxStatus Decoder::Init(sInputParams* pParams,void* hdl, SafetySurfaceBuffer  *ExtBuffer )
{
    mfxStatus sts; 
    
    mfxVersion ver = {{0,1}}; 
  
    MSDK_CHECK_POINTER(ExtBuffer, MFX_ERR_NULL_PTR);
    m_pBuffer = ExtBuffer; 
    
    
    //Create Bitstream Reader
    m_BitstreamReader = new FileBitstreamProcessor();
    MSDK_CHECK_POINTER(m_BitstreamReader, MFX_ERR_NULL_PTR); 
    

    //Initialize the Bitstream Reader  
    sts = m_BitstreamReader->Init(pParams->strSrcFile, NULL);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE,sts);
    
    //Create Session 
    m_pmfxSession.reset(new MFXVideoSession);
    sts = m_pmfxSession->Init(MFX_IMPL_HARDWARE,&ver);
    MSDK_CHECK_RESULT(sts,MFX_ERR_NONE,sts);
    
    // Add Handle to Session 
    mfxHandleType handleType = (mfxHandleType)0; 
    handleType = MFX_HANDLE_VA_DISPLAY; 
    sts = m_pmfxSession->SetHandle(handleType, hdl);
    MSDK_CHECK_RESULT(sts,MFX_ERR_NONE,sts);
    m_hdl = hdl; 
    
    //Create Decoder
    m_pmfxDEC.reset(new MFXVideoDECODE(*m_pmfxSession.get()));
        

    //Setup Decoder     
    m_mfxDecParams.mfx.CodecId = pParams->DecodeId; 
    m_mfxDecParams.AsyncDepth = 4; //m_AsyncDepth; 
    m_mfxDecParams.IOPattern = MFX_IOPATTERN_OUT_OPAQUE_MEMORY;     
    m_mfxDecParams.mfx.ExtendedPicStruct = 1;
        
    //Read the header
    sts = m_BitstreamReader->GetInputBitstream(&m_pmfxBS); 
    MSDK_CHECK_RESULT(sts,MFX_ERR_NONE,sts);                             
        
    //Decode the header
    sts = m_pmfxDEC->DecodeHeader(m_pmfxBS, &m_mfxDecParams);
    MSDK_IGNORE_MFX_STS(sts,MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts,MFX_ERR_NONE,sts);
    
    //Query # surfaces needed
    mfxFrameAllocRequest DecRequest; 
    sts = m_pmfxDEC->QueryIOSurf(&m_mfxDecParams, &DecRequest);
    MSDK_CHECK_RESULT(sts,MFX_ERR_NONE,sts);
        
    
    //Alloc Surface Headers  - We are using Opaque so leave memid alone
    mfxU16 nSurfDec = DecRequest.NumFrameSuggested;
    mfxFrameSurface1** pSurfaces = new mfxFrameSurface1 *[nSurfDec];
    MSDK_CHECK_POINTER(pSurfaces, MFX_ERR_MEMORY_ALLOC);
        
    for(int i = 0; i < nSurfDec; i++)
        {
            pSurfaces[i] = new mfxFrameSurface1;
            MSDK_CHECK_POINTER(pSurfaces[i], MFX_ERR_MEMORY_ALLOC);
            memset(pSurfaces[i], 0, sizeof(mfxFrameSurface1));
            memcpy( &(pSurfaces[i]->Info), &(DecRequest.Info), sizeof(mfxFrameInfo));
            m_pSurfaceDecPool.push_back(pSurfaces[i]);
        }
    
    //Setup Opaque Allocation info 
    m_DecExtParams.push_back((mfxExtBuffer *)&m_DecOpaqueAlloc);     
    m_mfxDecParams.ExtParam = &m_DecExtParams[0];
    m_mfxDecParams.NumExtParam = (mfxU16)m_DecExtParams.size(); 
        
    m_DecOpaqueAlloc.Header.BufferId =  MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION; 
    m_DecOpaqueAlloc.Header.BufferSz =  sizeof(mfxExtOpaqueSurfaceAlloc);
    
     
    m_DecOpaqueAlloc.Out.Surfaces = &m_pSurfaceDecPool[0];
    m_DecOpaqueAlloc.Out.NumSurface = (mfxU16)m_pSurfaceDecPool.size(); 
    m_DecOpaqueAlloc.Out.Type =  DecRequest.Type; 
        
    
    //Init the decoder
    sts = m_pmfxDEC->Init(&m_mfxDecParams); 
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE,sts);
    
    return sts; 
}


mfxStatus Decoder::Run()
{
 mfxStatus sts = MFX_ERR_NONE; 
    ExtendedSurface  DecExtSurface = {0}; 
    SafetySurfaceBuffer *pNextBuffer = m_pBuffer; 
    bool bEndOfFile = false; 
    
 
    while(MFX_ERR_NONE == sts)
    {
        pNextBuffer = m_pBuffer; 
        msdk_tick nBeginTime = msdk_time_get_tick();
        if(!bEndOfFile)
        {
            sts = DecodeOneFrame(&DecExtSurface);
            if(MFX_ERR_MORE_DATA == sts)
            {
                sts = DecodeLastFrame(&DecExtSurface);
                bEndOfFile = true; 
            }
        }
        else
        {
            sts = DecodeLastFrame(&DecExtSurface);
        }
        if(sts == MFX_ERR_NONE)
        {
            m_nProcessedFramesNum++;
        }
        if(sts == MFX_ERR_MORE_DATA /*&& m_pmfxVPP.get()*/)
        {
            DecExtSurface.pSurface = NULL; 
            break;

            
        }
        MSDK_BREAK_ON_ERROR(sts);
        
        pNextBuffer->AddSurface(DecExtSurface);
        while(pNextBuffer->m_pNext)
        {
            pNextBuffer = pNextBuffer->m_pNext;
            pNextBuffer->AddSurface(DecExtSurface);
        }
        
        if (0 == (m_nProcessedFramesNum - 1) % 100)
        {
            msdk_printf("%d\n", m_nProcessedFramesNum);
        }
    }
    
    //Done decoding 
    if(sts == MFX_ERR_MORE_DATA)
    {
           pNextBuffer->AddSurface(DecExtSurface);
           while(pNextBuffer->m_pNext)
                {
                  pNextBuffer = pNextBuffer->m_pNext; 
                  pNextBuffer->AddSurface(DecExtSurface);
                }   
    }
   // MSDK_IGNORE_MFX_STS(sts,MFX_ERR_MORE_DATA);     
    msdk_printf("Frames Processed: %d\n", m_nProcessedFramesNum); 
    
    return sts; 
   
}

mfxStatus Decoder::DecodeOneFrame(ExtendedSurface *pExtSurface)
{
    mfxStatus sts = MFX_ERR_MORE_SURFACE; 
    mfxFrameSurface1 *pmfxSurface = NULL;
    pExtSurface->pSurface = NULL;
    mfxU32 i = 0; 
    
    while(MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts || MFX_ERR_NONE < sts) 
    {
        if(MFX_WRN_DEVICE_BUSY == sts)
            MSDK_SLEEP(1);
        else if (MFX_ERR_MORE_DATA == sts)
        {
            sts = m_BitstreamReader->GetInputBitstream(&m_pmfxBS);
            MSDK_BREAK_ON_ERROR(sts); 
        }
        else if (MFX_ERR_MORE_SURFACE == sts)
        {
            for(i=0; i < MSDK_DEC_WAIT_INTERVAL; i++)
            {
                pmfxSurface = GetFreeSurface();
                if(pmfxSurface)
                    break;
                else
                {
                    MSDK_SLEEP(1);
                }
            }
            MSDK_CHECK_POINTER(pmfxSurface, MFX_ERR_MEMORY_ALLOC);
        }
        
        sts = m_pmfxDEC->DecodeFrameAsync(m_pmfxBS,pmfxSurface,&pExtSurface->pSurface, &pExtSurface->Syncp);
        if(MFX_ERR_NONE < sts && pExtSurface->Syncp)
            sts = MFX_ERR_NONE;      
    }
    return sts;
     
}
mfxStatus Decoder::DecodeLastFrame(ExtendedSurface *pExtSurface)
{
    mfxFrameSurface1 *pmfxSurface = NULL; 
    mfxStatus sts = MFX_ERR_MORE_SURFACE;
    mfxU32 i = 0; 
    
    while(MFX_ERR_MORE_SURFACE == sts || MFX_WRN_DEVICE_BUSY == sts) 
    {
        if(MFX_WRN_DEVICE_BUSY == sts)
            MSDK_SLEEP(1);
        
        for(i =0 ; i < MSDK_DEC_WAIT_INTERVAL; i+=5)
        {
            pmfxSurface = GetFreeSurface();
            if(pmfxSurface)
                break;
            else
            {
                MSDK_SLEEP(1); 
            }
        }
        MSDK_CHECK_POINTER(pmfxSurface,MFX_ERR_MEMORY_ALLOC);
        
        sts = m_pmfxDEC->DecodeFrameAsync(NULL, pmfxSurface, &pExtSurface->pSurface, &pExtSurface->Syncp);
    }
    return sts; 
    
}

void Decoder::NoMoreFramesSignal(ExtendedSurface &DecExtSurface)
{
    SafetySurfaceBuffer *pNextBuffer = m_pBuffer; 
    
    DecExtSurface.pSurface = NULL;
    pNextBuffer->AddSurface(DecExtSurface);
    while(pNextBuffer->m_pNext)
    {
        pNextBuffer = pNextBuffer->m_pNext; 
        pNextBuffer->AddSurface(DecExtSurface);
       
    }    
}

mfxFrameSurface1* Decoder::GetFreeSurface()
{
    SurfPointersArray *pArray; 
    
    pArray = &m_pSurfaceDecPool; 
  
    
    for(mfxU32 i = 0;  i < pArray->size(); i++)
    {
        if(!(*pArray)[i]->Data.Locked)
            return((*pArray)[i]);
        
    }
    return NULL;
    
}

mfxStatus Decoder::Join(MFXVideoSession* pChildSession)
{
    mfxStatus sts = MFX_ERR_NONE; 
    
    MSDK_CHECK_POINTER(pChildSession, MFX_ERR_NULL_PTR);
    
    sts = m_pmfxSession->JoinSession(*pChildSession);
 
    return sts; 
}