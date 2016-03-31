


#include "transcoder.h"

mfxU32 MFX_STDCALL ThreadRoutine(void *pObj)
{
   
    ThreadContext *pContext = (ThreadContext *)pObj; 
    pContext->transcodingSts = MFX_ERR_NONE; 
    bool bSink = false; 
    
    if(pContext->pDecoder.get())
        bSink = true; 
    

    while(MFX_ERR_NONE == pContext->transcodingSts) 
        {
            if(bSink)
            { 
                pContext->transcodingSts = pContext->pDecoder->Run();
             
            }
            else
                pContext->transcodingSts = pContext->pEncoder->Run(); 
        }

    MSDK_IGNORE_MFX_STS(pContext->transcodingSts, MFX_ERR_MORE_DATA); 
    return 0;    
}





Transcoder::Transcoder()
{
    
    
}

Transcoder::~Transcoder()
{
    while(m_pSessionArray.size())
    {
        delete m_pSessionArray[m_pSessionArray.size()-1];
        m_pSessionArray[m_pSessionArray.size()-1] = NULL; 
        m_pSessionArray.pop_back(); 
    }
    
    while(m_pBufferArray.size())
    {
        delete m_pBufferArray[m_pBufferArray.size() - 1];
        m_pBufferArray[m_pBufferArray.size()-1] = NULL; 
        m_pBufferArray.pop_back();
    }
    
}

mfxStatus Transcoder::Init(int argc, char* argv[])
{
    mfxStatus sts; 
    sInputParams InputParams; 
    mfxHDL hdl = NULL; 
    Decoder * pParent = NULL; 
    SafetySurfaceBuffer *pBuffer = NULL; 
    int BufferCounter = 0; 
    
    
    sts = m_parser.ParseCmdLine(argc, argv);
    MSDK_CHECK_PARSE_RESULT(sts,MFX_ERR_NONE,sts);
    
    while(m_parser.GetNextSessionParams(InputParams))
    {
        m_InputParamsArray.push_back(InputParams);
        InputParams.Reset();
    }
     
    m_pAllocParams.reset(new vaapiAllocatorParams);
    m_hwdev.reset(CreateVAAPIDevice());
    
    sts = m_hwdev->Init(NULL,0, MSDKAdapter::GetNumber());
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    
    sts = m_hwdev->GetHandle(MFX_HANDLE_VA_DISPLAY, (mfxHDL*)&hdl);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE,sts);
    
    vaapiAllocatorParams *pVAAPIParams = dynamic_cast<vaapiAllocatorParams*>(m_pAllocParams.get());
    pVAAPIParams->m_dpy = (VADisplay)hdl;    
    
    
    CreateSafteyBuffers();
    
    for(mfxU16 i =0; i < m_InputParamsArray.size(); i++)
     {
        std::auto_ptr<ThreadContext> pThreadPipeline(new ThreadContext);
        pThreadPipeline->transcodingSts = MFX_ERR_NONE; 
   
        if(Sink == m_InputParamsArray[i].eMode)
        {
            pThreadPipeline->pDecoder.reset(new Decoder);
            pParent = pThreadPipeline->pDecoder.get();          
            pBuffer = m_pBufferArray[m_pBufferArray.size()-1];
            
            sts = pParent->Init(&m_InputParamsArray[i],hdl, pBuffer ); 
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
           
        }
        else if(Source == m_InputParamsArray[i].eMode)
        {
            pThreadPipeline->pEncoder.reset(new Encoder);
            pThreadPipeline->pEncoder->SetParent(pParent);     //pointer to sink 
            pBuffer = m_pBufferArray[BufferCounter];
            BufferCounter++;
            pThreadPipeline->pEncoder->Init(&m_InputParamsArray[i], hdl, pBuffer);      
        }
        m_pSessionArray.push_back(pThreadPipeline.release());
     }
    return MFX_ERR_NONE; 
}



mfxStatus Transcoder::Run()
{
    mfxU32 totalSessions, i;
    mfxStatus sts; 
    MSDKThread *pThread = NULL; 
    
    
    msdk_printf(MSDK_STRING("Transcoding Started \n"));
   
    totalSessions = m_pSessionArray.size(); 
    
    for(i=0; i < totalSessions; i++)
    {
        pThread = new MSDKThread(sts, ThreadRoutine,(void *)m_pSessionArray[i]);
        m_HDLArray.push_back(pThread);
    }
    for(i =0 ; i < m_pSessionArray.size(); i++)
    {
        m_HDLArray[i]->Wait();  
    }
    
    msdk_printf(MSDK_STRING("Transcoding Finished\n"));
        
}     


mfxStatus  Transcoder::CreateSafteyBuffers()
{
    SafetySurfaceBuffer* pBuffer = NULL;
    SafetySurfaceBuffer* pPrevBuffer = NULL;
    
    for(mfxU32 i = 0; i < m_InputParamsArray.size(); i++)
    {
        if(Source == m_InputParamsArray[i].eMode)
        {
            pBuffer = new SafetySurfaceBuffer(pPrevBuffer);
            pPrevBuffer = pBuffer;
            m_pBufferArray.push_back(pBuffer);
        }
    }
    return MFX_ERR_NONE;     
}
