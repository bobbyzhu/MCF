// 这个文件是 MCF 的一部分。
// 有关具体授权说明，请参阅 MCFLicense.txt。
// Copyleft 2014. LH_Mouse. All wrongs reserved.

#include "dll_decl.h"
#include "../ext/unref_param.h"

bool MCFDll_OnProcessAttach(bool bDynamic){
	UNREF_PARAM(bDynamic);

	return true;
}
void MCFDll_OnProcessDetach(bool bDynamic){
	UNREF_PARAM(bDynamic);
}
void MCFDll_OnThreadAttach(){
}
void MCFDll_OnThreadDetach(){
}