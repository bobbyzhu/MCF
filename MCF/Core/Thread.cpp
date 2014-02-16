// 这个文件是 MCF 的一部分。
// 有关具体授权说明，请参阅 MCFLicense.txt。
// Copyleft 2014. LH_Mouse. All wrongs reserved.

#include "../StdMCF.hpp"
#include "Thread.hpp"
#include "Exception.hpp"
#include "UniqueHandle.hpp"
#include <exception>
using namespace MCF;

// 嵌套类定义。
class Thread::xDelegate : NO_COPY {
private:
	struct xThreadCloser {
		constexpr HANDLE operator()() const {
			return NULL;
		}
		void operator()(HANDLE hThread) const {
			::CloseHandle(hThread);
		}
	};
private:
	static unsigned int xThreadProc(std::intptr_t nParam) noexcept {
		auto *const pThis = (xDelegate *)nParam;
		try {
			const std::shared_ptr<xDelegate> pInstance(std::move(pThis->xm_pLock));
			pThis->xm_pLock.reset(); // 打破循环引用。
			pThis->xm_fnProc();
		} catch(...){
			pThis->xm_pException = std::current_exception();
		}
		pThis->xm_ulThreadId = 0;
		return 0;
	}
public:
	static std::shared_ptr<xDelegate> Create(std::function<void()> &&fnProc){
		std::shared_ptr<xDelegate> pRet(new xDelegate(std::move(fnProc)));
		pRet->xm_hThread.Reset(::__MCF_CreateCRTThread(&xThreadProc, (std::intptr_t)pRet.get(), CREATE_SUSPENDED, &pRet->xm_ulThreadId));
		if(!pRet->xm_hThread){
			MCF_THROW(::GetLastError(), L"__MCF_CreateCRTThread() 失败。");
		}
		pRet->xm_pLock = pRet; // 制造循环引用。这样代理对象就不会被删掉。
		return std::move(pRet);
	}
private:
	std::shared_ptr<xDelegate> xm_pLock;
	std::function<void()> xm_fnProc;
	UniqueHandle<HANDLE, xThreadCloser> xm_hThread;
	unsigned long xm_ulThreadId;
	std::exception_ptr xm_pException;
private:
	explicit xDelegate(std::function<void()> &&fnProc) noexcept
		: xm_fnProc(std::move(fnProc))
	{
	}
public:
	HANDLE GetHandle() const noexcept {
		return xm_hThread;
	}
	unsigned long GetId() const noexcept {
		return xm_ulThreadId;
	}

	void CheckThrow() const {
		if(xm_pException){
			std::rethrow_exception(xm_pException);
		}
	}
};

// 构造函数和析构函数。
Thread::Thread(){
}
Thread::~Thread(){
	JoinDetach();
}

// 其他非静态成员函数。
void Thread::Start(std::function<void()> fnProc, bool bSuspended){
	xm_pDelegate = xDelegate::Create(std::move(fnProc));
	if(!bSuspended){
		::ResumeThread(xm_pDelegate->GetHandle());
	}
}
void Thread::WaitTimeout(unsigned long ulMilliSeconds) const noexcept {
	if(xm_pDelegate){
		::WaitForSingleObject(xm_pDelegate->GetHandle(), ulMilliSeconds);
	}
}
void Thread::Join() const {
	if(xm_pDelegate){
		::WaitForSingleObject(xm_pDelegate->GetHandle(), INFINITE);
		xm_pDelegate->CheckThrow();
	}
}
void Thread::Detach() noexcept {
	if(xm_pDelegate){
		::ResumeThread(xm_pDelegate->GetHandle());
		xm_pDelegate.reset();
	}
}
void Thread::JoinDetach() noexcept {
	if(xm_pDelegate){
		::ResumeThread(xm_pDelegate->GetHandle());
		::WaitForSingleObject(xm_pDelegate->GetHandle(), INFINITE);
		xm_pDelegate.reset();
	}
}

void Thread::Suspend() noexcept {
	if(xm_pDelegate){
		::SuspendThread(xm_pDelegate->GetHandle());
	}
}
void Thread::Resume() noexcept {
	if(xm_pDelegate){
		::ResumeThread(xm_pDelegate->GetHandle());
	}
}

bool Thread::IsAlive() const noexcept {
	return GetThreadId() != 0;
}
unsigned long Thread::GetThreadId() const noexcept {
	if(xm_pDelegate){
		return xm_pDelegate->GetId();
	}
	return 0;
}
