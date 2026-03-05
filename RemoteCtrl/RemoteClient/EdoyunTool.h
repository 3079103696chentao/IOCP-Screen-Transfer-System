#pragma once
#include<windows.h>
#include<string>
#include<atlimage.h>

class CEdoyunTool
{
public:
   static void Dump(BYTE* pData, size_t nSize) {
        std::string strOut;
        for (size_t i = 0; i < nSize; i++) {
            char buf[8] = "";
            if (i > 0 && (i % 16 == 0)) strOut += "\n";
            snprintf(buf, sizeof(buf), "%02X ", pData[i] & 0xFF); //不足两位补0，和0xFF，确保只取低八位
            strOut += buf;
        }
        strOut += "\n";
        OutputDebugStringA(strOut.c_str());
    }
   static int Bytes2Image(CImage& image, const std::string& strBuffer) {
	
	   HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, 0);
	   if (hMem == NULL) {
		   TRACE(_T("内存不足！"));
		   Sleep(1);
		   return -1;
	   }
	   IStream* pStream = nullptr;
	   HRESULT hRet = CreateStreamOnHGlobal(hMem, TRUE, &pStream);
	   if (hRet == S_OK) {
		   ULONG length = 0;
		   pStream->Write(strBuffer.c_str(), strBuffer.size(), &length);
		   LARGE_INTEGER bg = { 0 };
		   pStream->Seek(bg, STREAM_SEEK_SET, NULL); //将流指针指向文件开头
		   if ((HBITMAP)image != NULL) {
			   image.Destroy();
		   }
		   image.Load(pStream);
	   }
	   return hRet;
   }
};

