// 这个文件是 MCF 的一部分。
// 有关具体授权说明，请参阅 MCFLicense.txt。
// Copyleft 2013 - 2016, LH_Mouse. All wrongs reserved.

#ifndef __MCFCRT_EXT_DIV64_H_
#define __MCFCRT_EXT_DIV64_H_

#include "../env/_crtdef.h"

_MCFCRT_EXTERN_C_BEGIN

static inline _MCFCRT_STD int32_t __MCFCRT_idiv64_nonconstexpr(_MCFCRT_STD int32_t *restrict __rem_ret, _MCFCRT_STD int64_t __a, _MCFCRT_STD int32_t __b) _MCFCRT_NOEXCEPT {
	_MCFCRT_STD int32_t __quo, __rem;
	__asm__(
#ifdef _WIN64
		"cqo \n"
		"idiv rcx \n"
		: "=a"(__quo), "=d"(__rem)
		: "a"(__a), "c"((_MCFCRT_STD int64_t)__b)
#else
		"idiv ecx \n"
		: "=a"(__quo), "=d"(__rem)
		: "A"(__a), "c"(__b)
#endif
	);
	if(__rem_ret){
		*__rem_ret = __rem;
	}
	return __quo;
}
static inline _MCFCRT_STD uint32_t __MCFCRT_udiv64_nonconstexpr(_MCFCRT_STD uint32_t *restrict __rem_ret, _MCFCRT_STD uint64_t __a, _MCFCRT_STD uint32_t __b) _MCFCRT_NOEXCEPT {
	_MCFCRT_STD uint32_t __quo, __rem;
	__asm__(
#ifdef _WIN64
		"xor rdx, rdx \n"
		"div rcx \n"
		: "=a"(__quo), "=d"(__rem)
		: "a"(__a), "c"((_MCFCRT_STD int64_t)__b)
#else
		"div ecx \n"
		: "=a"(__quo), "=d"(__rem)
		: "A"(__a), "c"(__b)
#endif
	);
	if(__rem_ret){
		*__rem_ret = __rem;
	}
	return __quo;
}

_MCFCRT_CONSTEXPR _MCFCRT_STD int32_t _MCFCRT_idiv64(_MCFCRT_STD int64_t __a, _MCFCRT_STD int32_t __b) _MCFCRT_NOEXCEPT {
	return __builtin_constant_p(__a / __b) ? (__a / (_MCFCRT_STD int64_t)__b)
	                                       : __MCFCRT_idiv64_nonconstexpr(nullptr, __a, __b);
}
_MCFCRT_CONSTEXPR _MCFCRT_STD uint32_t _MCFCRT_udiv64(_MCFCRT_STD uint64_t __a, _MCFCRT_STD uint32_t __b) _MCFCRT_NOEXCEPT {
	return __builtin_constant_p(__a / __b) ? (__a / (_MCFCRT_STD uint64_t)__b)
	                                       : __MCFCRT_udiv64_nonconstexpr(nullptr, __a, __b);
}
static inline _MCFCRT_STD int32_t _MCFCRT_idivrem64(_MCFCRT_STD int32_t *restrict __rem_ret, _MCFCRT_STD int64_t __a, _MCFCRT_STD int32_t __b) _MCFCRT_NOEXCEPT {
	return __builtin_constant_p(__a / __b) ? ((__rem_ret ? (*__rem_ret = __a % (_MCFCRT_STD int64_t)__b) : 0), (__a / (_MCFCRT_STD int64_t)__b))
	                                       : __MCFCRT_idiv64_nonconstexpr(__rem_ret, __a, __b);
}
static inline _MCFCRT_STD uint32_t _MCFCRT_udivrem64(_MCFCRT_STD uint32_t *restrict __rem_ret, _MCFCRT_STD uint64_t __a, _MCFCRT_STD uint32_t __b) _MCFCRT_NOEXCEPT {
	return __builtin_constant_p(__a / __b) ? ((__rem_ret ? (*__rem_ret = __a % (_MCFCRT_STD uint64_t)__b) : 0), (__a / (_MCFCRT_STD uint64_t)__b))
	                                       : __MCFCRT_udiv64_nonconstexpr(__rem_ret, __a, __b);
}

_MCFCRT_EXTERN_C_END

#endif
