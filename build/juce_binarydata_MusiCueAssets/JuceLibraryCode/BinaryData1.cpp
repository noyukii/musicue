/* ==================================== JUCER_BINARY_RESOURCE ====================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#include <cstring>

namespace BinaryData
{

//================== add.svg ==================
static const unsigned char temp_binary_data_0[] =
"<svg xmlns=\"http://www.w3.org/2000/svg\" height=\"24\" viewBox=\"0 -960 960 960\" width=\"24\"><path d=\"M440-440H200v-80h240v-240h80v240h240v80H520v240h-80v-240Z\"/></svg>";

const char* add_svg = (const char*) temp_binary_data_0;

}

#include "BinaryData.h"

namespace BinaryData
{

const char* getNamedResource (const char* resourceNameUTF8, int& numBytes);
const char* getNamedResource (const char* resourceNameUTF8, int& numBytes)
{
    unsigned int hash = 0;

    if (resourceNameUTF8 != nullptr)
        while (*resourceNameUTF8 != 0)
            hash = 31 * hash + (unsigned int) *resourceNameUTF8++;

    switch (hash)
    {
        case 0xbb8f2dc6:  numBytes = 163; return add_svg;
        case 0x24d649e3:  numBytes = 171; return play_arrow_svg;
        case 0x663b24a7:  numBytes = 173; return stop_svg;
        case 0x3175289b:  numBytes = 235; return pause_svg;
        case 0x2c8c5afc:  numBytes = 308; return fiber_manual_record_svg;
        case 0xe5c4b489:  numBytes = 267; return undo_svg;
        case 0x9b87958b:  numBytes = 418; return view_agenda_svg;
        case 0x1fdee048:  numBytes = 752; return settings_svg;
        case 0xf2267b59:  numBytes = 137; return workspace_open_svg;
        case 0x94b1450c:  numBytes = 164; return workspace_save_svg;
        case 0x622ea92f:  numBytes = 189; return workspace_save_as_svg;
        case 0x463aebba:  numBytes = 945559; return appicon_musicue_png;
        default: break;
    }

    numBytes = 0;
    return nullptr;
}

const char* namedResourceList[] =
{
    "add_svg",
    "play_arrow_svg",
    "stop_svg",
    "pause_svg",
    "fiber_manual_record_svg",
    "undo_svg",
    "view_agenda_svg",
    "settings_svg",
    "workspace_open_svg",
    "workspace_save_svg",
    "workspace_save_as_svg",
    "appicon_musicue_png"
};

const char* originalFilenames[] =
{
    "add.svg",
    "play_arrow.svg",
    "stop.svg",
    "pause.svg",
    "fiber_manual_record.svg",
    "undo.svg",
    "view_agenda.svg",
    "settings.svg",
    "workspace_open.svg",
    "workspace_save.svg",
    "workspace_save_as.svg",
    "appicon_musicue.png"
};

const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8)
{
    for (unsigned int i = 0; i < (sizeof (namedResourceList) / sizeof (namedResourceList[0])); ++i)
        if (strcmp (namedResourceList[i], resourceNameUTF8) == 0)
            return originalFilenames[i];

    return nullptr;
}

}
