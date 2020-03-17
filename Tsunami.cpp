// **************************************************************
//     Filename: Tsunami.cpp
// Date Created: 11/22/2016
//
//     Comments: Robertsonics Tsunami serial control library
//
// Programmers: Jamie Robertson, info@robertsonics.com
//
// **************************************************************

#include "Tsunami.h"


// **************************************************************
// Starts the serial communication between Arduino and Tsunami (@ 57600 baud)
// Then flushes the serial port
// Then Requests version string
// Then requests system info
void Tsunami::start(void) {

uint8_t txbuf[5];

	trackReportCallback = NULL;
	versionRcvd = false;
	sysinfoRcvd = false;
  	TsunamiSerial.begin(57600);
	flush();

	// Request version string
	txbuf[0] = SOM1;
	txbuf[1] = SOM2;
	txbuf[2] = 0x05;
	txbuf[3] = CMD_GET_VERSION;
	txbuf[4] = EOM;
	TsunamiSerial.write(txbuf, 5);

	// Request system info
	txbuf[0] = SOM1;
	txbuf[1] = SOM2;
	txbuf[2] = 0x05;
	txbuf[3] = CMD_GET_SYS_INFO;
	txbuf[4] = EOM;
	TsunamiSerial.write(txbuf, 5);
}

// **************************************************************
// Flush: Resets rxCount, rxLen, and rxMsgReady, then writes all 0xFFFF to the voice table
// then reads from the serial while there are bytes available to clear it
void Tsunami::flush(void) {

int i;

	rxCount = 0;
	rxLen = 0;
	rxMsgReady = false;
	for (i = 0; i < MAX_NUM_VOICES; i++) {
		voiceTable[i] = 0xffff;
	}
	while(TsunamiSerial.available())
		i = TsunamiSerial.read();
}


// **************************************************************
// Update function: Call this regularly to ensure messages are read and received
// and callbacks are triggered
void Tsunami::update(void) {

int i;
uint8_t dat;
uint8_t voice;
uint16_t track;

	rxMsgReady = false;
	while (TsunamiSerial.available() > 0) {
		// dat is always our most recent data byte
		dat = TsunamiSerial.read();
		// Byte 0 should be SOM1
		if ((rxCount == 0) && (dat == SOM1)) {
			rxCount++;
		}
		// Byte 1 should be SOM2
		else if (rxCount == 1) {
			// If byte 1 is SOM2, keep going
			if (dat == SOM2)
				rxCount++;
			// Otherwise, bad message
			else {
				rxCount = 0;
				#ifdef __TSUNAMI_DEBUG_MODE__
				Serial.print(F("Bad msg 1\n"));
				#endif
			}
		}
		// Byte 2 should be message length
		else if (rxCount == 2) {
			// If the message length is less than the max message length
			if (dat <= MAX_MESSAGE_LEN) {
				rxCount++;
				// Set our length
				rxLen = dat - 1;
			}
			else {
				rxCount = 0;
				#ifdef __TSUNAMI_DEBUG_MODE__
				Serial.print(F("Bad msg 2\n"));
				#endif
			}
		}
		// Everything past byte 2 but less than expected message length is the rx payload
		else if ((rxCount > 2) && (rxCount < rxLen)) {
			// Store payload in rxMessage
			rxMessage[rxCount - 3] = dat;
			rxCount++;
		}
		// If we're at the expected message length
		else if (rxCount == rxLen) {
			// If the last byte is the EOM byte, the message is valid
			if (dat == EOM)
			// This is a good place to put a messageReceived callback
				rxMsgReady = true;
			else {
				rxCount = 0;
				#ifdef __TSUNAMI_DEBUG_MODE__
				Serial.print(F("Bad msg 3\n"));
				#endif
			}
		}
		// Any other pattern of bytes indicates a bad message, reset and set rxCount to 0
		else {
			rxCount = 0;
			#ifdef __TSUNAMI_DEBUG_MODE__
			Serial.print(F("Bad msg 4\n"));
			#endif
		}
		// If we have a valid message
		if (rxMsgReady) {
			// Byte 0 in the payload indicates the rx message type
			switch (rxMessage[0]) {
				// Track report: sent every time a track starts or stops. Good place for a callback
				// This is where the voice table is updated
				case RSP_TRACK_REPORT:
					track = rxMessage[2];
					track = (track << 8) + rxMessage[1] + 1;
					// Voice is the index within voice table
					voice = rxMessage[3];
					if (voice < MAX_NUM_VOICES) {
						if (rxMessage[4] == 0) {
							if (track == voiceTable[voice])
								voiceTable[voice] = 0xffff;
						}
						else
							voiceTable[voice] = track;
					}
					// Call the track report callback, if one has been specified
					if (trackReportCallback) {
						trackReportCallback(track, voice, rxMessage[4]);
					}
					#ifdef __TSUNAMI_DEBUG_MODE__
					Serial.print("Track ");
					Serial.print(track);
					if (rxMessage[4] == 0)
						Serial.print(" off\n");
					else
						Serial.print(" on\n");
					#endif
				break;
				// Version string: Sent to the Arduino at somepoint after initialization
				case RSP_VERSION_STRING:
					// Copy version string from payload
					for (i = 0; i < (VERSION_STRING_LEN - 1); i++)
						version[i] = rxMessage[i + 1];
					// zero-terminated char array to make it a string
					version[VERSION_STRING_LEN - 1] = 0;
					// Mark version received
					versionRcvd = true;
					#ifdef __TSUNAMI_DEBUG_MODE__
					Serial.write(version);
					Serial.write("\n");
					#endif
				break;
				// System info: Indicates max number of voices supported and number of tracks found on SD card
				case RSP_SYSTEM_INFO:
					numVoices = rxMessage[1];
					numTracks = rxMessage[3];
					numTracks = (numTracks << 8) + rxMessage[2];
					sysinfoRcvd = true;
					#ifdef __TSUNAMI_DEBUG_MODE__
					Serial.print("Sys info received\n");
					#endif
				break;

			}
			// Reset rx payload state after this message has been received
			rxCount = 0;
			rxLen = 0;
			rxMsgReady = false;

		} // if (rxMsgReady)

	} // while (TsunamiSerial.available() > 0)
}

// **************************************************************
// Called when a TRACK_REPORT response is received from the Tsunami
// Indicates that track on voice has changed state. If didStart
// is true, the track started playing. Otherwisem it has stopped playing
void Tsunami::setTrackReportCallback(void (*pFunc)(uint16_t track, uint8_t voice, bool didStart)) {
	trackReportCallback = pFunc;
}

// **************************************************************
// Returns the channel on which the track number is playing, 
// or -1 if the track is not playing on any channels
int Tsunami::isTrackPlaying(int trk) {

	update();
	for (int i = 0; i < MAX_NUM_VOICES; i++) {
		if (voiceTable[i] == trk)
			return i;
	}
	return -1;
}

// **************************************************************
// Set master gain on channel out to gain
void Tsunami::masterGain(int out, int gain) {

uint8_t txbuf[8];
unsigned short vol;
uint8_t o;

	// Truncate output channel to proper range (4 stereo or 8 mono channels)
	o = out & 0x07;
	txbuf[0] = SOM1;
	txbuf[1] = SOM2;
	txbuf[2] = 0x08;
	txbuf[3] = CMD_MASTER_VOLUME;
	txbuf[4] = o;
	// Truncate gain to 16 bit short
	vol = (unsigned short)gain;
	// Byte 5 is gain LSB
	txbuf[5] = (uint8_t)vol;
	// Byte 6 is gain MSB
	txbuf[6] = (uint8_t)(vol >> 8);
	txbuf[7] = EOM;
	TsunamiSerial.write(txbuf, 8);
}

// **************************************************************
// Enables reporting on tracks from Tsunami -> Arduino
// Should be called to enable callbacks
void Tsunami::setReporting(bool enable) {

uint8_t txbuf[6];

	txbuf[0] = SOM1;
	txbuf[1] = SOM2;
	txbuf[2] = 0x06;
	txbuf[3] = CMD_SET_REPORTING;
	txbuf[4] = enable;
	txbuf[5] = EOM;
	TsunamiSerial.write(txbuf, 6);
}

// **************************************************************
// Writes the version string to the provided pointer, up to the given length
// If the write can be performed, returns true. On failure (i.e. if 
// no version string has been received from Tsunami), returns false
bool Tsunami::getVersion(char *pDst, int len) {

int i;

	update();
	if (!versionRcvd) {
		return false;
	}
	for (i = 0; i < (VERSION_STRING_LEN - 1); i++) {
		if (i >= (len - 1))
			break;
		pDst[i] = version[i];
	}
	pDst[++i] = 0;
	return true;
}

// **************************************************************
// Returns the number of tracks found on the SD Card. Must have 
// enabled reporting to get a correct value. Returns -1 if the
// Tsunami hasn't provided a number
int Tsunami::getNumTracks(void) {

	update();
	return numTracks;
}


// **************************************************************
// Stops any and all tracks currently playing (on all outputs?) 
// and starts playing track number trk (1-4096) from the beginning
// Track is routed to specified stereo (mono or stereo) output
//
// - Does it stop all tracks on all outputs, or just the specified one?
void Tsunami::trackPlaySolo(int trk, int out, bool lock) {

int flags = 0;

	if (lock)
	  flags |= 0x01;
	trackControl(trk, TRK_PLAY_SOLO, out, flags);
}

// **************************************************************
// Starts track trk (1-4096) from the beginning, blending it with
// any other tracks currently playing, including
// another copy of the same track if already playing. Track is 
// routed to the specified (mono or stereo) output
//
// - Blends with all tracks on the specified output?
// - What if we're at the max number of voices?
void Tsunami::trackPlayPoly(int trk, int out, bool lock) {
  
int flags = 0;

	if (lock)
	  flags |= 0x01;
	trackControl(trk, TRK_PLAY_POLY, out, flags);
}

// **************************************************************
// Loads track trk (1-4096) and pauses it at the beginning. Can be
// used for multiple tracks and have them all unpaused at same time
// with resumeAllInSync(). Track is routed to specified (mono or
// stereo) output
void Tsunami::trackLoad(int trk, int out, bool lock) {
  
int flags = 0;

	if (lock)
	  flags |= 0x01;
	trackControl(trk, TRK_LOAD, out, flags);
}

// **************************************************************
// Stops track trk (1-4096) if its currently playing. No effect if
// trk is not playing. Does not affect other tracks
void Tsunami::trackStop(int trk) {

	trackControl(trk, TRK_STOP, 0, 0);
}

// **************************************************************
// Pauses track trk (1-4096) if it's currently playing. Does nothing if
// trk is not playing. Note that a paused track is still using 1 of
// the (32 mono or 18 stereo) voice slots. A voice allocated to a
// track is freed only when sound is stopped or track reaches end
// of file and is not set to loop
void Tsunami::trackPause(int trk) {

	trackControl(trk, TRK_PAUSE, 0, 0);
}

// **************************************************************
// Resumes track trk (1-4096) if it's currently paused. Does nothing
// if trk is not currently paused
void Tsunami::trackResume(int trk) {

	trackControl(trk, TRK_RESUME, 0, 0);
}

// **************************************************************
// Enables or disables track trk (1-4096) to loop. Does not start
// the track; only determines behaviour once end of track is reached.
// If enabled, track will loop until manually stopped, in which
// case the track is still set to loop. If the loop is disabled
// and the track is currently playing, it will play until it
// reaches the end of the file. Can be used before or when the
// track is playing
void Tsunami::trackLoop(int trk, bool enable) {
 
	if (enable)
	trackControl(trk, TRK_LOOP_ON, 0, 0);
	else
	trackControl(trk, TRK_LOOP_OFF, 0, 0);
}

// **************************************************************
// Private internal function that handles all CMD_TRACK_CONTROL commands
void Tsunami::trackControl(int trk, int code, int out, int flags) {
  
uint8_t txbuf[10];
uint8_t o;

	o = out & 0x07;
	txbuf[0] = SOM1;
	txbuf[1] = SOM2;
	txbuf[2] = 0x0a;
	txbuf[3] = CMD_TRACK_CONTROL;
	txbuf[4] = (uint8_t)code;
	txbuf[5] = (uint8_t)trk;
	txbuf[6] = (uint8_t)(trk >> 8);
	txbuf[7] = (uint8_t)o;
	txbuf[8] = (uint8_t)flags;
	txbuf[9] = EOM;
	TsunamiSerial.write(txbuf, 10);
}

// **************************************************************
// Immediatly stops playing all tracks
void Tsunami::stopAllTracks(void) {

uint8_t txbuf[5];

	txbuf[0] = SOM1;
	txbuf[1] = SOM2;
	txbuf[2] = 0x05;
	txbuf[3] = CMD_STOP_ALL;
	txbuf[4] = EOM;
	TsunamiSerial.write(txbuf, 5);
}

// **************************************************************
// Resumes all paused tracks in sync
void Tsunami::resumeAllInSync(void) {

uint8_t txbuf[5];

	txbuf[0] = SOM1;
	txbuf[1] = SOM2;
	txbuf[2] = 0x05;
	txbuf[3] = CMD_RESUME_ALL_SYNC;
	txbuf[4] = EOM;
	TsunamiSerial.write(txbuf, 5);
}

// **************************************************************
// Set the gain of the specified track. Gain values are in dB and 
// range from -70 to +10.
//
// This adjusts the gain of an individual track prior to mixing to 
// the output. If you are playing many tracks to one output, you may
// need to reduce the gain of each track to avoid clipping in 
// the output mix buffer.
//
// The change in gain is applied smoothly over one audio buffer 
// (about 3ms) to avoid distortion, but is otherwise immediate. 
// If you want to smoothly ramp the volume up or down over time, 
// use the TRACK_FADE command instead.
void Tsunami::trackGain(int trk, int gain) {

uint8_t txbuf[9];
unsigned short vol;

	txbuf[0] = SOM1;
	txbuf[1] = SOM2;
	txbuf[2] = 0x09;
	txbuf[3] = CMD_TRACK_VOLUME;
	txbuf[4] = (uint8_t)trk;
	txbuf[5] = (uint8_t)(trk >> 8);
	vol = (unsigned short)gain;
	txbuf[6] = (uint8_t)vol;
	txbuf[7] = (uint8_t)(vol >> 8);
	txbuf[8] = EOM;
	TsunamiSerial.write(txbuf, 9);
}

// **************************************************************
// Fade (up or down) a track to a target gain from the current level
// in the specified number of milliseconds, with an option to stop 
// the track at the end of the fade.
//
// Target gain is specified in dB and range is from -70 to +10. 
// If Stop Flag = 1, the track will stop at the end of the fade. 
// This is most useful for implementing a fade out, since Tsunami 
// will automatically stop the track and release the voice at the end of the fade out.
void Tsunami::trackFade(int trk, int gain, int time, bool stopFlag) {

uint8_t txbuf[12];
unsigned short vol;

	txbuf[0] = SOM1;
	txbuf[1] = SOM2;
	txbuf[2] = 0x0c;
	txbuf[3] = CMD_TRACK_FADE;
	txbuf[4] = (uint8_t)trk;
	txbuf[5] = (uint8_t)(trk >> 8);
	vol = (unsigned short)gain;
	txbuf[6] = (uint8_t)vol;
	txbuf[7] = (uint8_t)(vol >> 8);
	txbuf[8] = (uint8_t)time;
	txbuf[9] = (uint8_t)(time >> 8);
	txbuf[10] = stopFlag;
	txbuf[11] = EOM;
	TsunamiSerial.write(txbuf, 12);
}

// **************************************************************
// Set the sample rate offset for the specified output.
//
// Each of Tsunami’s outputs can support it’s own vari-speed control.
// The offset value ranges from -32768, corresponding to 1/2X speed 
// (one octave pitch shift down), to +32767, corresponding to 2X 
// speed (one octave pitch shift up.)
void Tsunami::samplerateOffset(int out, int offset) {

uint8_t txbuf[8];
unsigned short off;
uint8_t o;

	o = out & 0x07;
	txbuf[0] = SOM1;
	txbuf[1] = SOM2;
	txbuf[2] = 0x08;
	txbuf[3] = CMD_SAMPLERATE_OFFSET;
	txbuf[4] = (uint8_t)o;
	off = (unsigned short)offset;
	txbuf[5] = (uint8_t)off;
	txbuf[6] = (uint8_t)(off >> 8);
	txbuf[7] = EOM;
	TsunamiSerial.write(txbuf, 8);
}

// **************************************************************
// Set the trigger bank
void Tsunami::setTriggerBank(int bank) {

	uint8_t txbuf[6];

	txbuf[0] = SOM1;
	txbuf[1] = SOM2;
	txbuf[2] = 0x06;
	txbuf[3] = CMD_SET_TRIGGER_BANK;
	txbuf[4] = (uint8_t)bank;
	txbuf[5] = EOM;
	TsunamiSerial.write(txbuf, 6);
}

// **************************************************************
// Set the input routing mix.
// 
// The lowest 4 bits of the mask correspond to the 4 output pairs.
// A “1” in the position will enable the audio input to be mixed
// to the corresponding output pair.
void Tsunami::setInputMix(int mix) {

	uint8_t txbuf[6];

	txbuf[0] = SOM1;
	txbuf[1] = SOM2;
	txbuf[2] = 0x06;
	txbuf[3] = CMD_SET_INPUT_MIX;
	txbuf[4] = (uint8_t)mix;
	txbuf[5] = EOM;
	TsunamiSerial.write(txbuf, 6);
}

// **************************************************************
// Set MIDI bank
void Tsunami::setMidiBank(int bank) {

	uint8_t txbuf[6];

	txbuf[0] = SOM1;
	txbuf[1] = SOM2;
	txbuf[2] = 0x06;
	txbuf[3] = CMD_SET_MIDI_BANK;
	txbuf[4] = (uint8_t)bank;
	txbuf[5] = EOM;
	TsunamiSerial.write(txbuf, 6);
}



