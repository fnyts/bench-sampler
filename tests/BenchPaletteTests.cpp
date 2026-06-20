#include "app/BenchPalette.h"

#include <cassert>
#include <iostream>

#include "TestAssert.h"

namespace
{
void testTextColoursAreReadableOnPanel()
{
    assert (samplebench::contrastRatio (samplebench::palette::text, samplebench::palette::panel) >= 7.0);
    assert (samplebench::contrastRatio (samplebench::palette::mutedText, samplebench::palette::panel) >= 4.5);
    assert (samplebench::contrastRatio (samplebench::palette::inverseText, samplebench::palette::accent) >= 4.5);
}
}

int main()
{
    configureSampleBenchTestProcess();

    testTextColoursAreReadableOnPanel();

    std::cout << "BenchPaletteTests passed\n";
    return 0;
}
