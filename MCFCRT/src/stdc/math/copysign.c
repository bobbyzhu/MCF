// 这个文件是 MCF 的一部分。
// 有关具体授权说明，请参阅 MCFLicense.txt。
// Copyleft 2013 - 2016, LH_Mouse. All wrongs reserved.

#include "../../env/_crtdef.h"
#include "_fpu.h"

#undef copysignf
#undef copysign
#undef copysignl

static inline long double fpu_copysign(long double x, long double y){
	long double ret = __MCFCRT_fabs(x);
	if(__MCFCRT_fgetsign(y)){
		ret = __MCFCRT_fneg(ret);
	}
	return ret;
}

float copysignf(float x, float y){
#ifdef _WIN64
	static const uint32_t mmask = 0x7FFFFFFFu;
	float ret;
	__asm__(
		"movss xmm2, dword ptr[%3] \n"
		"xorps xmm0, xmm1 \n"
		"andps xmm0, xmm2 \n"
		"xorps xmm0, xmm1 \n"
		: "=Yz"(ret)
		: "0"(x), "x"(y), "m"(mmask)
		: "xmm2"
	);
	return ret;
#else
	return (float)fpu_copysign(x, y);
#endif
}
double copysign(double x, double y){
#ifdef _WIN64
	static const uint64_t mmask = 0x7FFFFFFFFFFFFFFFu;
	double ret;
	__asm__(
		"movsd xmm2, qword ptr[%3] \n"
		"xorpd xmm0, xmm1 \n"
		"andpd xmm0, xmm2 \n"
		"xorpd xmm0, xmm1 \n"
		: "=Yz"(ret)
		: "0"(x), "x"(y), "m"(mmask)
		: "xmm2"
	);
	return ret;
#else
	return (double)fpu_copysign(x, y);
#endif
}
long double copysignl(long double x, long double y){
	return (long double)fpu_copysign(x, y);
}
