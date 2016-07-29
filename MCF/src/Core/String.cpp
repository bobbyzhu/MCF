// 这个文件是 MCF 的一部分。
// 有关具体授权说明，请参阅 MCFLicense.txt。
// Copyleft 2013 - 2016, LH_Mouse. All wrongs reserved.

#include "../StdMCF.hpp"
#include "String.hpp"
#include "Exception.hpp"
#include <winternl.h>
#include <ntstatus.h>

extern "C" {

__attribute__((__dllimport__, __stdcall__))
extern NTSTATUS RtlMultiByteToUnicodeN(wchar_t *pwcBuffer, ULONG ulBufferSize, ULONG *pulBytesMax, const char *pchMultiByteString, ULONG ulMultiByteStringSize) noexcept;
__attribute__((__dllimport__, __stdcall__))
extern NTSTATUS RtlUnicodeToMultiByteN(char *pchBuffer, ULONG ulBufferSize, ULONG *pulBytesMax, const wchar_t *pwcUnicodeString, ULONG ulUnicodeStringSize) noexcept;

}

namespace MCF {

namespace {
	// https://en.wikipedia.org/wiki/UTF-8
	// https://en.wikipedia.org/wiki/UTF-16
	// https://en.wikipedia.org/wiki/CESU-8

	template<typename CharT>
	class StringSource {
	private:
		const CharT *x_pchRead;
		const CharT *const x_pchEnd;

	public:
		StringSource(const CharT *pchBegin, const CharT *pchEnd) noexcept
			: x_pchRead(pchBegin), x_pchEnd(pchEnd)
		{
		}

	public:
		__attribute__((__flatten__))
		explicit operator bool() const noexcept {
			return x_pchRead != x_pchEnd;
		}
		__attribute__((__flatten__))
		std::uint32_t operator()(){
			const auto pchRead = x_pchRead;
			if(pchRead == x_pchEnd){
				MCF_THROW(Exception, ERROR_HANDLE_EOF, Rcntws::View(L"StringSource: 在字符串结尾处遇到不完整的编码点。"));
			}
			x_pchRead = pchRead + 1;
			return static_cast<std::make_unsigned_t<CharT>>(*pchRead);
		}
	};

	template<class StringViewT>
	auto MakeStringSource(const StringViewT &svRead) noexcept {
		return StringSource<typename StringViewT::Char>(svRead.GetBegin(), svRead.GetEnd());
	}

	template<class PrevT, bool kIsCesu8T>
	class Utf8Decoder {
	private:
		PrevT x_vPrev;

	public:
		explicit Utf8Decoder(PrevT vPrev)
			: x_vPrev(std::move(vPrev))
		{
		}

	public:
		__attribute__((__flatten__))
		explicit operator bool() const noexcept {
			return !!x_vPrev;
		}
		__attribute__((__flatten__))
		std::uint32_t operator()(){
			static constexpr unsigned char kByteCountTable[32] = {
				1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
				0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 4, 0,
			};

			auto u32Point = x_vPrev();
			// 这个值是该码点的总字节数。
			const unsigned uBytes = kByteCountTable[(u32Point >> 3) & 0x1F];
			if(uBytes == 0){
				MCF_THROW(Exception, ERROR_INVALID_DATA, Rcntws::View(L"Utf8Decoder: 编码单元无效（首字节）。"));
			}
			if(uBytes == 1){
				return u32Point & 0x7Fu;
			}

			const auto Unrolled = [&]{
				const auto u32Temp = x_vPrev();
				if((u32Temp & 0xC0u) != 0x80u){
					MCF_THROW(Exception, ERROR_INVALID_DATA, Rcntws::View(L"Utf8Decoder: 编码单元无效（非首字节）。"));
				}
				u32Point = (u32Point << 6) | (u32Temp & 0x3Fu);
			};

			if(uBytes == 2){
				u32Point &= 0x1Fu;

				Unrolled();
			} else if(uBytes == 3){
				u32Point &= 0x0Fu;

				Unrolled();
				Unrolled();
			} else {
				u32Point &= 0x07u;

				Unrolled();
				Unrolled();
				Unrolled();
			}
			if(u32Point > 0x10FFFFu){
				MCF_THROW(Exception, ERROR_INVALID_DATA, Rcntws::View(L"Utf8Decoder: 编码点的值超过规定范围。"));
			}
			if(!kIsCesu8T && (u32Point - 0xD800u < 0x800u)){
				MCF_THROW(Exception, ERROR_INVALID_DATA, Rcntws::View(L"Utf8Decoder: 编码点的值是为 UTF-16 保留的。"));
			}
			return u32Point;
		}
	};

	template<class PrevT>
	auto MakeUtf8Decoder(PrevT vPrev){
		return Utf8Decoder<PrevT, false>(std::move(vPrev));
	}
	template<class PrevT>
	auto MakeCesu8Decoder(PrevT vPrev){
		return Utf8Decoder<PrevT, true>(std::move(vPrev));
	}

	template<class PrevT>
	class Utf8Encoder {
	private:
		PrevT x_vPrev;
		std::uint32_t x_u32Pending;

	public:
		explicit Utf8Encoder(PrevT vPrev)
			: x_vPrev(std::move(vPrev)), x_u32Pending(0)
		{
		}

	public:
		__attribute__((__flatten__))
		explicit operator bool() const noexcept {
			return x_u32Pending || !!x_vPrev;
		}
		__attribute__((__flatten__))
		std::uint32_t operator()(){
			if(x_u32Pending){
				const auto u32Ret = x_u32Pending & 0xFFu;
				x_u32Pending >>= 8;
				return u32Ret;
			}

			auto u32Point = x_vPrev();
			if(u32Point > 0x10FFFFu){
				MCF_THROW(Exception, ERROR_INVALID_DATA, Rcntws::View(L"Utf8Encoder: 编码点的值超过规定范围。"));
			}
			// 这个值是该码点的总字节数。
			if((u32Point >> 7) == 0){ // u32Point < 0x80u
				return u32Point;
			}

			const auto Unrolled = [&]{
				x_u32Pending <<= 8;
				x_u32Pending |= (u32Point & 0x3F) | 0x80u;
				u32Point >>= 6;
			};

			if((u32Point >> 11) == 0){ // u32Point < 0x800u
				Unrolled();

				u32Point |= 0xC0;
			} else if((u32Point >> 16) == 0){ // u32Point < 0x10000
				Unrolled();
				Unrolled();

				u32Point |= 0xE0;
			} else {
				Unrolled();
				Unrolled();
				Unrolled();

				u32Point |= 0xF0;
			}
			return u32Point & 0xFFu;
		}
	};

	template<class PrevT>
	auto MakeUtf8Encoder(PrevT vPrev){
		return Utf8Encoder<PrevT>(std::move(vPrev));
	}

	template<class PrevT>
	class Utf16Decoder {
	private:
		PrevT x_vPrev;

	public:
		explicit Utf16Decoder(PrevT vPrev)
			: x_vPrev(std::move(vPrev))
		{
		}

	public:
		__attribute__((__flatten__))
		explicit operator bool() const noexcept {
			return !!x_vPrev;
		}
		__attribute__((__flatten__))
		std::uint32_t operator()(){
			auto u32Point = x_vPrev();
			// 检测前导代理。
			const auto u32Leading = u32Point - 0xD800u;
			if(u32Leading <= 0x7FFu){
				if(u32Leading > 0x3FFu){
					MCF_THROW(Exception, ERROR_INVALID_DATA, Rcntws::View(L"Utf16Decoder: 孤立的后尾代理。"));
				}
				const auto u32Trailing = x_vPrev() - 0xDC00u;
				// 检测后尾代理。
				if(u32Trailing > 0x3FFu){
					MCF_THROW(Exception, ERROR_INVALID_DATA, Rcntws::View(L"Utf16Decoder: 在前导代理后遇到不是后尾代理的编码单元。"));
				}
				// 将代理对拼成一个码点。
				u32Point = ((u32Leading << 10) | u32Trailing) + 0x10000u;
			}
			return u32Point;
		}
	};

	template<class PrevT>
	auto MakeUtf16Decoder(PrevT vPrev){
		return Utf16Decoder<PrevT>(std::move(vPrev));
	}

	template<class PrevT>
	class Utf16Encoder {
	private:
		PrevT x_vPrev;
		std::uint32_t x_u32Pending;

	public:
		explicit Utf16Encoder(PrevT vPrev)
			: x_vPrev(std::move(vPrev)), x_u32Pending(0)
		{
		}

	public:
		__attribute__((__flatten__))
		explicit operator bool() const noexcept {
			return x_u32Pending || !!x_vPrev;
		}
		__attribute__((__flatten__))
		std::uint32_t operator()(){
			if(x_u32Pending){
				const auto u32Ret = x_u32Pending;
				x_u32Pending >>= 16;
				return u32Ret;
			}

			auto u32Point = x_vPrev();
			if(u32Point > 0x10FFFFu){
				MCF_THROW(Exception, ERROR_INVALID_DATA, Rcntws::View(L"Utf16Encoder: 编码点的值超过规定范围。"));
			}
			if(u32Point > 0xFFFFu){
				// 编码成代理对。
				u32Point -= 0x10000u;
				x_u32Pending = (u32Point & 0x3FFu) | 0xDC00u;
				u32Point = (u32Point >> 10) | 0xD800u;
			}
			return u32Point;
		}
	};

	template<class PrevT>
	auto MakeUtf16Encoder(PrevT vPrev){
		return Utf16Encoder<PrevT>(std::move(vPrev));
	}

	template<class StringT, class FilterT>
	__attribute__((__flatten__, __optimize__("-funroll-loops")))
	void Convert(StringT &strWrite, FilterT vFilter){
		std::size_t uOldSize = strWrite.GetSize();
		try {
			for(;;){
				strWrite.ReserveMore(8);
				for(std::size_t i = 0; i < 8; ++i){
					if(!vFilter){
						goto jDone;
					}
					strWrite.UncheckedPush(static_cast<typename StringT::Char>(vFilter()));
				}
			}
		jDone:
			;
		} catch(...){
			strWrite.Pop(strWrite.GetSize() - uOldSize);
			throw;
		}
	}
}

template class String<StringType::kNarrow>;
template class String<StringType::kWide>;
template class String<StringType::kUtf8>;
template class String<StringType::kUtf16>;
template class String<StringType::kUtf32>;
template class String<StringType::kCesu8>;
template class String<StringType::kAnsi>;

// UTF-8
template<>
__attribute__((__flatten__))
void NarrowString::UnifyAppend(String<StringType::kUtf16> &u16sDst, const NarrowStringView &svSrc){
	u16sDst.ReserveMore(svSrc.GetSize());
	Convert(u16sDst, MakeUtf16Encoder(MakeUtf8Decoder(MakeStringSource(svSrc))));
}
template<>
__attribute__((__flatten__))
void NarrowString::DeunifyAppend(NarrowString &strDst, const StringView<StringType::kUtf16> &u16svSrc){
	strDst.ReserveMore(u16svSrc.GetSize() * 3);
	Convert(strDst, MakeUtf8Encoder(MakeUtf16Decoder(MakeStringSource(u16svSrc))));
}

template<>
__attribute__((__flatten__))
void NarrowString::UnifyAppend(String<StringType::kUtf32> &u32sDst, const NarrowStringView &svSrc){
	u32sDst.ReserveMore(svSrc.GetSize());
	Convert(u32sDst, MakeUtf8Decoder(MakeStringSource(svSrc)));
}
template<>
__attribute__((__flatten__))
void NarrowString::DeunifyAppend(NarrowString &strDst, const StringView<StringType::kUtf32> &u32svSrc){
	strDst.ReserveMore(u32svSrc.GetSize() * 2);
	Convert(strDst, MakeUtf8Encoder(MakeStringSource(u32svSrc)));
}

// UTF-16
template<>
__attribute__((__flatten__))
void WideString::UnifyAppend(String<StringType::kUtf16> &u16sDst, const WideStringView &svSrc){
	u16sDst.Append(reinterpret_cast<const char16_t *>(svSrc.GetBegin()), svSrc.GetSize());
}
template<>
__attribute__((__flatten__))
void WideString::DeunifyAppend(WideString &strDst, const StringView<StringType::kUtf16> &u16svSrc){
	strDst.Append(reinterpret_cast<const wchar_t *>(u16svSrc.GetBegin()), u16svSrc.GetSize());
}

template<>
__attribute__((__flatten__))
void WideString::UnifyAppend(String<StringType::kUtf32> &u32sDst, const WideStringView &svSrc){
	u32sDst.ReserveMore(svSrc.GetSize());
	Convert(u32sDst, MakeUtf16Decoder(MakeStringSource(svSrc)));
}
template<>
__attribute__((__flatten__))
void WideString::DeunifyAppend(WideString &strDst, const StringView<StringType::kUtf32> &u32svSrc){
	strDst.ReserveMore(u32svSrc.GetSize());
	Convert(strDst, MakeUtf16Encoder(MakeStringSource(u32svSrc)));
}

// UTF-8
template<>
__attribute__((__flatten__))
void Utf8String::UnifyAppend(String<StringType::kUtf16> &u16sDst, const Utf8StringView &svSrc){
	u16sDst.ReserveMore(svSrc.GetSize());
	Convert(u16sDst, MakeUtf16Encoder(MakeUtf8Decoder(MakeStringSource(svSrc))));
}
template<>
__attribute__((__flatten__))
void Utf8String::DeunifyAppend(Utf8String &strDst, const StringView<StringType::kUtf16> &u16svSrc){
	strDst.ReserveMore(u16svSrc.GetSize() * 3);
	Convert(strDst, MakeUtf8Encoder(MakeUtf16Decoder(MakeStringSource(u16svSrc))));
}

template<>
__attribute__((__flatten__))
void Utf8String::UnifyAppend(String<StringType::kUtf32> &u32sDst, const Utf8StringView &svSrc){
	u32sDst.ReserveMore(svSrc.GetSize());
	Convert(u32sDst, MakeUtf8Decoder(MakeStringSource(svSrc)));
}
template<>
__attribute__((__flatten__))
void Utf8String::DeunifyAppend(Utf8String &strDst, const StringView<StringType::kUtf32> &u32svSrc){
	strDst.ReserveMore(u32svSrc.GetSize() * 2);
	Convert(strDst, MakeUtf8Encoder(MakeStringSource(u32svSrc)));
}

// UTF-16
template<>
__attribute__((__flatten__))
void Utf16String::UnifyAppend(String<StringType::kUtf16> &u16sDst, const Utf16StringView &svSrc){
	u16sDst.Append(svSrc.GetBegin(), svSrc.GetSize());
}
template<>
__attribute__((__flatten__))
void Utf16String::DeunifyAppend(Utf16String &strDst, const StringView<StringType::kUtf16> &u16svSrc){
	strDst.Append(u16svSrc.GetBegin(), u16svSrc.GetSize());
}

template<>
__attribute__((__flatten__))
void Utf16String::UnifyAppend(String<StringType::kUtf32> &u32sDst, const Utf16StringView &svSrc){
	u32sDst.ReserveMore(svSrc.GetSize());
	Convert(u32sDst, MakeUtf16Decoder(MakeStringSource(svSrc)));
}
template<>
__attribute__((__flatten__))
void Utf16String::DeunifyAppend(Utf16String &strDst, const StringView<StringType::kUtf32> &u32svSrc){
	strDst.ReserveMore(u32svSrc.GetSize());
	Convert(strDst, MakeUtf16Encoder(MakeStringSource(u32svSrc)));
}

// UTF-32
template<>
__attribute__((__flatten__))
void Utf32String::UnifyAppend(String<StringType::kUtf16> &u16sDst, const Utf32StringView &svSrc){
	u16sDst.ReserveMore(svSrc.GetSize());
	Convert(u16sDst, MakeUtf16Encoder(MakeStringSource(svSrc)));
}
template<>
__attribute__((__flatten__))
void Utf32String::DeunifyAppend(Utf32String &strDst, const StringView<StringType::kUtf16> &u16svSrc){
	strDst.ReserveMore(u16svSrc.GetSize());
	Convert(strDst, MakeUtf16Decoder(MakeStringSource(u16svSrc)));
}

template<>
__attribute__((__flatten__))
void Utf32String::UnifyAppend(String<StringType::kUtf32> &u32sDst, const Utf32StringView &svSrc){
	u32sDst.Append(svSrc.GetBegin(), svSrc.GetSize());
}
template<>
__attribute__((__flatten__))
void Utf32String::DeunifyAppend(Utf32String &strDst, const StringView<StringType::kUtf32> &u32svSrc){
	strDst.Append(u32svSrc.GetBegin(), u32svSrc.GetSize());
}

// CESU-8
template<>
__attribute__((__flatten__))
void Cesu8String::UnifyAppend(String<StringType::kUtf16> &u16sDst, const Cesu8StringView &svSrc){
	u16sDst.ReserveMore(svSrc.GetSize());
	Convert(u16sDst, MakeCesu8Decoder(MakeStringSource(svSrc)));
}
template<>
__attribute__((__flatten__))
void Cesu8String::DeunifyAppend(Cesu8String &strDst, const StringView<StringType::kUtf16> &u16svSrc){
	strDst.ReserveMore(u16svSrc.GetSize() * 3);
	Convert(strDst, MakeUtf8Encoder(MakeStringSource(u16svSrc)));
}

template<>
__attribute__((__flatten__))
void Cesu8String::UnifyAppend(String<StringType::kUtf32> &u32sDst, const Cesu8StringView &svSrc){
	u32sDst.ReserveMore(svSrc.GetSize());
	Convert(u32sDst, MakeUtf16Decoder(MakeCesu8Decoder(MakeStringSource(svSrc))));
}
template<>
__attribute__((__flatten__))
void Cesu8String::DeunifyAppend(Cesu8String &strDst, const StringView<StringType::kUtf32> &u32svSrc){
	strDst.ReserveMore(u32svSrc.GetSize() * 2);
	Convert(strDst, MakeUtf8Encoder(MakeUtf16Encoder(MakeStringSource(u32svSrc))));
}

// ANSI
template<>
__attribute__((__flatten__))
void AnsiString::UnifyAppend(String<StringType::kUtf16> &u16sDst, const AnsiStringView &svSrc){
	const auto uInputSize = svSrc.GetSize() * sizeof(char);
	if(uInputSize > ULONG_MAX){
		MCF_THROW(Exception, ERROR_NOT_ENOUGH_MEMORY, Rcntws::View(L"AnsiString: 输入的 ANSI 字符串太长。"));
	}
	const auto uOutputSizeMax = svSrc.GetSize() * sizeof(wchar_t);
	if((uOutputSizeMax > ULONG_MAX) || (uOutputSizeMax / sizeof(wchar_t) != svSrc.GetSize())){
		MCF_THROW(Exception, ERROR_NOT_ENOUGH_MEMORY, Rcntws::View(L"AnsiString: 输出的 UTF-16 字符串太长。"));
	}
	const auto pchWrite = u16sDst.ResizeMore(uOutputSizeMax / sizeof(wchar_t));
	try {
		ULONG ulConvertedSize;
		const auto lStatus = ::RtlMultiByteToUnicodeN(reinterpret_cast<wchar_t *>(pchWrite), (DWORD)uOutputSizeMax, &ulConvertedSize, svSrc.GetBegin(), (DWORD)uInputSize);
		if(!NT_SUCCESS(lStatus)){
			MCF_THROW(Exception, ::RtlNtStatusToDosError(lStatus), Rcntws::View(L"AnsiString: RtlMultiByteToUnicodeN() 失败。"));
		}
		u16sDst.Pop(uOutputSizeMax / sizeof(wchar_t) - ulConvertedSize / sizeof(wchar_t));
	} catch(...){
		u16sDst.Pop(uOutputSizeMax / sizeof(wchar_t));
		throw;
	}
}
template<>
__attribute__((__flatten__))
void AnsiString::DeunifyAppend(AnsiString &strDst, const StringView<StringType::kUtf16> &u16svSrc){
	const auto uInputSize = u16svSrc.GetSize() * sizeof(wchar_t);
	if((uInputSize > ULONG_MAX) || (uInputSize / sizeof(wchar_t) != u16svSrc.GetSize())){
		MCF_THROW(Exception, ERROR_NOT_ENOUGH_MEMORY, Rcntws::View(L"AnsiString: 输入的 UTF-16 字符串太长。"));
	}
	const auto uOutputSizeMax = u16svSrc.GetSize() * 2 * sizeof(char);
	if((uOutputSizeMax > ULONG_MAX) || (uOutputSizeMax / (2 * sizeof(char)) != u16svSrc.GetSize())){
		MCF_THROW(Exception, ERROR_NOT_ENOUGH_MEMORY, Rcntws::View(L"AnsiString: 输出的 ANSI 字符串太长。"));
	}
	const auto pchWrite = strDst.ResizeMore(uOutputSizeMax / sizeof(char));
	try {
		ULONG ulConvertedSize;
		const auto lStatus = ::RtlUnicodeToMultiByteN(pchWrite, (DWORD)uOutputSizeMax, &ulConvertedSize, reinterpret_cast<const wchar_t *>(u16svSrc.GetBegin()), (DWORD)uInputSize);
		if(!NT_SUCCESS(lStatus)){
			MCF_THROW(Exception, ::RtlNtStatusToDosError(lStatus), Rcntws::View(L"AnsiString: RtlUnicodeToMultiByteN() 失败。"));
		}
		strDst.Pop(uOutputSizeMax / sizeof(char) - ulConvertedSize / sizeof(char));
	} catch(...){
		strDst.Pop(uOutputSizeMax / sizeof(char));
		throw;
	}
}

template<>
__attribute__((__flatten__))
void AnsiString::UnifyAppend(String<StringType::kUtf32> &u32sDst, const AnsiStringView &svSrc){
	Utf16String u16sTemp;
	UnifyAppend(u16sTemp, svSrc);
	Utf16String::UnifyAppend(u32sDst, u16sTemp);
}
template<>
__attribute__((__flatten__))
void AnsiString::DeunifyAppend(AnsiString &strDst, const StringView<StringType::kUtf32> &u32svSrc){
	Utf16String u16sTemp;
	Utf16String::DeunifyAppend(u16sTemp, u32svSrc);
	DeunifyAppend(strDst, u16sTemp);
}

}