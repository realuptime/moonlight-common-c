#include "ScreamWrapper.h"
#include "ScreamRx.h"
#include <cstdint>
#include <string.h> 
#include <cassert>
#include <sys/time.h>
#include <mutex>
#include <iostream>
#include "netinet/in.h"

#define U16(x) ((unsigned short) ((x) & UINT16_MAX))
#define isBefore16(x, y) (U16((x) - (y)) > (UINT16_MAX/2))

#define ACK_DIFF -1
#define REPORTED_RTP_PACKETS 64
#define SSRC 1

using namespace std;

namespace
{

std::mutex lock_scream;
double t0 = 0;

uint32_t getTimeInNtp()
{
  struct timeval tp;
  gettimeofday(&tp, NULL);
  double time = tp.tv_sec + tp.tv_usec*1e-6 - t0;
  uint64_t ntp64 = uint64_t(time*65536.0);
  uint32_t ntp = 0xFFFFFFFF & ntp64;
  return ntp;
}

class ScreamRxProxy
{
	public:
	ScreamRxProxy()
    {
	       screamRx = new ScreamRx(SSRC, ACK_DIFF, REPORTED_RTP_PACKETS);
	}
		
	bool receive(uint16_t seqNr, uint32_t ts, char* buf, int size, unsigned char received_ecn, bool isMark)
    {
            std::lock_guard<std::mutex> lock { lock_scream };

            bool ret = true;

            if (t0 == 0.0f)
            {
                struct timeval tp;
			    gettimeofday(&tp, NULL);
			    t0 = tp.tv_sec + tp.tv_usec * 1e-6;
            }
			
            uint32_t time_ntp = getTimeInNtp();
#if 1
			if (time_ntp - last_received_time_ntp > 2 * 65536)
            { // 2 sec
				receivedRtp = 0;
				delete screamRx;
				screamRx = new ScreamRx(SSRC, ACK_DIFF, REPORTED_RTP_PACKETS);
				assert(screamRx != 0);
				printf("SCREAM: Receiver state reset due to idle input. ScreamTx recreated!\n");
			}
#endif
			last_received_time_ntp = time_ntp;
			receivedRtp++;
			
			ts = time_ntp; // test

			if (lastSn && seqNr != U16(lastSn + 1))
            {
				printf("Packet(s) lost or reordered : %5d was received, previous rcvd is %5d. diff:%d\n", seqNr, lastSn, int(seqNr) - int(lastSn));
                ret = false;
			}

			lastSn = seqNr;
			
			screamRx->receive(ts, 0, SSRC, size, seqNr, received_ecn, isMark);

            return ret;
	  }
	  
	  bool getFeedback(unsigned char *buf, int *rtcpSize)
      {
        std::lock_guard<std::mutex> lock { lock_scream };

        if (t0 == 0.0f) return false;

        uint32_t time_ntp = getTimeInNtp();
        bool isFeedback = false;
        uint32_t rtcpFbInterval_ntp = screamRx->getRtcpFbInterval();

        if (screamRx->isFeedback(time_ntp) &&
                (screamRx->checkIfFlushAck() ||
                (time_ntp - screamRx->getLastFeedbackT() > rtcpFbInterval_ntp)))
        {
            isFeedback = screamRx->createStandardizedFeedback(time_ntp, true, buf, *rtcpSize);
        }

        if (isFeedback)
        {
#if 0
                std::string str;
                for (int i = 0; i < std::min(*rtcpSize, 15); ++i)
                    str += std::to_string(int(buf[i])) + ' ';
                printf("SCREAM: RTCP: '%s'\n", str.c_str());
#endif
        }

        return isFeedback;
	  }
		
	private:
		ScreamRx* screamRx = nullptr;
	    uint32_t last_received_time_ntp = 0;
	    uint32_t receivedRtp = 0;
		uint16_t lastSn = 0;
};

ScreamRxProxy sx;
} // namspace

bool screamReceive(uint16_t seqNr, uint32_t ts, char* buffer, int size, unsigned char ecn, bool isMark)
{
    return sx.receive(seqNr, ts, buffer, size, ecn, isMark);
};

bool screamGetFeedback(unsigned char *buf, int *size)
{
	return sx.getFeedback(buf, size);
};

