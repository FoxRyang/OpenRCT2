#pragma region Copyright (c) 2014-2017 OpenRCT2 Developers
/*****************************************************************************
 * OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
 *
 * OpenRCT2 is the work of many authors, a full list can be found in contributors.md
 * For more information, visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * A full copy of the GNU General Public License can be found in licence.txt
 *****************************************************************************/
#pragma endregion

#include <chrono>
#include "../audio/audio.h"
#include "../Context.h"
#include "../core/Console.hpp"
#include "../Imaging.h"
#include "../OpenRCT2.h"
#include "Screenshot.h"

#include "../drawing/drawing.h"
#include "../game.h"
#include "../intro.h"
#include "../localisation/localisation.h"
#include "../platform/platform.h"
#include "../util/util.h"
#include "viewport.h"

using namespace OpenRCT2;

extern "C"
{
uint8 gScreenshotCountdown = 0;

/**
 *
 *  rct2: 0x006E3AEC
 */
void screenshot_check()
{
    sint32 screenshotIndex;

    if (gScreenshotCountdown != 0) {
        gScreenshotCountdown--;
        if (gScreenshotCountdown == 0) {
            // update_rain_animation();
            screenshotIndex = screenshot_dump();

            if (screenshotIndex != -1) {
                audio_play_sound(SOUND_WINDOW_OPEN, 100, context_get_width() / 2);
            } else {
                context_show_error(STR_SCREENSHOT_FAILED, STR_NONE);
            }

            // redraw_rain();
        }
    }
}

static void screenshot_get_rendered_palette(rct_palette* palette) {
    for (sint32 i = 0; i < 256; i++) {
        palette->entries[i] = gPalette[i];
    }
}

static sint32 screenshot_get_next_path(char *path, size_t size)
{
    char screenshotPath[MAX_PATH];

    platform_get_user_directory(screenshotPath, "screenshot", sizeof(screenshotPath));
    if (!platform_ensure_directory_exists(screenshotPath)) {
        log_error("Unable to save screenshots in OpenRCT2 screenshot directory.\n");
        return -1;
    }

    char park_name[128];
    format_string(park_name, 128, gParkName, &gParkNameArgs);

    // Retrieve current time
    rct2_date currentDate;
    platform_get_date_local(&currentDate);
    rct2_time currentTime;
    platform_get_time_local(&currentTime);

#ifdef _WIN32
    // On NTFS filesystems, a colon (:) in a path
    // indicates you want to write a file stream
    // (hidden metadata). This will pass the
    // file_exists and fopen checks, since it is
    // technically valid. We don't want that, so
    // replace colons with hyphens in the park name.
    char * foundColon = park_name;
    while ((foundColon = strchr(foundColon, ':')) != nullptr)
    {
        *foundColon = '-';
    }
#endif

    // Glue together path and filename
    snprintf(path, size, "%s%s %d-%02d-%02d %02d-%02d-%02d.png", screenshotPath, park_name, currentDate.year, currentDate.month, currentDate.day, currentTime.hour, currentTime.minute, currentTime.second);

    if (!platform_file_exists(path)) {
        return 0; // path ok
    }

    // multiple screenshots with same timestamp
    // might be possible when switching timezones
    // in the unlikely case that this does happen,
    // append (%d) to the filename and increment
    // this sint32 until it doesn't overwrite any
    // other file in the directory.
    sint32 i;
    for (i = 1; i < 1000; i++) {
        // Glue together path and filename
        snprintf(path, size, "%s%s %d-%02d-%02d %02d-%02d-%02d (%d).png", screenshotPath, park_name, currentDate.year, currentDate.month, currentDate.day, currentTime.hour, currentTime.minute, currentTime.second, i);

        if (!platform_file_exists(path)) {
            return i;
        }
    }

    log_error("You have too many saved screenshots saved at exactly the same date and time.\n");
    return -1;
}

sint32 screenshot_dump_png(rct_drawpixelinfo *dpi)
{
    // Get a free screenshot path
    sint32 index;
    char path[MAX_PATH] = "";
    if ((index = screenshot_get_next_path(path, MAX_PATH)) == -1) {
        return -1;
    }

    rct_palette renderedPalette;
    screenshot_get_rendered_palette(&renderedPalette);

    if (image_io_png_write(dpi, &renderedPalette, path)) {
        return index;
    } else {
        return -1;
    }
}

sint32 screenshot_dump_png_32bpp(sint32 width, sint32 height, const void *pixels)
{
    // Get a free screenshot path
    sint32 index;
    char path[MAX_PATH] = "";
    if ((index = screenshot_get_next_path(path, MAX_PATH)) == -1) {
        return -1;
    }

    if (image_io_png_write_32bpp(width, height, pixels, path)) {
        return index;
    } else {
        return -1;
    }
}

void screenshot_giant()
{
    sint32 originalRotation = get_current_rotation();
    sint32 originalZoom = 0;

    rct_window *mainWindow = window_get_main();
    if (mainWindow != nullptr && mainWindow->viewport != nullptr)
        originalZoom = mainWindow->viewport->zoom;

    sint32 rotation = originalRotation;
    sint32 zoom = originalZoom;
    sint32 mapSize = gMapSize;
    sint32 resolutionWidth = (mapSize * 32 * 2) >> zoom;
    sint32 resolutionHeight = (mapSize * 32 * 1) >> zoom;

    resolutionWidth += 8;
    resolutionHeight += 128;

    rct_viewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = resolutionWidth;
    viewport.height = resolutionHeight;
    viewport.view_width = viewport.width;
    viewport.view_height = viewport.height;
    viewport.var_11 = 0;
    viewport.flags = 0;

    sint32 centreX = (mapSize / 2) * 32 + 16;
    sint32 centreY = (mapSize / 2) * 32 + 16;

    sint32 x = 0, y = 0;
    sint32 z = tile_element_height(centreX, centreY) & 0xFFFF;
    switch (rotation) {
    case 0:
        x = centreY - centreX;
        y = ((centreX + centreY) / 2) - z;
        break;
    case 1:
        x = -centreY - centreX;
        y = ((-centreX + centreY) / 2) - z;
        break;
    case 2:
        x = -centreY + centreX;
        y = ((-centreX - centreY) / 2) - z;
        break;
    case 3:
        x = centreY + centreX;
        y = ((centreX - centreY) / 2) - z;
        break;
    }

    viewport.view_x = x - ((viewport.view_width << zoom) / 2);
    viewport.view_y = y - ((viewport.view_height << zoom) / 2);
    viewport.zoom = zoom;
    gCurrentRotation = rotation;

    // Ensure sprites appear regardless of rotation
    reset_all_sprite_quadrant_placements();

    rct_drawpixelinfo dpi;
    dpi.x = 0;
    dpi.y = 0;
    dpi.width = resolutionWidth;
    dpi.height = resolutionHeight;
    dpi.pitch = 0;
    dpi.zoom_level = 0;
    dpi.bits = (uint8 *)malloc(dpi.width * dpi.height);

    viewport_render(&dpi, &viewport, 0, 0, viewport.width, viewport.height);

    // Get a free screenshot path
    char path[MAX_PATH];
    if (screenshot_get_next_path(path, MAX_PATH) == -1) {
        log_error("Giant screenshot failed, unable to find a suitable destination path.");
        context_show_error(STR_SCREENSHOT_FAILED, STR_NONE);
        return;
    }

    rct_palette renderedPalette;
    screenshot_get_rendered_palette(&renderedPalette);

    image_io_png_write(&dpi, &renderedPalette, path);

    free(dpi.bits);

    // Show user that screenshot saved successfully
    set_format_arg(0, rct_string_id, STR_STRING);
    set_format_arg(2, char *, path_get_filename(path));
    context_show_error(STR_SCREENSHOT_SAVED_AS, STR_NONE);
}

sint32 cmdline_for_gfxbench(const char **argv, sint32 argc)
{
    if (argc != 1 && argc != 2) {
        printf("Usage: openrct2 benchgfx <file> [<iteration_count>]\n");
        return -1;
    }

    sint32 iteration_count = 40;
    if (argc == 2)
    {
        iteration_count = atoi(argv[1]);
    }

    sint32 resolutionWidth, resolutionHeight, customX = 0, customY = 0;

    const char *inputPath = argv[0];

    gOpenRCT2Headless = true;
    auto context = CreateContext();
    if (context->Initialise())
    {
        drawing_engine_init();
        context->LoadParkFromFile(inputPath);

        gIntroState = INTRO_STATE_NONE;
        gScreenFlags = SCREEN_FLAGS_PLAYING;

        sint32 mapSize = gMapSize;
        resolutionWidth = (mapSize * 32 * 2);
        resolutionHeight = (mapSize * 32 * 1);

        resolutionWidth += 8;
        resolutionHeight += 128;

        rct_viewport viewport;
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = resolutionWidth;
        viewport.height = resolutionHeight;
        viewport.view_width = viewport.width;
        viewport.view_height = viewport.height;
        viewport.var_11 = 0;
        viewport.flags = 0;

        customX = (mapSize / 2) * 32 + 16;
        customY = (mapSize / 2) * 32 + 16;

        sint32 x = 0, y = 0;
        sint32 z = tile_element_height(customX, customY) & 0xFFFF;
        x = customY - customX;
        y = ((customX + customY) / 2) - z;

        viewport.view_x = x - ((viewport.view_width) / 2);
        viewport.view_y = y - ((viewport.view_height) / 2);
        viewport.zoom = 0;
        gCurrentRotation = 0;

        // Ensure sprites appear regardless of rotation
        reset_all_sprite_quadrant_placements();

        rct_drawpixelinfo dpi;
        dpi.x = 0;
        dpi.y = 0;
        dpi.width = resolutionWidth;
        dpi.height = resolutionHeight;
        dpi.pitch = 0;
        dpi.bits = (uint8 *)malloc(dpi.width * dpi.height);

        auto startTime = std::chrono::high_resolution_clock::now();
        for (sint32 i = 0; i < iteration_count; i++)
        {
            // Render at various zoom levels
            dpi.zoom_level = i & 3;
            viewport_render(&dpi, &viewport, 0, 0, viewport.width, viewport.height);
        }
        auto endTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> duration = endTime - startTime;
        char engine_name[128];
        rct_string_id engine_id = DrawingEngineStringIds[drawing_engine_get_type()];
        format_string(engine_name, sizeof(engine_name), engine_id, nullptr);
        Console::WriteLine("Rendering %d times with drawing engine %s took %.2f seconds.",
                           iteration_count, engine_name,
                           duration.count());

        free(dpi.bits);
        drawing_engine_dispose();
    }
    delete context;
    return 1;
}

sint32 cmdline_for_screenshot(const char **argv, sint32 argc)
{
    bool giantScreenshot = argc == 5 && _stricmp(argv[2], "giant") == 0;
    if (argc != 4 && argc != 8 && !giantScreenshot) {
        printf("Usage: openrct2 screenshot <file> <ouput_image> <width> <height> [<x> <y> <zoom> <rotation>]\n");
        printf("Usage: openrct2 screenshot <file> <ouput_image> giant <zoom> <rotation>\n");
        return -1;
    }

    bool customLocation = false;
    bool centreMapX = false;
    bool centreMapY = false;
    sint32 resolutionWidth, resolutionHeight, customX = 0, customY = 0, customZoom, customRotation = 0;

    const char *inputPath = argv[0];
    const char *outputPath = argv[1];
    if (giantScreenshot) {
        resolutionWidth = 0;
        resolutionHeight = 0;
        customLocation = true;
        centreMapX = true;
        centreMapY = true;
        customZoom = atoi(argv[3]);
        customRotation = atoi(argv[4]) & 3;
    } else {
        resolutionWidth = atoi(argv[2]);
        resolutionHeight = atoi(argv[3]);
        if (argc == 8) {
            customLocation = true;
            if (argv[4][0] == 'c')
                centreMapX = true;
            else
                customX = atoi(argv[4]);
            if (argv[5][0] == 'c')
                centreMapY = true;
            else
                customY = atoi(argv[5]);

            customZoom = atoi(argv[6]);
            customRotation = atoi(argv[7]) & 3;
        } else {
            customZoom = 0;
        }
    }

    gOpenRCT2Headless = true;
    auto context = CreateContext();
    if (context->Initialise())
    {
        drawing_engine_init();
        context->LoadParkFromFile(inputPath);

        gIntroState = INTRO_STATE_NONE;
        gScreenFlags = SCREEN_FLAGS_PLAYING;

        sint32 mapSize = gMapSize;
        if (resolutionWidth == 0 || resolutionHeight == 0) {
            resolutionWidth = (mapSize * 32 * 2) >> customZoom;
            resolutionHeight = (mapSize * 32 * 1) >> customZoom;

            resolutionWidth += 8;
            resolutionHeight += 128;
        }

        rct_viewport viewport;
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = resolutionWidth;
        viewport.height = resolutionHeight;
        viewport.view_width = viewport.width;
        viewport.view_height = viewport.height;
        viewport.var_11 = 0;
        viewport.flags = 0;

        if (customLocation) {
            if (centreMapX)
                customX = (mapSize / 2) * 32 + 16;
            if (centreMapY)
                customY = (mapSize / 2) * 32 + 16;

            sint32 x = 0, y = 0;
            sint32 z = tile_element_height(customX, customY) & 0xFFFF;
            switch (customRotation) {
            case 0:
                x = customY - customX;
                y = ((customX + customY) / 2) - z;
                break;
            case 1:
                x = -customY - customX;
                y = ((-customX + customY) / 2) - z;
                break;
            case 2:
                x = -customY + customX;
                y = ((-customX - customY) / 2) - z;
                break;
            case 3:
                x = customY + customX;
                y = ((customX - customY) / 2) - z;
                break;
            }

            viewport.view_x = x - ((viewport.view_width << customZoom) / 2);
            viewport.view_y = y - ((viewport.view_height << customZoom) / 2);
            viewport.zoom = customZoom;
            gCurrentRotation = customRotation;
        } else {
            viewport.view_x = gSavedViewX - (viewport.view_width / 2);
            viewport.view_y = gSavedViewY - (viewport.view_height / 2);
            viewport.zoom = gSavedViewZoom;
            gCurrentRotation = gSavedViewRotation;
        }

        // Ensure sprites appear regardless of rotation
        reset_all_sprite_quadrant_placements();

        rct_drawpixelinfo dpi;
        dpi.x = 0;
        dpi.y = 0;
        dpi.width = resolutionWidth;
        dpi.height = resolutionHeight;
        dpi.pitch = 0;
        dpi.zoom_level = 0;
        dpi.bits = (uint8 *)malloc(dpi.width * dpi.height);

        viewport_render(&dpi, &viewport, 0, 0, viewport.width, viewport.height);

        rct_palette renderedPalette;
        screenshot_get_rendered_palette(&renderedPalette);

        image_io_png_write(&dpi, &renderedPalette, outputPath);

        free(dpi.bits);
        drawing_engine_dispose();
    }
    delete context;
    return 1;
}

}
