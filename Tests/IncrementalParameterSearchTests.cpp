// TEST120: stand-alone search regression test (no JUCE / VST3 dependency).
#include "../Source/IncrementalParameterSearch.h"
#if defined(min) || defined(max)
#error "IncrementalParameterSearch must never leak Win32 min/max macros"
#endif
#include <algorithm>
// GitHub Actions builds this target in Release: keep assertions enabled.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

int main() {
    assert(std::min(2,3) == 2 && std::max(2,3) == 3);
    ljuno::IncrementalParameterSearch search;
    const std::vector<std::wstring> names{
        L"Osc Volume", L"Osc Detune", L"Filter Cutoff", L"Filter Resonance",
        L"LFO 1 Rate", L"LFO 2 Rate", L"MIDI Channel 1", L"MIDI Channel 16",
        L"1 Voice Mode", L"16 Source", L"Noise Velocity", L"Égaliseur 1"
    };
    auto label = [&](int i) { return names.at(static_cast<size_t>(i)); };
    auto next = [&](wchar_t c,int from,int dir,std::uint32_t t,bool repeat=false) {
        return search.next(c,from,static_cast<int>(names.size()),dir,t,label,repeat);
    };
    int row = next(L'f',0,1,100); assert(row == 2);
    row = next(L'i',row,1,200); assert(row == 2 && search.text() == L"fi");
    row = next(L'l',row,1,270); assert(row == 2 && search.text() == L"fil");
    row = next(L'f',row,1,1500); assert(row == 3);
    row = next(L'f',row,1,1550); assert(row == 2);  // cyclic wrap
    assert(next(L'f',row,1,1560,true) == -1); // repeat event ignored
    search.reset(); row=next(L'c',0,1,1900); assert(row == 2); // internal word "Cutoff"
    search.reset(); row=next(L'r',0,1,2000); assert(row == 3); // "Resonance"
    search.reset(); row=next(L'1',8,-1,2500); assert(row == 9); // full-name match
    row=next(L'6',row,-1,2550); assert(row==9 && search.text()==L"16");
    search.reset(); row=next(L'n',10,-1,3200); assert(row==10); // backwards search wraps to the current matching item
    search.reset(); row=next(L'1',0,1,3500); assert(row==8); // label-start preferred
    search.reset(); row=next(L'm',2,1,4000); assert(row==6);
    row=next(L'x',row,1,4050); assert(row==-1 && search.text()==L"x"); // restart miss
    search.reset(); row=next(L'e',0,1,4500); assert(row==-1); // e accent is different
    search.reset(); row=next(L'É',0,1,4600); assert(row==11); // Unicode case
    search.reset(); row=next(L'f',0,1,0xfffffff0u); assert(row==2);
    row=next(L'i',row,1,30u); assert(row==2 && search.text()==L"fi"); // clock rollover
    std::cout << "PASS TEST120: progressive prefix, words, wrap, Shift direction/digits, timeout, Unicode, Win32 NOMINMAX\n";
}
