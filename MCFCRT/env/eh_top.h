// 这个文件是 MCF 的一部分。
// 有关具体授权说明，请参阅 MCFLicense.txt。
// Copyleft 2013 - 2015, LH_Mouse. All wrongs reserved.

#ifndef __MCF_CRT_EH_TOP_H_
#define __MCF_CRT_EH_TOP_H_

#include "_crtdef.h"

#define __MCF_HAS_EH_TOP	\
	__attribute__((__section__(".text$")))

__MCF_CRT_EXTERN_C_BEGIN

extern bool __MCF_CRT_RegisterFrameInfo(void) MCF_NOEXCEPT;
extern void __MCF_CRT_UnregisterFrameInfo(void) MCF_NOEXCEPT;

__MCF_CRT_EXTERN_C_END

#if defined(__SEH__)	// SEH

#	define __MCF_EH_TOP_BEGIN	\
	__asm__ __volatile__(	\
		"60000: \n"	\
	);
#	define __MCF_EH_TOP_END	\
	__asm__ __volatile__(	\
		"60001: \n"	\
		"	.seh_handler __C_specific_handler, @except \n"	\
		"	.seh_handlerdata \n"	\
		"	.long 1 \n"	\
		"	.rva 60000b, 60001b + 1, _gnu_exception_handler, 60001b + 1 \n"	\
		"	.section .text$ \n"	\
	);

#else	// DWARF, SJLJ

#	define __MCF_EH_TOP_BEGIN
#	define __MCF_EH_TOP_END

#endif

#endif