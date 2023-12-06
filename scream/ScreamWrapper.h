#ifdef __cplusplus
#pragma once

extern "C"
{
#endif

// false if packet loss
bool screamReceive(unsigned short seqNr, unsigned int ts, char *buf, int size, unsigned char ecn, bool isMark);

bool screamGetFeedback(unsigned char *buf, int *size);

#ifdef __cplusplus
}
#endif
