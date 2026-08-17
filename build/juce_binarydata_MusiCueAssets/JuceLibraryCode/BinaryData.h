/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   add_svg;
    const int            add_svgSize = 163;

    extern const char*   play_arrow_svg;
    const int            play_arrow_svgSize = 171;

    extern const char*   stop_svg;
    const int            stop_svgSize = 173;

    extern const char*   pause_svg;
    const int            pause_svgSize = 235;

    extern const char*   fiber_manual_record_svg;
    const int            fiber_manual_record_svgSize = 308;

    extern const char*   undo_svg;
    const int            undo_svgSize = 267;

    extern const char*   view_agenda_svg;
    const int            view_agenda_svgSize = 418;

    extern const char*   settings_svg;
    const int            settings_svgSize = 752;

    extern const char*   workspace_open_svg;
    const int            workspace_open_svgSize = 137;

    extern const char*   workspace_save_svg;
    const int            workspace_save_svgSize = 164;

    extern const char*   workspace_save_as_svg;
    const int            workspace_save_as_svgSize = 189;

    extern const char*   appicon_musicue_png;
    const int            appicon_musicue_pngSize = 945559;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 12;

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
