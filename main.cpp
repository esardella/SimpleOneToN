/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   main.cpp
 * Author: esardell
 *
 * Created on December 15, 2015, 10:37 AM
 */

#include <cstdlib>
#include "sample_defs.h"
#include "transcoder.h"



int main(int argc, char** argv) {
 
 mfxStatus sts; 
 Transcoder ts; 
    
    sts = ts.Init(argc,argv);
    MSDK_CHECK_PARSE_RESULT(sts, MFX_ERR_NONE, 1);
    
    ts.Run();
}

