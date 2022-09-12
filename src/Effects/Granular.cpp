/*
  ZynAddSubFX - a software synthesizer

  Granular.cpp - Granulareration effect
  Copyright (C) 2002-2005 Nasca Octavian Paul
  Author: Nasca Octavian Paul

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.
*/

#include "Granular.h"
#include "../Misc/Util.h"
#include "../Misc/Allocator.h"
#include "../DSP/AnalogFilter.h"
#include "../DSP/Unison.h"
#include <cmath>
#include <rtosc/ports.h>
#include <rtosc/port-sugar.h>
namespace zyn {

#define rObject Granular
#define rBegin [](const char *msg, rtosc::RtData &d) {
#define rEnd }

rtosc::Ports Granular::ports = {
    {"preset::i", rOptions(Gran 1, Gran2 , Gran 3, Gran 4)
                  rProp(parameter)
                  rDoc("Instrument Presets"), 0,
                  rBegin;
                  rObject *o = (rObject*)d.obj;
                  if(rtosc_narguments(msg))
                      o->setpreset(rtosc_argument(msg, 0).i);
                  else
                      d.reply(d.loc, "i", o->Ppreset);
                  rEnd},
    rEffParVol(rDefault(90),  rPresets(100, 100, 110, 85)),
    rEffParPan(rPresets(8, 80, 100, 110)),
    rEffParOpt(Pprogram,    2, rShort("Program"), rOptions(Standard, Incr-Pulse, Decr-Pulse, Prog3, Prog4), rPresets(Standard, Standard, Standard, Standard), "Internal Program"),
    rEffPar(Ptriglevel,  3, rShort("tg.lev"), rPresets(24, 24, 24, 24), "amplitude to detect sample start "),
    rEffPar(Plength,4, , rShort("Smp.len"), rPresets(8, 42, 71, 71), "Length of sample"),
    rEffPar(Prepeatstime, 5, rShort("R.Tm"), rPresets(62, 127, 51, 114), "Repeat trigger time"),
    rEffParB(Prepeatsnum, 6, rLinear(0,20), rShort("R.Num"), rPresets(10, 75, 21, 75), "Number of repeat grains"),
    rEffPar(Prepeatslen, 7, rShort("R.Len"), rPresets(83, 71, 78, 78), "Length of each repeat"),
    rEffParOpt(Pfwdreverse, 8, rShort("Fw-Rv"), rOptions(Forward, Alternate, Reverse, Random, Remote), rPresets(Forward, Reverse, Alternate, Forward), "Foeward-Reverse-Remote select "),
    rEffPar(Pfade, 9, rShort("Fade"), rPresets(64, 100, 64, 64), "Repeats fade in/out"),
    rParamI(statusbits, "Sample status data"),
    rEffPar(Pcrossover,10, rShort("Crossover"), rPresets(127, 100, 64, 64), "Alternative repeats crossover"),
    rEffPar(P11,11, rShort("P.11"), rPresets(127, 100, 64, 64), "Spare parameter 11"),
    rEffPar(P12,12, rShort("P.12"), rPresets(127, 100, 64, 64), "Spare parameter 12"),
    rParamI(repeatbits, "Repeat status data"),
    rParamI(requestbits, "Requests to start samples and repeats"),
};
#undef rBegin
#undef rEnd
#undef rObject

Granular::Granular(EffectParams pars)
    :Effect(pars)
{
    setpreset(Ppreset);
    initialized=false;
    loadcycle=0;
    mode=0; // local sample and play
    for (int n=0; n<NUMSAMPLES; ++n) {
        sample[n].left=nullptr;
        sample[n].right=nullptr;
        sample[n].numbuffers=0;
        sample[n].inuse=false;
        sample[n].repeatscounter=0;
        sample[n].loadcounter=0;
        sample[n].triggertimer=0;
        sample[n].repeatstriggertime=0;
        sample[n].numActiveRepeats=0;
     }
   currsample=nullptr;
    for (int n=0; n<MAXGRAINS; ++n) {
        grain[n].timer=0;
        grain[n].inuse=0;
        grain[n].samplenumber=0;
        grain[n].sample=nullptr;
        grain[n].length=0;
        grain[n].reverse=false;
        grain[n].fade=0.0f;
        grain[n].repeatnumber=0;
        grain[n].repeatcross=0.0;

   }
   statusbits=0;
   requestbits=0;
   prevSample=0;

    nextgrain=0;
    nextsample=0;
    ctr=slowctr=0;
    revctr=false;
//    repeatcross=0.8; //This will eventually be a control parameter knob
    crossctr=false;  
    TotalGrains=0;
    saveefxoutl=0.0; // for debugging
    saveefxoutr=0.0; // for debugging

    // These were parameters but who knowas how to set them!!! Trial and error was used!
    ampsns  = 0.6;
    ampsmooth = 0.95;
    rms_mid=rms_max=rms_min=0;

}


Granular::~Granular()
{
}

//Cleanup the effect
void Granular::cleanup(void)
{
}

////////////////////////////////////////
//work out a multiplier for each grain depending on the fade position
// 64 = no fade.
// 0 = fade out to 0
// 127 = fade in from 0
float Granular::getfadefactor(int repeat, int total) {


   // return 1.0f;


    float fade1=(float)(2.0f*(float)(Pfade)/127.0f)-1.0f; // fade from -1 to +1
    float f=((float)repeat)/(float)total;
    if (fade1<0.0f) {
        return ((fade1+1.0)*f);
    } else {
        return 1.0-fade1*f; // FADE OUT WORKS
    }
}
///////////////////////////////////////
// Pfwdreverse bits 0-1 are repeat directions (00-fwd, 01-alt 02-rev 03-rand)
// Pfwdreverse bit 2 True = remote control only ( no internal triggering of samples)
//////////////////////////////////////
//Higher modes are manual control from a remote interface via automation
#define REMOTE_MODE (Pfwdreverse&0x04)
#define OVERLAY_REPEATS (Pfwdreverse&0x08)
///////////////////////////////////////
bool Granular::getdirection() { // true = reverse
    revctr=!revctr;
    switch (Pfwdreverse%4) {
    default:
    case 0:
        return false; //forward
    case 2:
        return true; //reverse
    case 1:
        return revctr;  //alternate
    case 3:
        return rand()%2; //all other values signify remote controls
    }
}
////////////////////////////////////////
void Granular::playgrain(Grain *cgrain, Sample *gsample) {

            int frame;
            float fade;
            float fadelimit=P12; // FADEIN_COUNT has been #defined



            if (cgrain->reverse) {
                frame=cgrain->length-cgrain->timer;
            } else {
                frame=cgrain->timer;
            }
            if (frame>=0) { // just incase grainline has changed while playing!
                for(int i = 0; i < buffersize; ++i) {
                    if (frame<gsample->numbuffers) {  // protection against the buffer size of new sample has decreased
                        if (cgrain->fadeinout<fadelimit/*FADEIN_COUNT*/ && cgrain->timer==1) { // fade in on first buffer of grain
                            cgrain->fadeinout++;
                        }
                        if (cgrain->timer==cgrain->length && i>buffersize-fadelimit/*FADEIN_COUNT*/) { // fade out on last buffer of grain
                            if (cgrain->fadeinout>1)
                                cgrain->fadeinout--;
                        }
                        fade=cgrain->fade*(float)(cgrain->fadeinout/fadelimit/*FADEIN_COUNT*/); // eliminate glitches at start and end of grain


                        if (cgrain->reverse ){
                            if (frame>0) {
                                if (gsample->loadcounter>=gsample->numbuffers) { //  need entire sample before we can reverse it!
                                    efxoutl[i] += gsample->left[(buffersize-i-1)+(frame-1)*buffersize]*fade; 
                                    efxoutr[i] += gsample->right[(buffersize-i-1)+(frame-1)*buffersize]*fade;
                                }
                            }
                        } else {
                            efxoutl[i] += gsample->left[i+frame*buffersize]*fade;
                            efxoutr[i] += gsample->right[i+frame*buffersize]*fade;
                        }
                    efxoutl[i]*=pangainL*cgrain->repeatcross*2.0; // deal with panning
                    efxoutr[i]*=pangainR*(1.0-cgrain->repeatcross)*2.0;

                    if (efxoutl[i]>saveefxoutl) {
                        saveefxoutl=efxoutl[i];
                        printf("\n\nAudio Left max exceeded %f frame=%d fade=%f\n\n\n\n", efxoutl[i], frame, fade);
                    }
                    if (efxoutr[i]>saveefxoutr) {
                        saveefxoutr=efxoutr[i];
                        printf("\n\nAudio Right max exceeded %f frame=%d fase=%f\n\n\n\n", efxoutr[i], frame, fade);
                    }
                }
            }
        }

}
////////////////////////////////////////
void Granular::playgrains() {
    for(int i = 0; i < buffersize; ++i) {
        efxoutl[i]=0.0f;
        efxoutr[i]=0.0f;
    }
    for (int n=0; n<MAXGRAINS; ++n) {
        Grain *cgrain=&grain[n];
        if (cgrain->inuse) {
            Sample *gsample=cgrain->sample;
            cgrain->timer++;
            if (cgrain->timer>=cgrain->length) { // grain has finished
//                gsample->currentrepeatscounter--; // in overlay mode will tell system to launch next repeat
                printf("End grain %d\n", n);
               if (cgrain->repeatnumber>=gsample->repeatsnum) { //last grain to use this sample has finished so can free the sample
                    repeatbits&=~(3<<(cgrain->samplenumber*2)); // clear the status for the grain in question
                    printf("Sample %d complete\n", cgrain->samplenumber );
                }
               TerminateGrain(n);
               continue;
            }
            playgrain(cgrain, gsample);
           
        }
    }
}
////////////////////////////////////////
void Granular::TerminateGrain(int n) {
    Grain* cgrain=&grain[n];
    cgrain->inuse=0;
    cgrain->sample->numActiveRepeats--;
    cgrain->sample=nullptr;
    TotalGrains--;
    //printf("Terminate grain \n", n);

}
////////////////////////////////////////
void Granular::TerminateGrains(int samplenumber) {
   for (int n=0; n<MAXGRAINS; ++n) {
        if (grain[n].inuse && grain[n].samplenumber==samplenumber) {
            TerminateGrain(n);
        }
    }
}
////////////////////////////////////////
//up to 16 samples can be playing together
void Granular::launchrepeat(Sample *currsample, int samplenumber) {
    // rotate through all the grains using the next one
    for (int ctr=0; ctr<MAXGRAINS; ++ctr) {
        ++nextgrain;
        nextgrain%=MAXGRAINS;
        if (!grain[nextgrain].inuse) { 
            break;
        }
    }
    if (grain[nextgrain].inuse) { 
        printf("No unused grains so giving up!\n");
        return;
    }
    crossctr=!crossctr;
    Grain *cgrain=&grain[nextgrain];
    cgrain->inuse=1;
    cgrain->timer=0;
    cgrain->sample=currsample;
    cgrain->samplenumber=samplenumber;
    cgrain->repeatnumber=currsample->repeatscounter;

    cgrain->reverse=getdirection();
    cgrain->repeatcross=crossctr?repeatcross:(1.0-repeatcross);
    cgrain->fadeinout=1;
    // The grains can be shorter than, but cannot exceed the sample length
    if (repeatslen >currsample->numbuffers) {
        cgrain->length=currsample->numbuffers;
    } else {
        cgrain->length=repeatslen;
    }  
    TotalGrains++; 
    cgrain->fade=getfadefactor(currsample->repeatscounter, currsample->repeatsnum);
    printf("LaunchRepeat %d repeat %d[of %d] triggertime=%d sample=%d max=%f length=%d fade=%f reverse=%d program=%d TotalGrains=%d\n", 
                    nextgrain, currsample->repeatscounter, currsample->repeatsnum, currsample->repeatstriggertime, samplenumber, 
                    currsample->max, cgrain->length, cgrain->fade, cgrain->reverse, Pprogram, TotalGrains);

    if (currsample->repeatsnum==0) { // look for a problem !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        int crash_please=5;
        int m=0;
        crash_please=crash_please/m; 
    }
}
////////////////////////////////////////
// Some program modes may vary the repeatstriggertime per repeat
int Granular::setrepeatstime(Sample *csample) {
    switch(Pprogram) {
    default:
    case 0: // standard
        return csample->repeatstriggertime;
    case 1:
        return csample->repeatstriggertime+1*csample->repeatscounter;
    case 2:
        return csample->repeatstriggertime-1*(csample->repeatscounter);
    }
}
////////////////////////////////////////
void Granular::launchSampleRepeatsWhenRequired(Sample *csample, int samplenumber) {
    if (csample->inuse) {
        if (csample->triggertimer<=0) {  // triggertimer must always complete before starting a new repeat  
            if (OVERLAY_REPEATS  ||                         //in overlay mode start another repeat juat based on triggertimer    
                        csample->numActiveRepeats<=0) {    // otherwise launch if all active repeats have ended
                if (csample->repeatscounter<csample->repeatsnum) {
                    csample->repeatscounter++;
                    csample->triggertimer=setrepeatstime(csample);
                    csample->numActiveRepeats++;
                    launchrepeat(csample, samplenumber);
                }
            }
        } else {
            --csample->triggertimer;
        }
    }
}
////////////////////////////////////////
void Granular::launchrepeatsWhenRequired(void) {
//    bool debugfound=0;
    for (int s=0; s<NUMSAMPLES; ++s) {
        Sample *csample=&sample[s];
        launchSampleRepeatsWhenRequired(csample, s);
    }
}
////////////////////////////////////////
void Granular::startSample(Sample *currsample, int samplenumber) {
    currsample->inuse=true;
    currsample->numbuffers=samplelen; //How many buffere need to be stored in the sample
    currsample->loadcounter=0;
    currsample->triggertimer=0;
    currsample->repeatscounter=0;
    currsample->max=0;
    // End any grains that use this sample
    TerminateGrains(samplenumber);
   //Allocate (or reallocate) space for the sample
    currsample->left=(float*)realloc(currsample->left, currsample->numbuffers*buffersize*sizeof(float));
    memset(currsample->left, currsample->numbuffers*buffersize*sizeof(float), 0);
    currsample->right=(float*)realloc(currsample->right, currsample->numbuffers*buffersize*sizeof(float));
    memset(currsample->right, currsample->numbuffers*buffersize*sizeof(float), 0);

}
////////////////////////////////////////
bool Granular::continueSample(Sample *currsample, const Stereo<float *> &input) {
    //Copy the buffer into the sample memory
    float m=0;
    for(int i = 0; i < buffersize; ++i) {
        m=input.l[i];
        if (m<0) {m*=-1;}
        if (m>currsample->max) {
            currsample->max=m; // planning to use max to decide how best to mix repeats together
        }
        m=input.r[i];
        if (m<0){m*=-1;}
        if (m>currsample->max) {
            currsample->max=m;
        }
        currsample->left[i+currsample->loadcounter*buffersize] = input.l[i];
        currsample->right[i+currsample->loadcounter*buffersize] = input.r[i];
    }
    currsample->loadcounter++;
    if (currsample->loadcounter>=currsample->numbuffers) {
        printf("Sample complete max=%f\n", currsample->max);
        return true; // sample conplete
    }
    return false;
}
////////////////////////////////////////
//Remote mode taking a sample is triggered via OSC port
//Replays can be triggered remotely via OSC or programatically here depending on PForceSample bits 
void Granular::remoteMode(const Stereo<float *> &input) {
    uint32_t request=requestbits;
    uint32_t sbits=0;
    uint32_t rbits=repeatbits;
    //repeatbits=requestbits; // debug

    requestbits=0; // tell display request has been actioned
    if (request!=0) {
        printf("requestbits=%04X, repeatbits=%04X\n", request, rbits);
        uint32_t samplenumber=request&0x07;
        Sample *currsample=&sample[samplenumber];

        if (request&0x08) { // start taking a sample
            prevSample=samplenumber; 
            printf("startSample %d\n", samplenumber);
            sbits^=(3<<(samplenumber*2)); // clear the status for the sample in question
            sbits|=(1<<(samplenumber*2)); //set the status bits for the sample in question
            startSample(currsample, samplenumber);
            currsample->repeatsnum=0; // dont start repeating right away!
            currsample->repeatstriggertime=0; // dont start repeating right away!
            currsample->triggertimer=0; // dont start repeating right away!
//              printf("remote-startSample %d repeatsnum %d repeatstriggertime=%d\n", samplenumber,
//                    currsample->repeatsnum, currsample->repeatstriggertime);
        } else if (request&0x10 ) { //start a repeat
            if (currsample->inuse) {

                rbits&=~(3<<(samplenumber*2)); // clear the status for the repeat in question
                rbits|=(1<<(samplenumber*2)); //set the status bits for the repeat in question
                currsample->triggertimer=0;
                currsample->repeatscounter=0;
                if (Prepeatsnum==0) { // Just for remote mode almost infinite repeats
                    currsample->repeatsnum=1;
                } else {
                    currsample->repeatsnum=repeatsnum;
                }
                currsample->repeatstriggertime=repeatstriggertime;
                printf("remote-launchRepeat (Request bit 0x10) %d repeat %d[of %d] repeatstriggertime=%d sample=%d program=%d\n", nextgrain, currsample->repeatscounter ,
                    currsample->repeatsnum, currsample->repeatstriggertime, samplenumber, Pprogram);

//                launchrepeat(currsample, samplenumber);
            }
        } else if (request&0x20) {  // erase sample
            TerminateGrains(samplenumber);
            currsample->repeatsnum=0;
            currsample->inuse=false;
            printf("erase sample %d\n", samplenumber);
        } else if (request&0x40) {  // next sample ( round robin )
        // starting with the last sample taken step through all 8 samples and start the first empty one found
        // if none are empty then use the samp,e after the last one that was taken - use previousSample
            printf("next sample after %d\n", samplenumber);
            bool done=false;
            for (int sp=0; sp<NUMSAMPLES; ++sp) {
                samplenumber=(prevSample+sp)%NUMSAMPLES;
                if (!sample[samplenumber].inuse) {
                    prevSample=samplenumber; 
                    currsample=&sample[samplenumber];
                    printf("startSample (Next) %d\n", samplenumber);
                    sbits^=(3<<(samplenumber*2)); // clear the status for the sample in question
                    sbits|=(1<<(samplenumber*2)); //set the status bits for the sample in question
                    startSample(currsample, samplenumber);
                    currsample->repeatsnum=0; // dont start repeating right away!
                    currsample->repeatstriggertime=0; // dont start repeating right away!
                    currsample->triggertimer=0; // dont start repeating right away!
                    done=true;
                    break;
                }

            }
            if (!done) {
                // all samples are in use - so use the next one round robin
                samplenumber=(prevSample+1)%NUMSAMPLES;
                prevSample=samplenumber; 
                TerminateGrains(samplenumber);
                currsample=&sample[samplenumber];
                printf("startSample (Next) %d\n", samplenumber);
                sbits^=(3<<(samplenumber*2)); // clear the status for the sample in question
                sbits|=(1<<(samplenumber*2)); //set the status bits for the sample in question
                startSample(currsample, samplenumber);
                currsample->repeatsnum=0; // dont start repeating right away!
                currsample->repeatstriggertime=0; // dont start repeating right away!
                currsample->triggertimer=0; // dont start repeating right away!
            }

        } else if (request&0x80) {  // play sequence
            // will cause the replay to cycle through all the samples - change sample on each grain.
            // May be program selectable how to sequence this. - use previousRepeatSample
            printf("play sequence %d\n", samplenumber);

        }
    }
    for (int n=0; n<NUMSAMPLES; ++n) {
        Sample* currsample=&sample[n];
        sbits&=~(3<<(n*2)); // clear the status for the sample in question
        if (currsample->inuse) {
            if (currsample->loadcounter<currsample->numbuffers)  {
                sbits|=(1<<(n*2)); //set the status bits for the sample in question
                continueSample(currsample, input);
//                printf("remote-ContinueSample %d\n", n);
            } else {
                sbits|=(2<<(n*2)); //set the status bits for the sample in question
            }
        }
    }
// check loadcounter and call continueSample
    statusbits=sbits; // now we can write out fully constructed statusbits
    repeatbits=rbits;
}
////////////////////////////////////////
// Local mode requires a pulse of sound to trigger taking a sample.
// Local controls are used to decide when repeats are played.
void Granular::localMode(const Stereo<float *> &input) {
   if (!initialized) {
        initialized=true;
        ms1=ms2=ms3=ms4=0.0f;
    }
    // No idea how this works in DynFilter.cpp. It had to be changes a lot!
    float x;   /// typically 0.33
    float ampsmooth2;
    for(int i = 0; i < buffersize; ++i) {
        x = (fabsf(input.l[i]) + fabsf(input.r[i])) * 0.5f; // from DynamicFilter.cpp
        ampsmooth2 = powf(ampsmooth, 0.2f) * 0.3f;
        ms1 = ms1 * (1.0f - ampsmooth2) + x * ampsmooth2 + 1e-10;
    }

//    const float ampsmooth2 = powf(ampsmooth, 0.2f) * 0.3f;
    ms2 = ms2 * (1.0f - ampsmooth2) + ms1 * ampsmooth2;
    ms3 = ms3 * (1.0f - ampsmooth2) + ms2 * ampsmooth2;
    ms4 = ms4 * (1.0f - ampsmooth2) + ms3 * ampsmooth2;
    const float rms = (sqrtf(ms4)) * ampsns;

// lets try to detect sone useful max and minimum values

    if (rms>rms_max) {
        rms_max+=(rms-rms_max)*0.8;
    }
    if (rms<rms_min) {
        rms_min+=(rms-rms_min)*0.8;
    }
    rms_mid=(rms_max-rms_min)/2;
// would require max and min to revert to mean over time
    rms_min+=(rms_mid-rms_min)*0.01;
    rms_max-=(rms_max-rms_mid)*0.01;


//    if (!(ctr%5)) {
//        printf ("rms=%1.3f min=%1.3f mid=%1.3f max=%1.3f max-min=%1.3f\n", rms, rms_min, rms_mid, rms_max, rms_max-rms_min);
//    }


    switch(loadcycle ) { // we are either waiting for envelope detection or loading an envelope
        case 0: // wait for an envelope to be detected
            if (rms < triglevel) {
                
//                if (!(ctr%1000)) {
//                    printf("rms=%f < triglevel=%f\n", rms, triglevel);
//                }
                break;
            }   
            // take the next sample cyclical
            ++nextsample;
            nextsample%=NUMSAMPLES;
            printf("---------------loadcycle=1\n");
            currsample=&sample[nextsample];
            startSample(currsample, nextsample);
            currsample->repeatsnum=repeatsnum;
            currsample->repeatstriggertime=repeatstriggertime;
            currsample->triggertimer=repeatstriggertime; // dont start first repeat until timer has counted down from sample starting
             printf( "start sample %d at rms=%f\n", nextsample, rms);
            printf("sampling envelope  numbuffers=%d, repeatsnum=%d repeatstriggertime=%d\n", currsample->numbuffers, currsample->repeatsnum, currsample->repeatstriggertime);
            loadcycle=1;
              // fall thro'
        case 1: 
            // sample envelope - called once per buffer so is that ok to align env to audio buffer - we'll see!
            // in fact with a typical buffersize of 256 and a samplerate of 48000 a buffer is 5.333mSec
            if (!continueSample(currsample, input)) {
                break;
            }
            loadcycle=2;
            printf("--------------loadcycle=2\n");
            // fall thro'

        case 2:
            if (rms > (triglevel*0.9)) { //put some hysteresis on the trigger
                break;
            }

            loadcycle=0;
                printf("--------------loadcycle=0 Pfwdreverse=%d Pfade=%d \n", Pfwdreverse, Pfade);
            break;

        default:
            break;
    }
}
////////////////////////////////////////
//Effect output
//Effect output
void Granular::out(const Stereo<float *> &input)
{
    if (REMOTE_MODE) {
        remoteMode(input);
    } else {
        localMode(input);
    }
    launchrepeatsWhenRequired();
    playgrains();
}
////////////////////////////////////////
//Parameter control
void Granular::setvolume(unsigned char _Pvolume)
{
    Pvolume = _Pvolume;

    if(insertion == 0) {
        if (Pvolume == 0) {
            outvolume = 0.0f;
        } else {
            outvolume = powf(0.01f, (1.0f - Pvolume / 127.0f)) * 4.0f;
        }
        volume    = 1.0f;
    }
    else
        volume = outvolume = Pvolume / 127.0f;
    if(Pvolume == 0)
        cleanup();
}

void Granular::setrepeatslen(unsigned char _Prepeatslen)
{
    Prepeatslen = _Prepeatslen;
    repeatslen=1+TimeToBuffers((float)(Prepeatslen*Prepeatslen/127.0f/127.0f)*10.0); // max 10 seconds
}

void Granular::setfwdreverse(unsigned char _Pfwdreverse)
{
    Pfwdreverse = _Pfwdreverse;

}

void Granular::settriglevel(unsigned char _Ptriglevel)
{
    Ptriglevel = _Ptriglevel;
    triglevel = (float)(pow(Ptriglevel, 3)/127.0f/127.0f/127.0f)*1.5f;
}

void Granular::setlength(unsigned char _Plength)
{
    Plength = _Plength;
    samplelen=1+TimeToBuffers((float)(Plength*Plength/127.0f/127.0f)*10.0); // max 10 seconds. length of 0 would cause a memory fault
//    samplelen=1+TimeToBuffers((float)pow(2, Plength)); // max 10 seconds. length of 0 would cause a memory fault
//    printf("Plength=%d pow(2, Plength)=%f samplelength=%d\n", Plength, (float)pow(2, Plength), samplelen);
}



void Granular::setrepeatsnum(unsigned char _Prepeatsnum)
{
    Prepeatsnum = _Prepeatsnum;
    repeatsnum = Prepeatsnum;
}
void Granular::setrepeatstime(unsigned char _Prepeatstime)
{
    Prepeatstime = _Prepeatstime;
    repeatstriggertime=1+TimeToBuffers((float)(Prepeatstime*Prepeatstime/127.0f/127.0f)*10.0); // max 10 seconds
}

void Granular::setfade(unsigned char _Pfade)
{
    Pfade = _Pfade;
}

void Granular::setcrossover(unsigned char _Pcrossover)
{
    Pcrossover= _Pcrossover;
    repeatcross=(float)(Pcrossover+50.0f)/100.0f; // make this 0-1
}
void Granular::setp11(unsigned char _P11)
{
    P11= _P11+1;
}
void Granular::setp12(unsigned char _P12)
{
    if (_P12<=0) {
        P12=1;
    }  else { 
        P12= _P12;
    }

}

void Granular::setprogram(unsigned char _Pprogram)
{
    Pprogram= _Pprogram;

}

unsigned char Granular::getpresetpar(unsigned char npreset, unsigned int npar)
{
    // These get loaded when the preset is changed.
#define	PRESET_SIZE 10
#define	NUM_PRESETS 4
    static const unsigned char presets[NUM_PRESETS][PRESET_SIZE] = {
        //Gran 1
        {80,  64, 63,  24, 50,  60, 70, 85,  5,  83},
        //Gran 2
        {80,  64, 69,  35, 30,  40, 50, 127, 0,  71},
        //Gran 3
        {80,  64, 69,  24, 20,  50, 60, 105, 75, 78},
        //Gran 4
        {90,  64, 51,  10, 80,  90, 100, 95, 21, 67}
    };
    if(npreset < NUM_PRESETS && npar < PRESET_SIZE) {
        if (npar == 0 && insertion != 0) {
            /* lower the volume if Granular is insertion effect */
            return presets[npreset][npar] / 2;
        }
        return presets[npreset][npar];
    }
    return 0;
}

void Granular::setpreset(unsigned char npreset)
{
    if(npreset >= NUM_PRESETS)
        npreset = NUM_PRESETS - 1;
    for(int n = 0; n != 128; n++)
        changepar(n, getpresetpar(npreset, n));
    Ppreset = npreset;
}

void Granular::changepar(int npar, unsigned char value)
{
    switch(npar) {
        case 0:
            setvolume(value);
            break;
        case 1:
            setpanning(value);
            break;
        case 2:
            setprogram(value);
            break;
        case 3:
            settriglevel(value);
            break;
        case 4:
            setlength(value);
            break;
        case 5:
            setrepeatstime(value);
            break;
        case 6:
            setrepeatsnum(value);
            break;
        case 7:
            setrepeatslen(value);
            break;
        case 8:
            setfwdreverse(value);
            break;
        case 9:
            setfade(value);
            break;
        case 10:
            setcrossover(value);
            break;
        case 11:
            setp11(value);
            break;
        case 12:
            setp12(value);
            break;
    }
}

unsigned char Granular::getpar(int npar) const
{
    switch(npar) {
        case 0:  return Pvolume;
        case 1:  return Ppanning;
        case 2:  return Pprogram;
        case 3:  return Ptriglevel;
        case 4:  return Plength;
        case 5:  return Prepeatstime;
        case 6:  return Prepeatsnum;
        case 7:  return Prepeatslen;
        case 8: return Pfwdreverse;
        case 9: return Pfade;
        case 10: return Pcrossover;
        case 11: return P11;
        case 12: return P12;
        default: return 0;
    }
}

}
