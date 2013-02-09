#include "stdafx.h"

_COM_SMARTPTR_TYPEDEF(IWICImagingFactory, __uuidof(IWICImagingFactory));
_COM_SMARTPTR_TYPEDEF(IWICBitmapDecoder, __uuidof(IWICBitmapDecoder));
_COM_SMARTPTR_TYPEDEF(IWICBitmapSource, __uuidof(IWICBitmapSource));
_COM_SMARTPTR_TYPEDEF(IWICBitmapFrameDecode, __uuidof(IWICBitmapFrameDecode));
_COM_SMARTPTR_TYPEDEF(IWICFormatConverter, __uuidof(IWICFormatConverter));

enum class ExitCode
{
	Success = 0,
	Failure = 1,
};

struct Result
{
	ExitCode code;
	std::vector<std::string> isbns;

	Result(ExitCode code) : code(code) {}
};

Result ReadISBN(const wchar_t* filepath)
{
	using namespace zbar;
	HRESULT hr;
	IWICImagingFactoryPtr wic;
	IWICBitmapDecoderPtr decoder;
	IWICBitmapFrameDecodePtr frame;
	IWICBitmapSourcePtr source;

	hr = wic.CreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER);
	if (FAILED(hr)) {
		fwprintf(stderr, L"Failed to Initialize WIC\n");
		return Result(ExitCode::Failure);
	}
	
	hr = wic->CreateDecoderFromFilename(filepath, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
	if (FAILED(hr)) {
		fwprintf(stderr, L"Failed to open %s\n", filepath);
		return Result(ExitCode::Failure);
	}

	decoder->GetFrame(0, &frame);
	UINT width, height;
	WICPixelFormatGUID pf;
	frame->GetSize(&width, &height);
	frame->GetPixelFormat(&pf);
	if (pf != GUID_WICPixelFormat8bppGray) {
		IWICFormatConverterPtr grayscaled;
		wic->CreateFormatConverter(&grayscaled);
		grayscaled->Initialize(frame, GUID_WICPixelFormat8bppGray, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom);
		source = grayscaled;
	} else {
		source = frame;
	}
	
	std::vector<BYTE> buf(width * height * 1);
	WICRect rc = { 0, 0, width, height };
	hr = source->CopyPixels(&rc, width, buf.size(), buf.data());

	Image img(width, height, "Y800", buf.data(), buf.size());
	ImageScanner scanner;
	scanner.set_config(ZBAR_ISBN13, ZBAR_CFG_ENABLE, 1);
	int scanRet = scanner.scan(img);
	if (scanRet < 0) {
		fwprintf(stderr, L"ZBar scan error(%d)\n", scanRet);
		return Result(ExitCode::Failure);
	}

	Result ret(ExitCode::Success);
	auto symbols = scanner.get_results();
	for (auto s = symbols.symbol_begin(); s != symbols.symbol_end(); ++s) {
		if (s->get_type() == ZBAR_ISBN13) {
			ret.isbns.push_back(s->get_data());
		}
	}
	return ret;
}



int wmain(int argc, wchar_t* argv[])
{
	if (argc <= 1) {
		fwprintf(stderr, L"usage: isbnx.exe <input>\n");
		return (int) ExitCode::Failure;
	}

	::CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
	
	auto ret = ReadISBN(argv[1]);
	if (ret.code == ExitCode::Success) {
		printf ("Found: %d\n", ret.isbns.size());
		for (auto& isbn : ret.isbns) {
			printf("%s\n", isbn.c_str());
		}
	}

	::CoUninitialize();
	return (int) ret.code;}

