/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   icon_ico;
    const int            icon_icoSize = 332898;

    extern const char*   logo_png;
    const int            logo_pngSize = 81795;

    extern const char*   On_stage_logo_png;
    const int            On_stage_logo_pngSize = 76514;

    extern const char*   ir_wav;
    const int            ir_wavSize = 1738792;

    extern const char*   license_mid;
    const int            license_midSize = 40282;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 5;

    // Points to the start of a list of resource names.
    extern const char* namedResourceList[];

    // Points to the start of a list of resource filenames.
    extern const char* originalFilenames[];

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding data and its size (or a null pointer if the name isn't found).
    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding original, non-mangled filename (or a null pointer if the name isn't found).
    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
}
