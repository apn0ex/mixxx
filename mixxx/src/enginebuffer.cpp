/***************************************************************************
                          enginebuffer.cpp  -  description
                             -------------------
    begin                : Wed Feb 20 2002
    copyright            : (C) 2002 by Tue and Ken Haste Andersen
    email                : 
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "enginebuffer.h"

#include <qevent.h>
#include "configobject.h"
#include "controlpushbutton.h"
#include "controlpotmeter.h"
#include "controlttrotary.h"
#include "controlengine.h"
#include "controlbeat.h"
#include "reader.h"
#include "readerextractbeat.h"
#include "enginebufferscalelinear.h"
#include "powermate.h"

class VisualChannel;
#ifdef __VISUALS__
  #include "wvisualwaveform.h"
  #include "visual/visualchannel.h"
#endif

EngineBuffer::EngineBuffer(PowerMate *_powermate, const char *_group)
{
    group = _group;
    powermate = _powermate;

    // Play button
    ControlPushButton *p = new ControlPushButton(ConfigKey(group, "play"), true);
    playButton = new ControlEngine(p);
    connect(playButton, SIGNAL(valueChanged(double)), this, SLOT(slotControlPlay(double)));
    playButton->set(0);

    // Reverse button
    p = new ControlPushButton(ConfigKey(group, "reverse"));
    reverseButton = new ControlEngine(p);
    reverseButton->set(0);
    
    // Fwd button
    p = new ControlPushButton(ConfigKey(group, "fwd"));
    fwdButton = new ControlEngine(p);
    fwdButton->set(0);

    // Back button
    p = new ControlPushButton(ConfigKey(group, "back"));
    backButton = new ControlEngine(p);
    backButton->set(0);

    // Start button
    p = new ControlPushButton(ConfigKey(group, "start"));
    startButton = new ControlEngine(p);
    connect(startButton, SIGNAL(valueChanged(double)), this, SLOT(slotControlStart(double)));
    startButton->set(0);

    // End button
    p = new ControlPushButton(ConfigKey(group, "end"));
    endButton = new ControlEngine(p);
    connect(endButton, SIGNAL(valueChanged(double)), this, SLOT(slotControlEnd(double)));
    endButton->set(0);

    // Cue set button:
    p = new ControlPushButton(ConfigKey(group, "cue_set"));
    buttonCueSet = new ControlEngine(p);
    connect(buttonCueSet, SIGNAL(valueChanged(double)), this, SLOT(slotControlCueSet(double)));

    // Cue goto button:
    p = new ControlPushButton(ConfigKey(group, "cue_goto"));
    buttonCueGoto = new ControlEngine(p);
    connect(buttonCueGoto, SIGNAL(valueChanged(double)), this, SLOT(slotControlCueGoto(double)));

    // Cue preview button:
    p = new ControlPushButton(ConfigKey(group, "cue_preview"));
    buttonCuePreview = new ControlEngine(p);
    connect(buttonCuePreview, SIGNAL(valueChanged(double)), this, SLOT(slotControlCuePreview(double)));

    // Playback rate slider
    ControlPotmeter *p2 = new ControlPotmeter(ConfigKey(group, "rate"), 0.9f, 1.1f);
    rateSlider = new ControlEngine(p2);

    // Permanent rate-change buttons
    p = new ControlPushButton(ConfigKey(group,"rate_perm_down"));
    buttonRatePermDown = new ControlEngine(p);
    connect(buttonRatePermDown, SIGNAL(valueChanged(double)), this, SLOT(slotControlRatePermDown(double)));
    p = new ControlPushButton(ConfigKey(group,"rate_perm_down_small"));
    buttonRatePermDownSmall = new ControlEngine(p);
    connect(buttonRatePermDownSmall, SIGNAL(valueChanged(double)), this, SLOT(slotControlRatePermDownSmall(double)));
    p = new ControlPushButton(ConfigKey(group,"rate_perm_up"));
    buttonRatePermUp = new ControlEngine(p);
    connect(buttonRatePermUp, SIGNAL(valueChanged(double)), this, SLOT(slotControlRatePermUp(double)));
    p = new ControlPushButton(ConfigKey(group,"rate_perm_up_small"));
    buttonRatePermUpSmall = new ControlEngine(p);
    connect(buttonRatePermUpSmall, SIGNAL(valueChanged(double)), this, SLOT(slotControlRatePermUpSmall(double)));

    // Temporary rate-change buttons
    p = new ControlPushButton(ConfigKey(group,"rate_temp_down"));
    buttonRateTempDown = new ControlEngine(p);
    connect(buttonRateTempDown, SIGNAL(valueChanged(double)), this, SLOT(slotControlRateTempDown(double)));
    p = new ControlPushButton(ConfigKey(group,"rate_temp_down_small"));
    buttonRateTempDownSmall = new ControlEngine(p);
    connect(buttonRateTempDownSmall, SIGNAL(valueChanged(double)), this, SLOT(slotControlRateTempDownSmall(double)));
    p = new ControlPushButton(ConfigKey(group,"rate_temp_up"));
    buttonRateTempUp = new ControlEngine(p);
    connect(buttonRateTempUp, SIGNAL(valueChanged(double)), this, SLOT(slotControlRateTempUp(double)));
    p = new ControlPushButton(ConfigKey(group,"rate_temp_up_small"));
    buttonRateTempUpSmall = new ControlEngine(p);
    connect(buttonRateTempUpSmall, SIGNAL(valueChanged(double)), this, SLOT(slotControlRateTempUpSmall(double)));

    // Wheel to control playback position/speed
    ControlTTRotary *p3 = new ControlTTRotary(ConfigKey(group, "wheel"));
    wheel = new ControlEngine(p3);

    // Slider to show and change song position
    ControlPotmeter *controlplaypos = new ControlPotmeter(ConfigKey(group, "playposition"), 0., 1.);
    playposSlider = new ControlEngine(controlplaypos);
    connect(playposSlider, SIGNAL(valueChanged(double)), this, SLOT(slotControlSeek(double)));

    // Potmeter used to communicate bufferpos_play to GUI thread
    ControlPotmeter *controlbufferpos = new ControlPotmeter(ConfigKey(group, "bufferplayposition"), 0., READBUFFERSIZE);
    bufferposSlider = new ControlEngine(controlbufferpos);

    // m_pTrackEnd is used to signal when at end of file during playback
    ControlObject *p5 = new ControlObject(ConfigKey(group, "TrackEnd"));
    m_pTrackEnd = new ControlEngine(p5);

    // Direction of rate slider
    p5 = new ControlObject(ConfigKey(group, "rate_dir"));
    m_pRateDir = new ControlEngine(p5);
    
    // TrackEndMode determines what to do at the end of a track
    p5 = new ControlObject(ConfigKey(group,"TrackEndMode"));
    m_pTrackEndMode = new ControlEngine(p5);
    m_pTrackEndMode->set((double)TRACK_END_MODE_STOP);
    
    // BPM control
    ControlBeat *p4 = new ControlBeat(ConfigKey(group, "bpm"));
    bpmControl = new ControlEngine(p4);
//    bpmControl->setNotify(this,(EngineMethod)&EngineBuffer::bpmChange);

    // Beat event control
    p2 = new ControlPotmeter(ConfigKey(group, "beatevent"));
    beatEventControl = new ControlEngine(p2);

    // Audio beat mark toggle
    p = new ControlPushButton(ConfigKey(group, "audiobeatmarks"));
    audioBeatMark = new ControlEngine(p);

    // Control file changed
//    filechanged = new ControlEngine(controlfilechanged);
//    filechanged->setNotify(this,(EngineMethod)&EngineBuffer::newtrack);

    setNewPlaypos(0.);

    reader = new Reader(this, &rate_exchange, &pause);
    read_buffer_prt = reader->getBufferWavePtr();
    file_length_old = -1;
    file_srate_old = 0;
    rate_old = 0;

    m_iBeatMarkSamplesLeft = 0;

    // Allocate buffer for processing:
    buffer = new CSAMPLE[MAX_BUFFER_LEN];

    // Construct scaling object
    scale = new EngineBufferScaleLinear(reader->getWavePtr());

    oldEvent = 0.;
    temp_rate = 0.;

    // Used in update of playpos slider
    m_iSamplesCalculated = 0;


    reader->start();
}

EngineBuffer::~EngineBuffer()
{

    delete playButton;
    delete wheel;
    delete rateSlider;
    delete buffer;
    delete scale;
    delete bufferposSlider;
    delete m_pTrackEnd;
}

void EngineBuffer::setVisual(WVisualWaveform *pVisualWaveform)
{
    VisualChannel *pVisualChannel = 0;
    // Try setting up visuals
#ifdef __VISUALS__
    if (pVisualWaveform)
    {
        // Add buffer as a visual channel
        pVisualChannel = pVisualWaveform->add((ControlPotmeter *)ControlObject::getControl(ConfigKey(group, "bufferplayposition")), group);
        reader->addVisual(pVisualChannel);
    }
#endif
}


Reader *EngineBuffer::getReader()
{
    return reader;
}

void EngineBuffer::setNewPlaypos(double newpos)
{
    filepos_play = newpos;
    bufferpos_play = 0.;

    // Update bufferposSlider
    bufferposSlider->set((CSAMPLE)bufferpos_play);

    // Ensures that the playpos slider gets updated in next process call
    m_iSamplesCalculated = 1000000;

    m_iBeatMarkSamplesLeft = 0;
}

const char *EngineBuffer::getGroup()
{
    return group;
}

int EngineBuffer::getPlaypos(int) // int Srate
{
    return 0; //(int)((CSAMPLE)visualPlaypos.read()/(2.*(CSAMPLE)file_srate/(CSAMPLE)Srate));
}


void EngineBuffer::slotControlSeek(double change)
{
//    qDebug("seeking... %f",change);

    // Find new playpos
    double new_playpos = change*file_length_old;
    if (new_playpos > file_length_old)
        new_playpos = file_length_old;
    if (new_playpos < 0.)
        new_playpos = 0.;

    // Seek reader
    reader->requestSeek(new_playpos);

    m_iBeatMarkSamplesLeft = 0;
//    filepos_play_exchange.write(filepos_play);
//    file->seek((long unsigned)filepos_play);
//    visualPlaypos.tryWrite(
}

// Set the cue point at the current play position:
void EngineBuffer::slotControlCueSet(double)
{
    reader->f_dCuePoint = filepos_play;
}

// Goto the cue point:
void EngineBuffer::slotControlCueGoto(double)
{
    // Seek to cue point
    reader->requestSeek(reader->f_dCuePoint);
    m_iBeatMarkSamplesLeft = 0;

    // Start playing
    playButton->set(1.);
}

void EngineBuffer::slotControlCuePreview(double)
{
//    qDebug("cue preview: %d",buttonCuePreview->get());
    if (buttonCuePreview->get()==0.)
    {
        // Stop playing (set playbutton to stoped) and seek to cue point
        playButton->set(0.);
        reader->requestSeek(reader->f_dCuePoint);
        m_iBeatMarkSamplesLeft = 0;
    }
    else
    {
        // Seek to cue point and start playing
        slotControlCueGoto();
    }
}

void EngineBuffer::slotControlPlay(double)
{
    // Set cue when play button is pressed for stopping the sound
    if (playButton->get()==0.)
        slotControlCueSet();
}

void EngineBuffer::slotControlStart(double)
{
    slotControlSeek(0.);
}

void EngineBuffer::slotControlEnd(double)
{
    slotControlSeek(1.);
}

/*
void EngineBuffer::bpmChange(double bpm)
{
    CSAMPLE filebpm = bpmbuffer[];

    baserate = baserate*(bpm/reader->->getBPM());
}
*/

void EngineBuffer::slotControlRatePermDown(double)
{
    // Adjusts temp rate down if button pressed
    if (buttonRatePermDown->get()==1.)
        rateSlider->sub(0.005);
}

void EngineBuffer::slotControlRatePermDownSmall(double)
{
    // Adjusts temp rate down if button pressed
    if (buttonRatePermDownSmall->get()==1.)
        rateSlider->sub(0.001);
}

void EngineBuffer::slotControlRatePermUp(double)
{
    // Adjusts temp rate up if button pressed
    if (buttonRatePermUp->get()==1.)
        rateSlider->add(0.005);
}

void EngineBuffer::slotControlRatePermUpSmall(double)
{
    // Adjusts temp rate up if button pressed
    if (buttonRatePermUpSmall->get()==1.)
        rateSlider->add(0.001);
}

void EngineBuffer::slotControlRateTempDown(double)
{
    // Adjusts temp rate down if button pressed, otherwise set to 0.
    if (buttonRateTempDown->get()==1.)
        temp_rate = -0.01;
    else
        temp_rate = 0.;
}

void EngineBuffer::slotControlRateTempDownSmall(double)
{
    // Adjusts temp rate down if button pressed, otherwise set to 0.
    if (buttonRateTempDownSmall->get()==1.)
        temp_rate = -0.001;
    else
        temp_rate = 0.;
}

void EngineBuffer::slotControlRateTempUp(double)
{
    // Adjusts temp rate up if button pressed, otherwise set to 0.
    if (buttonRateTempUp->get()==1.)
        temp_rate = 0.01;
    else
        temp_rate = 0.;
}

void EngineBuffer::slotControlRateTempUpSmall(double)
{
    // Adjusts temp rate up if button pressed, otherwise set to 0.
    if (buttonRateTempUpSmall->get()==1.)
        temp_rate = 0.001;
    else
        temp_rate = 0.;
}

inline bool even(long n)
{
//    if ((n/2) != (n+1)/2)
    if (n%2 != 0)
        return false;
    else
        return true;
}

CSAMPLE *EngineBuffer::process(const CSAMPLE *, const int buf_size)
{
    // pause can be locked if the reader is currently loading a new track.
    if (m_pTrackEnd->get()==0 && pause.tryLock())
    {
        // Try to fetch info from the reader
        bool readerinfo = false;
        long int filepos_start = 0;
        long int filepos_end = 0;
        if (reader->tryLock())
        {
            file_length_old = reader->getFileLength();
            file_srate_old = reader->getFileSrate();
            filepos_start = reader->getFileposStart();
            filepos_end = reader->getFileposEnd();
            reader->unlock();
            readerinfo = true;
        }

//        qDebug("filepos_play %f,\tstart %i,\tend %i\t info %i",filepos_play, filepos_start, filepos_end, readerinfo);



        //
        // Calculate rate
        //

        // Find BPM adjustment factor
        ReaderExtractBeat *beat = reader->getBeatPtr();
        double bpmrate = 1.;
        if (beat!=0)
        {
            CSAMPLE *bpmBuffer = beat->getBpmPtr();
            double filebpm = bpmBuffer[(int)(bufferpos_play*(beat->getBufferSize()/READCHUNKSIZE))];
//            if (bpmControl->get()>-1. && filebpm>-1.)
//                bpmrate = bpmControl->get()/filebpm;
            bpmControl->set(filebpm);
//        qDebug("bpmrate %f, filebpm %f, midibpm %f",bpmrate,filebpm,bpmControl->get());
        }
                                                
        // Determine direction of playback
        double dir = 1.;
        if (reverseButton->get()==1.)
            dir = -1.;

        double baserate = dir*bpmrate*((double)file_srate_old/(double)getPlaySrate());
        if (fwdButton->get()==1.)
            baserate = fabs(baserate)*5.;
        else if (backButton->get()==1.)
            baserate = fabs(baserate)*-5.;

        double rate;
        if (playButton->get()==1. || fwdButton->get()==1. || backButton->get()==1.)
            rate=(temp_rate*m_pRateDir->get())+wheel->get()+(1.+rateSlider->get()*m_pRateDir->get())*baserate;
        else
            rate=(temp_rate*m_pRateDir->get())+wheel->get()*baserate*20.;
/*
        //
        // Beat event control. Assume forward play
        //

        // Search for next beat
        ReaderExtractBeat *readerbeat = reader->getBeatPtr();
        bool *beatBuffer = (bool *)readerbeat->getBasePtr();
        int nextBeatPos;
        int beatBufferPos = bufferpos_play*((CSAMPLE)readerbeat->getBufferSize()/(CSAMPLE)READBUFFERSIZE);
        int i;
        for (i=beatBufferPos+1; i<beatBufferPos+readerbeat->getBufferSize(); i++)
            if (beatBuffer[i%readerbeat->getBufferSize()])
                break;
        if (beatBuffer[i%readerbeat->getBufferSize()])
            // Next beat was found
            nextBeatPos = (i%readerbeat->getBufferSize())*(READBUFFERSIZE/readerbeat->getBufferSize());
        else
            // No next beat was found
            nextBeatPos = bufferpos_play+buf_size;

        double event = beatEventControl->get();
        if (event > 0.)
        {
            qDebug("event: %f, playpos %f, nextBeatPos %i",event,bufferpos_play,nextBeatPos);
            //
            // Play next event
            //

            // Reset beat event control
            beatEventControl->set(0.);

            if (oldEvent>0.)
            {
                // Adjust bufferplaypos
                bufferpos_play = nextBeatPos;

                // Search for a new next beat position
                ReaderExtractBeat *readerbeat = reader->getBeatPtr();
                bool *beatBuffer = (bool *)readerbeat->getBasePtr();

                int beatBufferPos = bufferpos_play*((CSAMPLE)readerbeat->getBufferSize()/(CSAMPLE)READBUFFERSIZE);
                int i;
                for (i=beatBufferPos+1; i<beatBufferPos+readerbeat->getBufferSize(); i++)
                {
    //                qDebug("i %i",i);
                    if (beatBuffer[i%readerbeat->getBufferSize()])
                        break;
                }
                if (beatBuffer[i%readerbeat->getBufferSize()])
                    // Next beat was found
                    nextBeatPos = (i%readerbeat->getBufferSize())*(READBUFFERSIZE/readerbeat->getBufferSize());
                else
                    // No next beat was found
                    nextBeatPos = -1;
            }
            
            oldEvent = 1.;
        }
        else if (oldEvent==0.)
            nextBeatPos = -1;

//        qDebug("NextBeatPos :%i, bufDiff: %i",nextBeatPos,READBUFFERSIZE/readerbeat->getBufferSize());
*/
        
        // If the rate has changed, write it to the rate_exchange monitor
        if (rate != rate_old)
        {
            rate_exchange.tryWrite(rate);
            rate_old = rate;
            scale->setRate(rate);        
        }

        // Determine playback direction
        bool backwards = false;
        if (rate<0.)
            backwards = true;

        //qDebug("rate: %f, playpos: %f",rate,playButton->get());
        if (rate==0.)
        {
            for (int i=0; i<buf_size; i++)
                buffer[i]=0.;
        }
        else
        {
            // Check if we are at the boundaries of the file
            if ((filepos_play<0. && backwards==true) || (filepos_play>file_length_old && backwards==false))
            {
                for (int i=0; i<buf_size; i++)
                    buffer[i] = 0.;
            }
            else
            {
                // Perform scaling of Reader buffer into buffer
                CSAMPLE *output = scale->scale(bufferpos_play, buf_size);
                int i;
                for (i=0; i<buf_size; i++)
                    buffer[i] = output[i];
                double idx = scale->getNewPlaypos();

                // If a beat occours in current buffer mark it by led or in audio
                // This code currently only works in forward playback.
                ReaderExtractBeat *readerbeat = reader->getBeatPtr();
                if (readerbeat!=0)
                {
                    // Check if we need to set samples from a previos beat mark
                    if (audioBeatMark->get()==1. && m_iBeatMarkSamplesLeft>0)
                    {
                        int to = (int)min(m_iBeatMarkSamplesLeft, idx-bufferpos_play);
                        for (int j=0; j<to; j++)
                            buffer[j] = 30000.;
                        m_iBeatMarkSamplesLeft = max(0,m_iBeatMarkSamplesLeft-to);
                    }

                    float *beatBuffer = (float *)readerbeat->getBasePtr();
                    int chunkSizeDiff = READBUFFERSIZE/readerbeat->getBufferSize();
//                    qDebug("from %i-%i",(int)floor(bufferpos_play),(int)floor(idx));
					for (i=(int)floor(bufferpos_play); i<=(int)floor(idx); i++)
                    {
                        if (((i%chunkSizeDiff)==0) && (beatBuffer[i/chunkSizeDiff]>0.))
                        {
//                            qDebug("%i: %f",i/chunkSizeDiff,beatBuffer[i/chunkSizeDiff]);

							// Audio beat mark
                            if (audioBeatMark->get()==1.)
                            {
                                int from = (int)(i-bufferpos_play);
                                int to = (int)min(i-bufferpos_play+audioBeatMarkLen, idx-bufferpos_play);
                                for (int j=from; j<to; j++)
                                    buffer[j] = 30000.;
                                m_iBeatMarkSamplesLeft = max(0,audioBeatMarkLen-(to-from));

                            //    qDebug("mark %i: %i-%i", i2/chunkSizeDiff, (int)max(0,i-bufferpos_play),(int)min(i-bufferpos_play+audioBeatMarkLen, idx));
                            }
#ifdef __UNIX__
                            // PowerMate led
                            if (powermate!=0)
                                powermate->led();
#endif
                        }
                    }
                }

                // Write file playpos
                filepos_play += (idx-bufferpos_play); //rate*(double)(buf_size);

                // Ensure valid range of idx
                if (idx>READBUFFERSIZE)
                    idx -= (double)READBUFFERSIZE;
                else if (idx<0)
                    idx += (double)READBUFFERSIZE;

                // Write buffer playpos
                bufferpos_play = idx;
            }
        }

        //
        // Check if more samples are needed from reader, and wake it up if necessary.
        //
        if (readerinfo)
        {
//            qDebug("checking");
            if (!backwards && filepos_end<file_length_old && (filepos_end - filepos_play < READCHUNKSIZE*(READCHUNK_NO/2-1)))
            {
//                qDebug("wake fwd");
                reader->wake();
            }
            else if (backwards && filepos_start>0. && (filepos_play-filepos_start)<READCHUNKSIZE*(READCHUNK_NO/2-1))
            {
//                qDebug("wake back");
                reader->wake();
            }
            //
            // Check if end or start of file, and playmode, write new rate, playpos and do wakeall
            // if playmode is next file: set next in playlistcontrol
            //

            // Update playpos slider if necessary
            m_iSamplesCalculated += buf_size;
            if (m_iSamplesCalculated > (44100/UPDATE_RATE) )
            {
                if (file_length_old!=0.)
                    playposSlider->set(filepos_play/file_length_old);
                else
                    playposSlider->set(0.);
                m_iSamplesCalculated = 0;
            }

            // Update bufferposSlider
            bufferposSlider->set((CSAMPLE)bufferpos_play);
        }

        // If playbutton is pressed, check if we are at start or end of track
        if (playButton->get()==1. && m_pTrackEnd->get()==0. &&
            ((filepos_play<=0. && backwards==true) ||
            ((int)filepos_play>=file_length_old && backwards==false)))
        {
            // If end of track mode is set to next, signal EndOfTrack to TrackList,
            // otherwise start looping, pingpong or stop the track
            int m = (int)m_pTrackEndMode->get();
            switch (m)
            {
            case TRACK_END_MODE_STOP:
                playButton->set(0.);
                break;
            case TRACK_END_MODE_NEXT:
                m_pTrackEnd->set(1.);
                break;
            case TRACK_END_MODE_LOOP:
                slotControlSeek(0.);
                break;
/*
            case TRACK_END_MODE_PING:
                qDebug("Ping not implemented yet");

                if (reverseButton->get()==1.)
                    reverseButton->set(0.);
                else
                    reverseButton->set(1.);
                
                break;
*/
            default:
                qDebug("Invalid track end mode: %i",m);
            }
        }

        pause.unlock();

    }
    else
    {
        for (int i=0; i<buf_size; i++)
            buffer[i]=0.;
    }

    return buffer;
}

