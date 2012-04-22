#define MAIN_CPP
#ifndef __HMAIN_CPP
#include "main.h"
#include "CDocument.h"
#include "CElement.h"
#include "CNode.h"
#include "CTextNode.h"
#include "CReader.h"
#include "CExplorer.h"
#endif


void utf8toWStr(wstring& dest, const string& src){
    dest.clear();
    wchar_t w = 0;
    int bytes = 0;
    wchar_t err = L'�';
    for (size_t i = 0; i < src.size(); i++){
        unsigned char c = (unsigned char)src[i];
        if (c <= 0x7f){//first byte
            if (bytes){
                dest.push_back(err);
                bytes = 0;
            }
            dest.push_back((wchar_t)c);
        }
        else if (c <= 0xbf){//second/third/etc byte
            if (bytes){
                w = ((w << 6)|(c & 0x3f));
                bytes--;
                if (bytes == 0)
                    dest.push_back(w);
            }
            else
                dest.push_back(err);
        }
        else if (c <= 0xdf){//2byte sequence start
            bytes = 1;
            w = c & 0x1f;
        }
        else if (c <= 0xef){//3byte sequence start
            bytes = 2;
            w = c & 0x0f;
        }
        else if (c <= 0xf7){//3byte sequence start
            bytes = 3;
            w = c & 0x07;
        }
        else{
            dest.push_back(err);
            bytes = 0;
        }
    }
    if (bytes)
        dest.push_back(err);
}

void wstrToUtf8(string& dest, const wstring& src){
    dest.clear();
    for (size_t i = 0; i < src.size(); i++){
        wchar_t w = src[i];
        if (w <= 0x7f)
            dest.push_back((char)w);
        else if (w <= 0x7ff){
            dest.push_back(0xc0 | ((w >> 6)& 0x1f));
            dest.push_back(0x80| (w & 0x3f));
        }
        else if (w <= 0xffff){
            dest.push_back(0xe0 | ((w >> 12)& 0x0f));
            dest.push_back(0x80| ((w >> 6) & 0x3f));
            dest.push_back(0x80| (w & 0x3f));
        }
        else if (w <= 0x10ffff){
            dest.push_back(0xf0 | ((w >> 18)& 0x07));
            dest.push_back(0x80| ((w >> 12) & 0x3f));
            dest.push_back(0x80| ((w >> 6) & 0x3f));
            dest.push_back(0x80| (w & 0x3f));
        }
        else
            dest.push_back('?');
    }
}


std::string WStringToString(const std::wstring& str)
{
    string result;
    wstrToUtf8(result, str);
    return result;
}

std::wstring StringToWString(const std::string& str)
{
    wstring result;
    utf8toWStr(result, str);
    return result;
}

wstring Html$(wstring text)
{
    wstring str, s;
    for(unsigned int i = 0; i < text.length(); i++)
    {
        s = text.at(i);
        if(s == L"<")
        {
            str += L"&lt;";
        }
        else if(s == L">")
        {
            str += L"&gt;";
        }
        else if(s == L"&")
        {
            if((text.substr(i, 6) == L"&nbsp;"))
            {str+= L"&nbsp"; i +=5;} //On ignore les espaces
            else str += L"&amp;";
        }
        else if(s == L"\"")
        {
            str += L"&quot;";
        }
        else
        {
            str += s;
        }
    }

    return str;
}


ostream &operator<<( ostream &out, std::wstring &str )
{
    out << WStringToString(str);
    return out;
}

ostream &operator<<( ostream &out, std::wstring *str )
{
    out << *str;
    return out;
}

/* http://www.w3.org/TR/REC-xml/#NT-NameStartChar

  NameStartChar	::=  ":" | [A-Z] | "_" | [a-z] | [#xC0-#xD6] | [#xD8-#xF6] | [#xF8-#x2FF] |
  [#x370-#x37D] | [#x37F-#x1FFF] | [#x200C-#x200D] | [#x2070-#x218F] |
  [#x2C00-#x2FEF] | [#x3001-#xD7FF] | [#xF900-#xFDCF] | [#xFDF0-#xFFFD] | [#x10000-#xEFFFF]
  */

#define INTER(min, max) (car >= min && car <= max)
#define INTERCAR(min, max) (car >= *(min) && car <= *(max))
#define CAR(c) (car == *(c))

bool isNameStartChar(wstring &s)
{
    const wchar_t car = (s.at(0));

    return CAR(":") || INTERCAR("A", "Z") || CAR("_") || INTERCAR("a", "z") || CAR("Ø") ||
            INTER(0xC0, 0xD6) || INTER(0xD8, 0xF6) || INTER(0xF8, 0x2FF) ||
            INTER(0x370, 0x37D) || INTER(0x37F, 0x1FFF) || INTER(0x200C, 0x200D) ||
            INTER(0x2070, 0x218F) || INTER(0xC200, 0x2FEF) || INTER(0x3001, 0xD7FF) ||
            INTER(0xF900, 0xFDCF) || INTER(0xFDF0, 0xFFFD) || INTER(0x10000, 0xEFFFF);


}

/* http://www.w3.org/TR/REC-xml/#NT-NameChar

  NameChar ::= 	NameStartChar | "-" | "." | [0-9] | #xB7 | [#x0300-#x036F] | [#x203F-#x2040]

  */

bool isNameChar(wstring &s)
{
    const wchar_t car = (s.at(0));

    return isNameStartChar(s) || CAR("-") || CAR(".") || INTERCAR("0", "9") ||
            (car == 0xB7) || INTER(0x0300, 0x036F) || INTER(0x203F, 0x2040);
}

/* http://www.w3.org/TR/REC-xml/#NT-S

    S ::= (#x20 | #x9 | #xD | #xA)+

  */

bool isWhiteSpace(wstring &s)
{
    const wchar_t car = (s.at(0));

    return (car == 0x20) || (car == 0x9) || (car == 0xD) || (car == 0xA);
}

#ifndef __HMAIN_CPP

GB_INTERFACE GB EXPORT;

extern "C"{
GB_DESC *GB_CLASSES[] EXPORT =
{
  CDocumentDesc, CNodeDesc, CElementAttributesDesc, CElementDesc, CTextNodeDesc,
    CCommentNodeDesc, CCDATANodeDesc, CReaderDesc, CReaderNodeDesc, CReaderNodeTypeDesc,
    CReaderNodeAttributesDesc, CReaderReadFlagsDesc, CExplorerDesc, CExplorerReadFlagsDesc, 0
};

int EXPORT GB_INIT(void)
{

  return -1;
}

void EXPORT GB_EXIT()
{

}
}
#endif
