// 这个文件是 MCF 的一部分。
// 有关具体授权说明，请参阅 MCFLicense.txt。
// Copyleft 2013 - 2016, LH_Mouse. All wrongs reserved.

#include "../env/_crtdef.h"
#include "../env/crt_module.h"

int at_quick_exit(void (*func)(void)){
	if(!_MCFCRT_AtCrtModuleQuickExit((__attribute__((__cdecl__)) void (*)(intptr_t))func, 0)){
		return -1;
	}
	return 0;
}
