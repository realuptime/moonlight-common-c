#include "ScreamWrapper.h"
#include "ScreamRx.h"
#include <cstdint>
#include <string.h> 
#include <cassert>
#include <sys/time.h>
#include <pthread.h>
#include <iostream>
#include "netinet/in.h"

#define ACK_DIFF 4
#define REPORTED_RTP_PACKETS 4
#define SSRC 1

using namespace std;

static pthread_mutex_t lock_scream;
double t0 = 0;

uint32_t getTimeInNtp(){
  struct timeval tp;
  gettimeofday(&tp, NULL);
  double time = tp.tv_sec + tp.tv_usec*1e-6-t0;
  uint64_t ntp64 = uint64_t(time*65536.0);
  uint32_t ntp = 0xFFFFFFFF & ntp64;
  return ntp;
}

void parseRtp(char* buf, uint16_t* seqNr, uint32_t* timeStamp) {
	uint16_t rawSeq;
	uint32_t rawTs;
	memcpy(&rawSeq, buf + 2, 2);
	memcpy(&rawTs, buf + 4, 4);
	*seqNr = ntohs(rawSeq);
	*timeStamp = ntohl(rawTs);
}

class ScreamRxProxy{
	
	public:
		ScreamRxProxy(){
			   struct timeval tp;
			   gettimeofday(&tp, NULL);
			   t0 = tp.tv_sec + tp.tv_usec*1e-6;
		};
		
		void receive(char* buf, int recvlen, unsigned char received_ecn, bool isMark){
			uint32_t time_ntp = getTimeInNtp();
			pthread_mutex_lock(&lock_scream);
			if (time_ntp - last_received_time_ntp > 2 * 65536) { // 2 sec
				receivedRtp = 0;
				delete screamRx;
				screamRx = new ScreamRx(SSRC, ACK_DIFF, REPORTED_RTP_PACKETS);
				assert(screamRx != 0);
				cerr << "Receiver state reset due to idle input" << endl;
			}
			last_received_time_ntp = time_ntp;
			receivedRtp++;
			
			uint16_t seqNr;
			uint32_t ts;
			parseRtp(buf, &seqNr, &ts);

			if ((seqNr - lastSn) > 1) {
				fprintf(stderr, "Packet(s) lost or reordered : %5d was received, previous rcvd is %5d \n", seqNr, lastSn);
			}
			lastSn = seqNr;
			
			screamRx->receive(getTimeInNtp(), 0, SSRC, recvlen, seqNr, received_ecn, isMark);
			pthread_mutex_unlock(&lock_scream);
	  
	  };
	  
	  bool getFeedback(unsigned char *buf, int *rtcpSize){
		  
		uint32_t time_ntp = getTimeInNtp();
		bool isFeedback = false;
	    pthread_mutex_lock(&lock_scream);
		uint32_t rtcpFbInterval_ntp = screamRx->getRtcpFbInterval();
		
		if (screamRx->isFeedback(time_ntp) && (screamRx->checkIfFlushAck() ||
			(time_ntp - screamRx->getLastFeedbackT() > rtcpFbInterval_ntp))) {
				isFeedback = screamRx->createStandardizedFeedback(getTimeInNtp(), true, buf, *rtcpSize);
			}
		pthread_mutex_unlock(&lock_scream);
		
		return isFeedback;
	  };
		
	private:
		ScreamRx* screamRx;
	    uint32_t last_received_time_ntp = 0;
	    uint32_t receivedRtp = 0;
		uint16_t lastSn = 0;
	
};

static ScreamRxProxy sx;

void screamReceive(char* buffer, int recvLength, unsigned char ecn, bool isMark){
	  sx.receive(buffer, recvLength, ecn, isMark);
};

bool screamGetFeedback(unsigned char *buf, int *size){
	return sx.getFeedback(buf, size);
};