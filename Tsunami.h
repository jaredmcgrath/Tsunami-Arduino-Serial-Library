// **************************************************************
//     Filename: Tsunami.h
// Date Created: 11/22/2016
//
//     Comments: Robertsonics Tsunami serial control library
//
// Programmers: Jamie Robertson, info@robertsonics.com
//
// **************************************************************

#ifndef _20161015_TSUNAMI_H_
#define _20161015_TSUNAMI_H_

// ==================================================================
// The following defines are used to control which serial class is
//  used. Uncomment only the one you wish to use. If all of them are
//  commented out, the library will use Hardware Serial
#define __TSUNAMI_USE_SERIAL1__
//#define __TSUNAMI_USE_SERIAL2__
//#define __TSUNAMI_USE_SERIAL3__
//#define __TSUNAMI_USE_ALTSOFTSERIAL__
// ==================================================================

// ==================================================================
// If you are using the Arduino Due, uncomment the following line
#define __TSUNAMI_USE_DUE_IMPORT__
// ==================================================================

// ==================================================================
// If you are using the Tsunami in mono track mode, uncomment the following line
// Note: You must set the appropriate firmware settings in the config file on the 
// Tsunami's SD card for the device to use mono mode
#define __TSUNAMI_USE_MONO__
// ==================================================================

// ==================================================================
// To enable debug prints to Serial, uncomment the following line
#define __TSUNAMI_DEBUG_MODE__
// ==================================================================

#define CMD_GET_VERSION				1
#define CMD_GET_SYS_INFO			2
#define CMD_TRACK_CONTROL			3
#define CMD_STOP_ALL				4
#define CMD_MASTER_VOLUME			5
#define CMD_TRACK_VOLUME			8
#define CMD_TRACK_FADE				10
#define CMD_RESUME_ALL_SYNC			11
#define CMD_SAMPLERATE_OFFSET		12
#define	CMD_SET_REPORTING			13
#define CMD_SET_TRIGGER_BANK		14
#define CMD_SET_INPUT_MIX			15
#define CMD_SET_MIDI_BANK			16

#define TRK_PLAY_SOLO				0
#define TRK_PLAY_POLY				1
#define TRK_PAUSE					2
#define TRK_RESUME					3
#define TRK_STOP					4
#define TRK_LOOP_ON					5
#define TRK_LOOP_OFF				6
#define TRK_LOAD					7

#define	RSP_VERSION_STRING			129
#define	RSP_SYSTEM_INFO				130
#define	RSP_STATUS					131
#define	RSP_TRACK_REPORT			132

#ifdef __TSUNAMI_USE_MONO__
#define MAX_NUM_VOICES				32
#else
#define MAX_NUM_VOICES				18
#endif
#define MAX_MESSAGE_LEN				32
#define VERSION_STRING_LEN			23
#define TSUNAMI_NUM_OUTPUTS			8

#define SOM1	0xf0
#define SOM2	0xaa
#define EOM		0x55

#define IMIX_OUT1	0x01
#define IMIX_OUT2	0x02
#define IMIX_OUT3	0x04
#define IMIX_OUT4	0x08

#ifdef __TSUNAMI_USE_ALTSOFTSERIAL__
#include "../AltSoftSerial/AltSoftSerial.h"
#else
#ifdef __TSUNAMI_USE_DUE_IMPORT__
#include <Arduino.h>
#else
#include <HardwareSerial.h>
#endif
#ifdef __TSUNAMI_USE_SERIAL1__
#define TsunamiSerial Serial1
#define __TSUNAMI_SERIAL_ASSIGNED__
#endif
#ifdef __TSUNAMI_USE_SERIAL2__
#define TsunamiSerial Serial2
#define __TSUNAMI_SERIAL_ASSIGNED__
#endif
#ifdef __TSUNAMI_USE_SERIAL3__
#define TsunamiSerial Serial3
#define __TSUNAMI_SERIAL_ASSIGNED__
#endif
#ifndef __TSUNAMI_SERIAL_ASSIGNED__
#define TsunamiSerial Serial
#endif
#endif

class Tsunami
{
public:
	Tsunami() {;}
	~Tsunami() {;}
	void start(void);
	void update(void);
	void flush(void);
	void setReporting(bool enable);
	bool getVersion(char *pDst, int len);
	int getNumTracks(void);
	bool isTrackPlaying(int trk);
	void masterGain(int out, int gain);
	void stopAllTracks(void);
	void resumeAllInSync(void);
	void trackPlaySolo(int trk, int out, bool lock);
	void trackPlayPoly(int trk, int out, bool lock);
	void trackLoad(int trk, int out, bool lock);
	void trackStop(int trk);
	void trackPause(int trk);
	void trackResume(int trk);
	void trackLoop(int trk, bool enable);
	void trackGain(int trk, int gain);
	void trackFade(int trk, int gain, int time, bool stopFlag);
	void samplerateOffset(int out, int offset);
	void setTriggerBank(int bank);
	void setInputMix(int mix);
	void setMidiBank(int bank);

private:
	void trackControl(int trk, int code, int out, int flags);

#ifdef __TSUNAMI_USE_ALTSOFTSERIAL__
	AltSoftSerial TsunamiSerial;
#endif

	// State variables
	// Voice table: array of track numbers (numbered 1-4096)
	// Each index represents a single voice, which is either a mono (0-31) or stereo (0-17) track depending on config
	uint16_t voiceTable[MAX_NUM_VOICES];
	// A buffer for the last received rx message payload
	uint8_t rxMessage[MAX_MESSAGE_LEN];
	// String containing the version string, which is set by Tsunami upon initialization
	char version[VERSION_STRING_LEN];
	// Short containing # of tracks, which is set by Tsunami upon initialization
	uint16_t numTracks;
	// Byte containing # of voices, which is set by Tsunami upon initialization
	uint8_t numVoices;
	// Counter to keep track of current byte index when decoding rx message from serial
	uint8_t rxCount;
	// Variable that contains the message length indicated in byte 2 of every rx message
	uint8_t rxLen;
	// Bool indicating that there is a valid message ready to be received
	bool rxMsgReady;
	// Bool indicating that a valid version string has been received, located in version
	bool versionRcvd;
	// Bool indicating that valid system info has been received in numTracks and numVoices
	bool sysinfoRcvd;
};

#endif
