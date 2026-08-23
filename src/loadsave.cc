#include "loadsave.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <algorithm>

#include "art.h"
#include "automap.h"
#include "character_editor.h"
#include "color.h"
#include "combat.h"
#include "combat_ai.h"
#include "config.h"
#include "critter.h"
#include "cycle.h"
#include "db.h"
#include "dbox.h"
#include "debug.h"
#include "delay.h"
#include "display_monitor.h"
#include "draw.h"
#include "file_utils.h"
#include "game.h"
#include "game_mouse.h"
#include "game_movie.h"
#include "game_sound.h"
#include "geometry.h"
#include "input.h"
#include "interface.h"
#include "item.h"
#include "light.h"
#include "kb.h"
#include "map.h"
#include "memory.h"
#include "message.h"
#include "mouse.h"
#include "object.h"
#include "palette.h"
#include "party_member.h"
#include "perk.h"
#include "pipboy.h"
#include "platform_compat.h"
#include "preferences.h"
#include "proto.h"
#include "queue.h"
#include "random.h"
#include "scripts.h"
#include "settings.h"
#include "sfall_arrays.h"
#include "sfall_callbacks.h"
#include "sfall_config.h"
#include "sfall_ext.h"
#include "sfall_global_scripts.h"
#include "sfall_global_vars.h"
#include "sfall_metarules.h"
#include "sfall_opcodes.h"
#include "skill.h"
#include "stat.h"
#include "svga.h"
#include "text_font.h"
#include "tile.h"
#include "trait.h"
#include "version.h"
#include "window_manager.h"
#include "word_wrap.h"
#include "worldmap.h"
#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

namespace fallout {

#define LOAD_SAVE_SIGNATURE "FALLOUT SAVE FILE"
#define LOAD_SAVE_DESCRIPTION_LENGTH 30
// C-04 (HIGH): Number of handler chunks in the current (1.4R) save format.
// The 1.4R layout adds lightSave/lightLoad at index 26 (28 chunks total).
#define LOAD_SAVE_HANDLER_COUNT 28
// Number of handler chunks in the legacy (1.2R/1.3R) save format. Pre-pass-15
// builds wrote 27 chunks with _EndLoad at index 26 (no lightSave/lightLoad).
#define LOAD_SAVE_LEGACY_HANDLER_COUNT 27

// SFALL: Minimum save file version that includes CRC32 checksums on each
// handler chunk. Version 1.3+ saves have a 4-byte CRC prefix per handler
// chunk; version 1.2 saves (original format) have no CRC and load without
// verification. Both versions have the same major=1 and release='R'.
#define SAVE_FORMAT_CRC_VERSION_MAJOR (3)

// C-03/C-04 (save-format pass): On-disk versionMajor stores VERSION_MINOR.
//   versionMajor == 2 (1.2R): no handler CRC, no header CRC, 27 chunks.
//   versionMajor == 3 (1.3R): handler CRC; header CRC absent for pass-7..10
//                             saves (the 4 bytes after the header are
//                             handler-0's zero placeholder), garbage
//                             crc32-of-zeros for pass-11+ saves (unrecoverable,
//                             dropped); 27 chunks.
//   versionMajor == 4 (1.4R): handler CRC + header CRC (verified
//                             unconditionally); 28 chunks (lightSave/lightLoad
//                             at index 26).
#define SAVE_FORMAT_28_HANDLERS_VERSION_MAJOR (4)

// CRC-32 (IEEE 802.3) table — initialized lazily on first use.
static unsigned int _crc32Table[256];
static bool _crc32TableInit = false;

static void _crc32Init()
{
    if (_crc32TableInit) return;
    for (unsigned int i = 0; i < 256; i++) {
        unsigned int crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1));
        }
        _crc32Table[i] = crc;
    }
    _crc32TableInit = true;
}

static unsigned int _crc32Compute(const unsigned char* data, size_t len)
{
    _crc32Init();
    unsigned int crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc = _crc32Table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

#define LSGAME_MSG_NAME "LSGAME.MSG"

#define LS_WINDOW_WIDTH 640
#define LS_WINDOW_HEIGHT 480

#define LS_PREVIEW_WIDTH 224
#define LS_PREVIEW_HEIGHT 133
#define LS_PREVIEW_SIZE ((LS_PREVIEW_WIDTH) * (LS_PREVIEW_HEIGHT))

#define LS_COMMENT_WINDOW_X 169
#define LS_COMMENT_WINDOW_Y 116

// NOTE: The following are "normalized" path components for "proto/critters" and
// "proto/items". The original code does not use uniform case for them (as
// opposed to other path components like MAPS, SAVE.DAT, etc). It does not have
// effect on Windows, but it's important on Linux and Mac, where filesystem is
// case-sensitive. Lowercase is preferred as it is used in other parts of the
// codebase (see `protoInit`, `gArtListDescriptions`).

#define PROTO_DIR_NAME "proto"
#define CRITTERS_DIR_NAME "critters"
#define ITEMS_DIR_NAME "items"
#define PROTO_FILE_EXT "pro"

typedef int LoadGameHandler(File* stream);
typedef int SaveGameHandler(File* stream);

typedef enum LoadSaveWindowType {
    LOAD_SAVE_WINDOW_TYPE_SAVE_GAME,
    LOAD_SAVE_WINDOW_TYPE_PICK_QUICK_SAVE_SLOT,
    LOAD_SAVE_WINDOW_TYPE_LOAD_GAME,
    LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU,
    LOAD_SAVE_WINDOW_TYPE_PICK_QUICK_LOAD_SLOT,
} LoadSaveWindowType;

typedef enum LoadSaveSlotState {
    SLOT_STATE_EMPTY,
    SLOT_STATE_OCCUPIED,
    SLOT_STATE_ERROR,
    SLOT_STATE_UNSUPPORTED_VERSION,
} LoadSaveSlotState;

typedef enum LoadSaveScrollDirection {
    LOAD_SAVE_SCROLL_DIRECTION_NONE,
    LOAD_SAVE_SCROLL_DIRECTION_UP,
    LOAD_SAVE_SCROLL_DIRECTION_DOWN,
} LoadSaveScrollDirection;

static void loadSaveMessageListReset();

typedef struct LoadSaveSlotData {
    char signature[24];
    // NOTE: These field names are swapped relative to their semantics:
    // versionMinor stores VERSION_MAJOR, versionMajor stores VERSION_MINOR.
    // Both save (lsgSaveHeaderInSlot) and load (lsgLoadHeaderInSlot) use
    // the identical mapping, so the on-disk format is consistent.
    short versionMinor;
    short versionMajor;
    // TODO: The type is probably char, but it's read with the same function as
    // reading unsigned chars, which in turn probably result of collapsing
    // reading functions.
    unsigned char versionRelease;
    char characterName[32];
    char description[LOAD_SAVE_DESCRIPTION_LENGTH];
    short fileMonth;
    short fileDay;
    short fileYear;
    int fileTime;
    short gameMonth;
    short gameDay;
    short gameYear;
    unsigned int gameTime;
    short elevation;
    short map;
    char fileName[16];
} LoadSaveSlotData;

typedef enum LoadSaveFrm {
    LOAD_SAVE_FRM_BACKGROUND,
    LOAD_SAVE_FRM_BOX,
    LOAD_SAVE_FRM_PREVIEW_COVER,
    LOAD_SAVE_FRM_RED_BUTTON_PRESSED,
    LOAD_SAVE_FRM_RED_BUTTON_NORMAL,
    LOAD_SAVE_FRM_ARROW_DOWN_NORMAL,
    LOAD_SAVE_FRM_ARROW_DOWN_PRESSED,
    LOAD_SAVE_FRM_ARROW_UP_NORMAL,
    LOAD_SAVE_FRM_ARROW_UP_PRESSED,
    LOAD_SAVE_FRM_COUNT,
} LoadSaveFrm;

static int _QuickSnapShot();
static int lsgWindowInit(int windowType);
static int lsgWindowFree(int windowType);
static int lsgPerformSaveGame();
static int lsgLoadGameInSlot(int slot);
static int lsgSaveHeaderInSlot(int slot);
static int lsgLoadHeaderInSlot(int slot);
static int _GetSlotList();
static void _ShowSlotList(int windowType);
static void _DrawInfoBox(int slot);
static int _LoadTumbSlot(int slot);
static int _GetComment(int slot);
static int _get_input_str2(int win, int doneKeyCode, int cancelKeyCode, char* description, int maxLength, int x, int y, int textColor, int backgroundColor, int flags);
static int _DummyFunc(File* stream);
static int _PrepLoad(File* stream);
static int _EndLoad(File* stream);
static int _GameMap2Slot(File* stream);
static int _SlotMap2Game(File* stream);
static int _mygets(char* dest, File* stream);
static int _copy_file(const char* existingFileName, const char* newFileName);
static int _SaveBackup();
static int _RestoreSave();
static int _LoadObjDudeCid(File* stream);
static int _SaveObjDudeCid(File* stream);
static int _EraseSave();

// 0x47B7C0 lsgrphs
static const int gLoadSaveFrmIds[LOAD_SAVE_FRM_COUNT] = {
    237, // lsgame.frm - load/save game
    238, // lsgbox.frm - load/save game
    239, // lscover.frm - load/save game
    9, // lilreddn.frm - little red button down
    8, // lilredup.frm - little red button up
    181, // dnarwoff.frm - character editor
    182, // dnarwon.frm - character editor
    199, // uparwoff.frm - character editor
    200, // uparwon.frm - character editor
};

// Control max number of save/load pages.
// Page count is initialized from sfall config in _InitLoadSave():
//   gExtraSaveSlots=true  → 100 pages (1000 slots, extended, matches sfall 4.5)
//   gExtraSaveSlots=false → 1 page   (10 slots,   FO1/FO2 default)
// NOTE: Increasing beyond 1000 would require updating kMaxSaveTotalSlots and the
// compile-time array allocations (_LSData, _LSstatus).
constexpr int kMaxSaveTotalSlots = 1000;
int saveLoadPages = 10;
constexpr int slotsPerPage = 10;
int saveLoadTotalSlots = saveLoadPages * slotsPerPage;
constexpr int kLoadSaveActionDone = 500;

// Global variable to track the current slot page
static int _currentSlotPage = 0;

// 0x5193B8 slot_cursor
static int _slot_cursor = 0;

static int gDevLoadGameSlot = -1;

// 0x5193BC quick_done
static bool _quick_done = false;

// 0x5193C0 bk_enable_3
static bool gLoadSaveWindowIsoWasEnabled = false;

// 0x5193C4 map_backup_count
static int _map_backup_count = -1;

// 0x5193C8 automap_db_flag
static bool _automap_db_flag = false;

// 0x5193CC patches
static const char* _patches = nullptr;

// 0x5193EC master_save_list
static SaveGameHandler* _master_save_list[LOAD_SAVE_HANDLER_COUNT] = {
    _DummyFunc,
    _SaveObjDudeCid,
    scriptsSaveGameGlobalVars,
    _GameMap2Slot,
    scriptsSaveGameGlobalVars,
    _obj_save_dude,
    critterSave,
    killsSave,
    skillsSave,
    randomSave,
    perksSave,
    combatSave,
    aiSave,
    statsSave,
    itemsSave,
    traitsSave,
    automapSave,
    preferencesSave,
    characterEditorSave,
    wmWorldMap_save,
    pipboySave,
    gameMoviesSave,
    skillsUsageSave,
    partyMembersSave,
    queueSave,
    interfaceSave,
    lightSave,
    _DummyFunc,
};

// 0x519458 master_load_list
static LoadGameHandler* _master_load_list[LOAD_SAVE_HANDLER_COUNT] = {
    _PrepLoad,
    _LoadObjDudeCid,
    scriptsLoadGameGlobalVars,
    _SlotMap2Game,
    scriptsSkipGameGlobalVars,
    _obj_load_dude,
    critterLoad,
    killsLoad,
    skillsLoad,
    randomLoad,
    perksLoad,
    combatLoad,
    aiLoad,
    statsLoad,
    itemsLoad,
    traitsLoad,
    automapLoad,
    preferencesLoad,
    characterEditorLoad,
    wmWorldMap_load,
    pipboyLoad,
    gameMoviesLoad,
    skillsUsageLoad,
    partyMembersLoad,
    queueLoad,
    interfaceLoad,
    lightLoad,
    _EndLoad,
};

// C-04 (HIGH): Legacy 27-chunk load handler list (save formats 1.2R/1.3R,
// on-disk versionMajor 2/3). Identical to _master_load_list for indices
// 0..25, but index 26 is _EndLoad instead of lightLoad — pre-pass-15 builds
// had no lightSave/lightLoad handler, so a 27-chunk save written before that
// change ends at interfaceLoad, and _EndLoad reads no data. The 28-chunk
// 1.4R list above must be used only for versionMajor >= 4 saves; using it on
// a 27-chunk save would run lightLoad at index 26 and read garbage/EOF.
static LoadGameHandler* _master_load_list_legacy[LOAD_SAVE_LEGACY_HANDLER_COUNT] = {
    _PrepLoad,
    _LoadObjDudeCid,
    scriptsLoadGameGlobalVars,
    _SlotMap2Game,
    scriptsSkipGameGlobalVars,
    _obj_load_dude,
    critterLoad,
    killsLoad,
    skillsLoad,
    randomLoad,
    perksLoad,
    combatLoad,
    aiLoad,
    statsLoad,
    itemsLoad,
    traitsLoad,
    automapLoad,
    preferencesLoad,
    characterEditorLoad,
    wmWorldMap_load,
    pipboyLoad,
    gameMoviesLoad,
    skillsUsageLoad,
    partyMembersLoad,
    queueLoad,
    interfaceLoad,
    _EndLoad,
};

// 0x5194C4 loadingGame
static bool _loadingGame = false;

// M-62 (MEDIUM): Version gate for the gScriptWorldMapMulti save field.
// Defined in worldmap.cc (defaults to fork format). loadsave.cc MUST set this
// to the loaded save's versionMajor before the handler loop so wmWorldMap_load
// reads the fork-added float only for fork saves (versionMajor >= 3) and skips
// it for upstream/vanilla 1.2R saves (versionMajor == 2) — otherwise the first
// city's x is consumed as the float and the whole worldmap stream misaligns.
// On the save side we set it to the current format (VERSION_MINOR == 4) so the
// write path is always fork-format.
extern int gLoadedSaveVersionMajor;

static int _loadingMapId = -1;

// lsgame.msg
//
// 0x613D28 lsgame_msgfl
static MessageList gLoadSaveMessageList;

// 0x613D30 LSData
static LoadSaveSlotData _LSData[kMaxSaveTotalSlots];

// 0x614280 LSstatus
static int _LSstatus[kMaxSaveTotalSlots];

// 0x6142A8 thumbnail_image
static unsigned char* _thumbnail_image;

// 0x6142AC snapshotBuf
static unsigned char* _snapshotBuf;

// 0x6142B0 lsgmesg
static MessageListItem gLoadSaveMessageListItem;

// 0x6142C0 dbleclkcntr
static int _dbleclkcntr;

// 0x6142C4 lsgwin
static int gLoadSaveWindow;

// 0x6142EC snapshot
static unsigned char* _snapshot;

// 0x6142F0 str2
static char _str2[COMPAT_MAX_PATH];

// 0x6143F4 str0
static char _str0[COMPAT_MAX_PATH];

// 0x6144F8 str1
static char _str1[COMPAT_MAX_PATH];

// 0x6145FC str
static char _str[COMPAT_MAX_PATH];

static void loadSaveMessageListReset()
{
    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_LSGAME, nullptr);
    messageListFree(&gLoadSaveMessageList);
}

// 0x614700 lsgbuf
static unsigned char* gLoadSaveWindowBuffer;

// 0x614704 gmpath
static char _gmpath[COMPAT_MAX_PATH];

// 0x614808 flptr
static File* _flptr;

// 0x61480C ls_error_code
static int _ls_error_code;

// 0x614810 fontsave_2
static int gLoadSaveWindowOldFont;

static FrmImage _loadsaveFrmImages[LOAD_SAVE_FRM_COUNT];

static int quickSaveSlots = 0;
static bool autoQuickSaveSlots = false;

static constexpr char kLoadSaveSlotDataSection[] = "POSITION";
static constexpr char kLoadSaveSlotDataKey[] = "CurrentSlot";
static constexpr char kLoadSaveSlotDataFile[] = "SAVEGAME\\slotdat.ini";

static void loadSaveRememberSelectedSlot()
{
    assert(_patches != nullptr);

    _slot_cursor = 0;
    _currentSlotPage = 0;

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s\\%s", _patches, kLoadSaveSlotDataFile);

    int slot = 0;
    if (ScopedConfig config { path, false }; config) {
        configGetInt(config.get(), kLoadSaveSlotDataSection, kLoadSaveSlotDataKey, &slot, 0);
    }

    _slot_cursor = std::clamp(slot, 0, saveLoadTotalSlots - 1);
    _currentSlotPage = _slot_cursor / slotsPerPage;
}

static void loadSaveSetCurrentPage(int page)
{
    int slotIndex = _slot_cursor % slotsPerPage;

    _currentSlotPage = std::clamp(page, 0, saveLoadPages - 1);
    _slot_cursor = std::min(_currentSlotPage * slotsPerPage + slotIndex, saveLoadTotalSlots - 1);
}

static void loadSavePersistSelectedSlot()
{
    assert(_patches != nullptr);

    char savegamePath[COMPAT_MAX_PATH];
    snprintf(savegamePath, sizeof(savegamePath), "%s\\SAVEGAME", _patches);
    compat_mkdir(savegamePath);

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s\\%s", _patches, kLoadSaveSlotDataFile);

    ScopedConfig config { path, false };
    if (!config.isInitialized()) {
        return;
    }

    configSetInt(config.get(), kLoadSaveSlotDataSection, kLoadSaveSlotDataKey, _slot_cursor);
    configWrite(config.get(), path, false);
}

// 0x47B7E4
void _InitLoadSave()
{
    _quick_done = false;
    _patches = settings.system.master_patches_path.c_str();

    // Initialize save slot page count from sfall config.
    // gExtraSaveSlots=true  → 100 pages (1000 slots, extended, matches sfall 4.5)
    // gExtraSaveSlots=false → 1 page   (10 slots,   FO1/FO2 default)
    saveLoadPages = gExtraSaveSlots ? 100 : 1;
    saveLoadTotalSlots = saveLoadPages * slotsPerPage;

    loadSaveRememberSelectedSlot();

    MapDirErase("MAPS\\", "SAV");
    MapDirErase(PROTO_DIR_NAME "\\" CRITTERS_DIR_NAME "\\", PROTO_FILE_EXT);
    MapDirErase(PROTO_DIR_NAME "\\" ITEMS_DIR_NAME "\\", PROTO_FILE_EXT);

    quickSaveSlots = settings.ui.auto_quick_save;
    if (quickSaveSlots > 0 && quickSaveSlots <= saveLoadTotalSlots) {
        autoQuickSaveSlots = true;
    }
}

// 0x47B85C
void _ResetLoadSave()
{
    MapDirErase("MAPS\\", "SAV");
    MapDirErase(PROTO_DIR_NAME "\\" CRITTERS_DIR_NAME "\\", PROTO_FILE_EXT);
    MapDirErase(PROTO_DIR_NAME "\\" ITEMS_DIR_NAME "\\", PROTO_FILE_EXT);
}

// SaveGame
// 0x47B88C
int lsgSaveGame(int mode)
{
    ScopedGameMode gm(GameMode::kSaveGame);

    MessageListItem messageListItem;

    _ls_error_code = 0;
    _patches = settings.system.master_patches_path.c_str();
    // NOTE: compat_mkdir calls construct paths with _patches prefix
    // (e.g. "_patches\SAVEGAME\SLOT01"), while fileOpen calls use
    // relative paths without the prefix (e.g. "SAVEGAME\SLOT01\").
    // Both resolve to the same location: fileOpen resolves relative
    // paths through the directory xbase registered from _patches
    // (see xfile.cc directory xbase resolution loop).

    // SFALL: skip slot selection if auto quicksave is enabled
    if (autoQuickSaveSlots) {
        _quick_done = true;
    }

    if (mode == LOAD_SAVE_MODE_QUICK && _quick_done) {
        // SFALL: cycle through first N slots for quicksaving
        if (autoQuickSaveSlots) {
            if (++_slot_cursor >= quickSaveSlots) {
                _slot_cursor = 0;
            }
        }
        snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
        strcat(_gmpath, "SAVE.DAT");

        _flptr = fileOpen(_gmpath, "rb");
        if (_flptr != nullptr) {
            lsgLoadHeaderInSlot(_slot_cursor);
            fileClose(_flptr);
        }

        if (!messageListInit(&gLoadSaveMessageList)) {
            return -1;
        }

        char path[COMPAT_MAX_PATH];
        snprintf(path, sizeof(path), "%s%s", asc_5186C8, "LSGAME.MSG");
        if (!messageListLoad(&gLoadSaveMessageList, path)) {
            return -1;
        }
        messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_LSGAME, &gLoadSaveMessageList);

        _snapshotBuf = nullptr;
        int v6 = _QuickSnapShot();
        if (v6 == 1) {
            int v7 = lsgPerformSaveGame();
            if (v7 != -1) {
                v6 = v7;
            } else {
                v6 = -1;
            }
        }

        if (_snapshotBuf != nullptr) {
            internal_free(_snapshot);
        }

        gameMouseSetCursor(MOUSE_CURSOR_ARROW);

        if (v6 != -1) {
            loadSaveMessageListReset();
            return 1;
        }

        soundPlayFile("iisxxxx1");

        // Error saving game!
        strcpy(_str0, getmsg(&gLoadSaveMessageList, &messageListItem, 132));
        // Unable to save game.
        strcpy(_str1, getmsg(&gLoadSaveMessageList, &messageListItem, 133));

        const char* body[] = {
            _str1,
        };
        showDialogBox(_str0, body, 1, 169, 116, COLOR_AMBER, nullptr, COLOR_AMBER, DIALOG_BOX_LARGE);

        loadSaveMessageListReset();

        return -1;
    }

    touch_set_touchscreen_mode(mode == LOAD_SAVE_MODE_NORMAL);

    _quick_done = false;

    int windowType = mode == LOAD_SAVE_MODE_QUICK
        ? LOAD_SAVE_WINDOW_TYPE_PICK_QUICK_SAVE_SLOT
        : LOAD_SAVE_WINDOW_TYPE_SAVE_GAME;
    if (lsgWindowInit(windowType) == -1) {
        debugPrint("\nLOADSAVE: ** Error loading save game screen data! **\n");
        return -1;
    }

    pipboyMessageListInit();

    if (_GetSlotList() == -1) {
        windowRefresh(gLoadSaveWindow);

        soundPlayFile("iisxxxx1");

        // Error loading save game list!
        strcpy(_str0, getmsg(&gLoadSaveMessageList, &messageListItem, 106));
        // Save game directory:
        strcpy(_str1, getmsg(&gLoadSaveMessageList, &messageListItem, 107));

        snprintf(_str2, sizeof(_str2), "\"%s\\\"", "SAVEGAME");

        // TODO: Check.
        strcpy(_str2, getmsg(&gLoadSaveMessageList, &messageListItem, 108));

        const char* body[] = {
            _str1,
            _str2,
        };
        showDialogBox(_str0, body, 2, 169, 116, COLOR_AMBER, nullptr, COLOR_AMBER, DIALOG_BOX_LARGE);

        lsgWindowFree(0);

        return -1;
    }

    switch (_LSstatus[_slot_cursor]) {
    case SLOT_STATE_EMPTY:
    case SLOT_STATE_ERROR:
    case SLOT_STATE_UNSUPPORTED_VERSION:
        blitBufferToBuffer(_snapshotBuf,
            LS_PREVIEW_WIDTH - 1,
            LS_PREVIEW_HEIGHT - 1,
            LS_PREVIEW_WIDTH,
            gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366,
            LS_WINDOW_WIDTH);
        break;
    default:
        _LoadTumbSlot(_slot_cursor);
        blitBufferToBuffer(_thumbnail_image,
            LS_PREVIEW_WIDTH - 1,
            LS_PREVIEW_HEIGHT - 1,
            LS_PREVIEW_WIDTH,
            gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366,
            LS_WINDOW_WIDTH);
        break;
    }

    _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_SAVE_GAME);
    _DrawInfoBox(_slot_cursor);
    windowRefresh(gLoadSaveWindow);

    _dbleclkcntr = 24;

    int rc = -1;
    int doubleClickSlot = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        unsigned int tick = getTicks();
        int keyCode = inputGetInput();
        bool selectionChanged = false;
        int scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_NONE;

        convertMouseWheelToArrowKey(&keyCode);

        if (keyCode == KEY_ESCAPE || keyCode == 501 || _game_user_wants_to_quit != GAME_QUIT_REQUEST_NONE) {
            rc = 0;
        } else {
            switch (keyCode) {
            case KEY_ARROW_UP:
                if (_slot_cursor > 0) { // Prevent going below 0
                    if (_slot_cursor % 10 == 0 && _currentSlotPage > 0) {
                        // Move to the previous page and set cursor to the last slot on that page
                        _currentSlotPage--;
                        _slot_cursor--;
                        _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_SAVE_GAME);
                        windowRefresh(gLoadSaveWindow);
                    } else {
                        // Normal movement within the page
                        _slot_cursor--;
                    }
                }

                selectionChanged = true;
                doubleClickSlot = -1;
                break;

            case KEY_ARROW_DOWN:
                if (_slot_cursor < (saveLoadTotalSlots - 1)) { // Prevent going above 99
                    if (_slot_cursor % 10 == 9 && _currentSlotPage < (saveLoadTotalSlots / 10) - 1) {
                        // Move to the next page and set cursor to the first slot on that page
                        _currentSlotPage++;
                        _slot_cursor++;
                        _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_SAVE_GAME);
                        windowRefresh(gLoadSaveWindow);
                    } else {
                        // Normal movement within the page
                        _slot_cursor++;
                    }
                }

                selectionChanged = true;
                doubleClickSlot = -1;
                break;
            case KEY_HOME:
                // Move to the first slot of the current page
                _slot_cursor = _currentSlotPage * 10;
                selectionChanged = true;
                doubleClickSlot = -1;
                break;

            case KEY_END:
                // Move to the last slot of the current page
                _slot_cursor = (_currentSlotPage * 10) + 9;

                // Prevent overflow on the last page
                if (_slot_cursor > (saveLoadTotalSlots - 1)) {
                    _slot_cursor = (saveLoadTotalSlots - 1);
                }

                selectionChanged = true;
                doubleClickSlot = -1;
                break;
            case 506:
                scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_UP;
                break;

            case 504:
                scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_DOWN;
                break;

            case KEY_ARROW_RIGHT:
            case KEY_ARROW_LEFT:
            case 502: { // Mouse click detected
                int mouseX, mouseY;
                mouseGetPositionInWindow(gLoadSaveWindow, &mouseX, &mouseY);

                // Check if the click was in the "Next Page" button area
                if ((mouseX >= 195 && mouseX <= 280 && mouseY >= 425 && mouseY <= 435) || keyCode == KEY_ARROW_RIGHT) { // Next Page coordinates
                    if (_currentSlotPage < (saveLoadTotalSlots / 10) - 1) { // Max 10 pages (0-9)
                        soundPlayFile("ib1p1xx1");
                        loadSaveSetCurrentPage(_currentSlotPage + 1);
                        selectionChanged = true;
                        doubleClickSlot = -1;
                        _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_SAVE_GAME);
                        windowRefresh(gLoadSaveWindow);
                    }
                    break;
                }

                // Check if the click was in the "Previous Page" button area
                if ((mouseX >= 55 && mouseX <= 180 && mouseY >= 425 && mouseY <= 435) || keyCode == KEY_ARROW_LEFT) { // Previous Page coordinates
                    if (_currentSlotPage > 0) {
                        soundPlayFile("ib1p1xx1");
                        loadSaveSetCurrentPage(_currentSlotPage - 1);
                        selectionChanged = true;
                        doubleClickSlot = -1;
                        _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_SAVE_GAME);
                        windowRefresh(gLoadSaveWindow);
                    }
                    break;
                }

                // Calculate the clicked slot, adjusting for pagination
                int relativeSlot = (mouseY - 79) / (3 * fontGetLineHeight() + 4);
                if (relativeSlot < 0) {
                    relativeSlot = 0;
                } else if (relativeSlot > 9) {
                    relativeSlot = 9;
                }

                // Adjust for the current page
                int clickedSlot = (_currentSlotPage * 10) + relativeSlot;

                if (clickedSlot > (saveLoadTotalSlots - 1)) { // Ensure we don't go beyond max slots
                    clickedSlot = (saveLoadTotalSlots - 1);
                }

                _slot_cursor = clickedSlot;
                if (clickedSlot == doubleClickSlot) {
                    keyCode = kLoadSaveActionDone;
                    soundPlayFile("ib1p1xx1");
                }

                selectionChanged = true;
                doubleClickSlot = _slot_cursor;
                scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_NONE;
            } break;

            case KEY_CTRL_Q:
            case KEY_CTRL_X:
            case KEY_F10:
                showQuitConfirmationDialog();

                if (_game_user_wants_to_quit != GAME_QUIT_REQUEST_NONE) {
                    rc = 0;
                }
                break;
            case KEY_PLUS:
            case KEY_EQUAL:
                brightnessIncrease();
                break;
            case KEY_MINUS:
            case KEY_UNDERSCORE:
                brightnessDecrease();
                break;
            case KEY_RETURN:
                keyCode = kLoadSaveActionDone;
                break;
            }
        }

        if (keyCode == kLoadSaveActionDone) {
            if (_LSstatus[_slot_cursor] == SLOT_STATE_OCCUPIED) {
                rc = 1;
                // Save game already exists, overwrite?
                const char* title = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 131);
                if (showDialogBox(title, nullptr, 0, 169, 131, COLOR_AMBER, nullptr, COLOR_AMBER, DIALOG_BOX_YES_NO) == 0) {
                    rc = -1;
                }
            } else {
                rc = 1;
            }

            selectionChanged = true;
            scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_NONE;
        }

        if (scrollDirection) {
            unsigned int scrollVelocity = 4;
            bool isScrolling = false;
            int scrollCounter = 0;
            do {
                sharedFpsLimiter.mark();

                unsigned int start = getTicks();
                scrollCounter += 1;

                if ((!isScrolling && scrollCounter == 1) || (isScrolling && scrollCounter > 14.4)) {
                    isScrolling = true;

                    if (scrollCounter > 14.4) {
                        scrollVelocity += 1;
                        if (scrollVelocity > 24) {
                            scrollVelocity = 24;
                        }
                    }
                    // handle scrolling between pages via buttons
                    if (scrollDirection == LOAD_SAVE_SCROLL_DIRECTION_UP) {
                        _slot_cursor--;

                        // If moving up past the first slot of the page, go to the previous page
                        if (_slot_cursor < _currentSlotPage * 10) {
                            if (_currentSlotPage > 0) {
                                _currentSlotPage--;
                                _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_LOAD_GAME);
                                windowRefresh(gLoadSaveWindow);
                                _slot_cursor = (_currentSlotPage * 10) + 9; // Move to the last slot of the previous page
                            } else {
                                _slot_cursor = 0; // Prevent underflow
                            }
                        }
                    } else { // LOAD_SAVE_SCROLL_DIRECTION_DOWN
                        _slot_cursor++;

                        // If moving down past the last slot of the page, go to the next page
                        if (_slot_cursor > (_currentSlotPage * 10) + 9) {
                            if (_currentSlotPage < (saveLoadTotalSlots / 10) - 1) { // Max pages: 0-9
                                _currentSlotPage++;
                                _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_LOAD_GAME);
                                windowRefresh(gLoadSaveWindow);
                                _slot_cursor = _currentSlotPage * 10; // Move to the first slot of the next page
                            } else {
                                _slot_cursor = (saveLoadTotalSlots - 1); // Prevent overflow (last slot overall)
                            }
                        }
                    }

                    // TODO: Does not check for unsupported version error like
                    // other switches do.
                    // fixed to match load screen 'thumbnail'/'blank' updating
                    switch (_LSstatus[_slot_cursor]) {
                    case SLOT_STATE_EMPTY:
                    case SLOT_STATE_ERROR:
                        blitBufferToBuffer(_loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getData(),
                            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
                            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getHeight(),
                            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
                            gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 39 + 340,
                            LS_WINDOW_WIDTH);
                        break;
                    default:
                        _LoadTumbSlot(_slot_cursor);
                        blitBufferToBuffer(_loadsaveFrmImages[LOAD_SAVE_FRM_BACKGROUND].getData() + LS_WINDOW_WIDTH * 39 + 340,
                            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
                            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getHeight(),
                            LS_WINDOW_WIDTH,
                            gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 39 + 340,
                            LS_WINDOW_WIDTH);
                        blitBufferToBuffer(_thumbnail_image,
                            LS_PREVIEW_WIDTH - 1,
                            LS_PREVIEW_HEIGHT - 1,
                            LS_PREVIEW_WIDTH,
                            gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366,
                            LS_WINDOW_WIDTH);
                        break;
                    }

                    _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_SAVE_GAME);
                    _DrawInfoBox(_slot_cursor);
                    windowRefresh(gLoadSaveWindow);
                }

                if (scrollCounter > 14.4) {
                    delay_ms(1000 / scrollVelocity - (getTicks() - start));
                } else {
                    delay_ms(1000 / 24 - (getTicks() - start));
                }

                keyCode = inputGetInput();

                renderPresent();
                sharedFpsLimiter.throttle();
            } while (keyCode != 505 && keyCode != 503);
        } else {
            if (selectionChanged) {
                // fixed to match load screen 'thumbnail'/'blank' updating
                switch (_LSstatus[_slot_cursor]) {
                case SLOT_STATE_EMPTY:
                case SLOT_STATE_ERROR:
                case SLOT_STATE_UNSUPPORTED_VERSION:
                    blitBufferToBuffer(_loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getData(),
                        _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
                        _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getHeight(),
                        _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
                        gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 39 + 340,
                        LS_WINDOW_WIDTH);
                    break;
                default:
                    _LoadTumbSlot(_slot_cursor);
                    blitBufferToBuffer(_loadsaveFrmImages[LOAD_SAVE_FRM_BACKGROUND].getData() + LS_WINDOW_WIDTH * 39 + 340,
                        _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
                        _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getHeight(),
                        LS_WINDOW_WIDTH,
                        gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 39 + 340,
                        LS_WINDOW_WIDTH);
                    blitBufferToBuffer(_thumbnail_image,
                        LS_PREVIEW_WIDTH - 1,
                        LS_PREVIEW_HEIGHT - 1,
                        LS_PREVIEW_WIDTH,
                        gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366,
                        LS_WINDOW_WIDTH);
                    break;
                }

                _DrawInfoBox(_slot_cursor);
                _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_SAVE_GAME);
            }

            windowRefresh(gLoadSaveWindow);

            _dbleclkcntr -= 1;
            if (_dbleclkcntr == 0) {
                _dbleclkcntr = 24;
                doubleClickSlot = -1;
            }

            delay_ms(1000 / 24 - (getTicks() - tick));
        }

        if (rc == 1) {
            int v50 = _GetComment(_slot_cursor);
            if (v50 == -1) {
                gameMouseSetCursor(MOUSE_CURSOR_ARROW);
                soundPlayFile("iisxxxx1");
                debugPrint("\nLOADSAVE: ** Error getting save file comment **\n");

                // Error saving game!
                strcpy(_str0, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 132));
                // Unable to save game.
                strcpy(_str1, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 133));

                const char* body[1] = {
                    _str1,
                };
                showDialogBox(_str0, body, 1, 169, 116, COLOR_AMBER, nullptr, COLOR_AMBER, DIALOG_BOX_LARGE);
                rc = -1;
            } else if (v50 == 0) {
                gameMouseSetCursor(MOUSE_CURSOR_ARROW);
                rc = -1;
            } else if (v50 == 1) {
                if (lsgPerformSaveGame() == -1) {
                    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
                    soundPlayFile("iisxxxx1");

                    // Error saving game!
                    strcpy(_str0, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 132));
                    // Unable to save game.
                    strcpy(_str1, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 133));

                    rc = -1;

                    const char* body[1] = {
                        _str1,
                    };
                    showDialogBox(_str0, body, 1, 169, 116, COLOR_AMBER, nullptr, COLOR_AMBER, DIALOG_BOX_LARGE);

                    if (_GetSlotList() == -1) {
                        windowRefresh(gLoadSaveWindow);
                        soundPlayFile("iisxxxx1");

                        // Error loading save agme list!
                        strcpy(_str0, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 106));
                        // Save game directory:
                        strcpy(_str1, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 107));

                        snprintf(_str2, sizeof(_str2), "\"%s\\\"", "SAVEGAME");

                        char text[260];
                        // Doesn't exist or is corrupted.
                        strcpy(text, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 107));

                        const char* body[2] = {
                            _str1,
                            _str2,
                        };
                        showDialogBox(_str0, body, 2, 169, 116, COLOR_AMBER, nullptr, COLOR_AMBER, DIALOG_BOX_LARGE);

                        lsgWindowFree(0);

                        return -1;
                    }
                    // fixed to match load screen 'thumbnail'/'blank' updating
                    switch (_LSstatus[_slot_cursor]) {
                    case SLOT_STATE_EMPTY:
                    case SLOT_STATE_ERROR:
                    case SLOT_STATE_UNSUPPORTED_VERSION:
                        blitBufferToBuffer(_loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getData(),
                            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
                            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getHeight(),
                            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
                            gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 39 + 340,
                            LS_WINDOW_WIDTH);
                        break;
                    default:
                        _LoadTumbSlot(_slot_cursor);
                        blitBufferToBuffer(_loadsaveFrmImages[LOAD_SAVE_FRM_BACKGROUND].getData() + LS_WINDOW_WIDTH * 39 + 340,
                            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
                            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getHeight(),
                            LS_WINDOW_WIDTH,
                            gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 39 + 340,
                            LS_WINDOW_WIDTH);
                        blitBufferToBuffer(_thumbnail_image,
                            LS_PREVIEW_WIDTH - 1,
                            LS_PREVIEW_HEIGHT - 1,
                            LS_PREVIEW_WIDTH,
                            gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366,
                            LS_WINDOW_WIDTH);
                        break;
                    }

                    _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_SAVE_GAME);
                    _DrawInfoBox(_slot_cursor);
                    windowRefresh(gLoadSaveWindow);
                    _dbleclkcntr = 24;
                }
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    lsgWindowFree(LOAD_SAVE_WINDOW_TYPE_SAVE_GAME);

    pipboyMessageListFree();

    tileWindowRefresh();

    if (mode == LOAD_SAVE_MODE_QUICK) {
        if (rc == 1) {
            _quick_done = true;
        }
    }

    return rc;
}

// 0x47C5B4
static int _QuickSnapShot()
{
    _snapshot = (unsigned char*)internal_malloc(LS_PREVIEW_SIZE);
    if (_snapshot == nullptr) {
        return -1;
    }

    bool gameMouseWasVisible = gameMouseObjectsIsVisible();
    if (gameMouseWasVisible) {
        gameMouseObjectsHide();
    }

    mouseHideCursor();
    tileWindowRefresh();
    mouseShowCursor();

    if (gameMouseWasVisible) {
        gameMouseObjectsShow();
    }

    // For preview take 640x380 area in the center of isometric window.
    Window* window = windowGetWindow(gIsoWindow);
    unsigned char* isoWindowBuffer = window->buffer
        + window->width * (window->height - ORIGINAL_ISO_WINDOW_HEIGHT) / 2
        + (window->width - ORIGINAL_ISO_WINDOW_WIDTH) / 2;
    blitBufferToBufferStretch(isoWindowBuffer,
        ORIGINAL_ISO_WINDOW_WIDTH,
        ORIGINAL_ISO_WINDOW_HEIGHT,
        windowGetWidth(gIsoWindow),
        _snapshot,
        LS_PREVIEW_WIDTH,
        LS_PREVIEW_HEIGHT,
        LS_PREVIEW_WIDTH);

    _snapshotBuf = _snapshot;

    return 1;
}

// LoadGame
// 0x47C640
int lsgLoadGame(int mode)
{
    ScopedGameMode gm(GameMode::kLoadGame);

    MessageListItem messageListItem;

    const char* body[] = {
        _str1,
        _str2,
    };

    _ls_error_code = 0;
    _patches = settings.system.master_patches_path.c_str();

    if (mode == LOAD_SAVE_MODE_QUICK && _quick_done) {
        int quickSaveWindowX = (screenGetWidth() - LS_WINDOW_WIDTH) / 2;
        int quickSaveWindowY = (screenGetHeight() - LS_WINDOW_HEIGHT) / 2;
        int window = windowCreate(quickSaveWindowX,
            quickSaveWindowY,
            LS_WINDOW_WIDTH,
            LS_WINDOW_HEIGHT,
            256,
            WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
        if (window != -1) {
            unsigned char* windowBuffer = windowGetBuffer(window);
            bufferFill(windowBuffer, LS_WINDOW_WIDTH, LS_WINDOW_HEIGHT, LS_WINDOW_WIDTH, COLOR_BLACK);
            windowRefresh(window);
            renderPresent();
        }

        if (lsgLoadGameInSlot(_slot_cursor) != -1) {
            if (window != -1) {
                windowDestroy(window);
            }
            gameMouseSetCursor(MOUSE_CURSOR_ARROW);
            return 1;
        }

        if (!messageListInit(&gLoadSaveMessageList)) {
            return -1;
        }

        char path[COMPAT_MAX_PATH];
        snprintf(path, sizeof(path), "%s\\%s", asc_5186C8, "LSGAME.MSG");
        if (!messageListLoad(&gLoadSaveMessageList, path)) {
            return -1;
        }
        messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_LSGAME, &gLoadSaveMessageList);

        if (window != -1) {
            windowDestroy(window);
        }

        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
        soundPlayFile("iisxxxx1");
        strcpy(_str0, getmsg(&gLoadSaveMessageList, &messageListItem, 134));
        strcpy(_str1, getmsg(&gLoadSaveMessageList, &messageListItem, 135));
        showDialogBox(_str0, body, 1, 169, 116, COLOR_AMBER, nullptr, COLOR_AMBER, DIALOG_BOX_LARGE);

        loadSaveMessageListReset();
        mapNewMap();
        _game_user_wants_to_quit = GAME_QUIT_REQUEST_MAIN_MENU;

        return -1;
    }

    _quick_done = false;

    int windowType;
    switch (mode) {
    case LOAD_SAVE_MODE_FROM_MAIN_MENU:
        windowType = LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU;
        break;
    case LOAD_SAVE_MODE_NORMAL:
        windowType = LOAD_SAVE_WINDOW_TYPE_LOAD_GAME;
        break;
    case LOAD_SAVE_MODE_QUICK:
        windowType = LOAD_SAVE_WINDOW_TYPE_PICK_QUICK_LOAD_SLOT;
        break;
    default:
        assert(false && "Should be unreachable");
    }

    int devAutoloadSlot = -1;
    if (mode == LOAD_SAVE_MODE_FROM_MAIN_MENU && gDevLoadGameSlot != -1) {
        devAutoloadSlot = gDevLoadGameSlot;
        gDevLoadGameSlot = -1;
    }

    touch_set_touchscreen_mode(windowType == LOAD_SAVE_WINDOW_TYPE_LOAD_GAME || windowType == LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU);

    if (lsgWindowInit(windowType) == -1) {
        debugPrint("\nLOADSAVE: ** Error loading save game screen data! **\n");
        return -1;
    }

    pipboyMessageListInit();

    if (_GetSlotList() == -1) {
        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
        windowRefresh(gLoadSaveWindow);
        renderPresent();
        if (mode == LOAD_SAVE_MODE_FROM_MAIN_MENU) {
            colorPaletteLoad("color.pal");
            paletteFadeTo(_cmap);
        }
        soundPlayFile("iisxxxx1");
        strcpy(_str0, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 106));
        strcpy(_str1, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 107));
        snprintf(_str2, sizeof(_str2), "\"%s\\\"", "SAVEGAME");
        showDialogBox(_str0, body, 2, 169, 116, COLOR_AMBER, nullptr, COLOR_AMBER, DIALOG_BOX_LARGE);
        lsgWindowFree(windowType);
        return -1;
    }

    bool devAutoloadPending = false;

    if (devAutoloadSlot != -1) {
        if (devAutoloadSlot >= 0 && devAutoloadSlot < saveLoadTotalSlots) {
            _slot_cursor = devAutoloadSlot;
            _currentSlotPage = devAutoloadSlot / slotsPerPage;
            if (_LSstatus[_slot_cursor] == SLOT_STATE_OCCUPIED) {
                devAutoloadPending = true;
            } else {
                debugPrint("LOADSAVE: dev load slot %d is not occupied\n", _slot_cursor + 1);
            }
        } else {
            debugPrint("LOADSAVE: invalid dev load slot %d\n", devAutoloadSlot + 1);
        }
    }

    switch (_LSstatus[_slot_cursor]) {
    case SLOT_STATE_EMPTY:
    case SLOT_STATE_ERROR:
    case SLOT_STATE_UNSUPPORTED_VERSION:
        blitBufferToBuffer(_loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getData(),
            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getHeight(),
            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
            gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 39 + 340,
            LS_WINDOW_WIDTH);
        break;
    default:
        _LoadTumbSlot(_slot_cursor);
        blitBufferToBuffer(_thumbnail_image,
            LS_PREVIEW_WIDTH - 1,
            LS_PREVIEW_HEIGHT - 1,
            LS_PREVIEW_WIDTH,
            gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366,
            LS_WINDOW_WIDTH);
        break;
    }

    _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_LOAD_GAME);
    _DrawInfoBox(_slot_cursor);
    windowRefresh(gLoadSaveWindow);
    renderPresent();
    if (mode == LOAD_SAVE_MODE_FROM_MAIN_MENU) {
        colorPaletteLoad("color.pal");
        paletteFadeTo(_cmap);
    }
    _dbleclkcntr = 24;

    int rc = -1;

    int doubleClickSlot = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        unsigned int time = getTicks();
        int keyCode = devAutoloadPending ? kLoadSaveActionDone : inputGetInput();
        devAutoloadPending = false;
        bool selectionChanged = false;
        int scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_NONE;

        convertMouseWheelToArrowKey(&keyCode);

        if (keyCode == KEY_ESCAPE || keyCode == 501 || _game_user_wants_to_quit != GAME_QUIT_REQUEST_NONE) {
            rc = 0;
        } else {
            switch (keyCode) {
            case KEY_ARROW_UP:
                if (_slot_cursor > 0) { // Prevent going below 0
                    if (_slot_cursor % 10 == 0 && _currentSlotPage > 0) {
                        // Move to the previous page and set cursor to the last slot on that page
                        _currentSlotPage--;
                        _slot_cursor--;
                        _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_LOAD_GAME);
                        windowRefresh(gLoadSaveWindow);
                    } else {
                        // Normal movement within the page
                        _slot_cursor--;
                    }
                }

                selectionChanged = true;
                doubleClickSlot = -1;
                break;

            case KEY_ARROW_DOWN:
                if (_slot_cursor < (saveLoadTotalSlots - 1)) { // Prevent going above 99
                    if (_slot_cursor % 10 == 9 && _currentSlotPage < (saveLoadTotalSlots / 10) - 1) {
                        // Move to the next page and set cursor to the first slot on that page
                        _currentSlotPage++;
                        _slot_cursor++;
                        _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_LOAD_GAME);
                        windowRefresh(gLoadSaveWindow);
                    } else {
                        // Normal movement within the page
                        _slot_cursor++;
                    }
                }

                selectionChanged = true;
                doubleClickSlot = -1;
                break;
            case KEY_HOME:
                // Move to the first slot of the current page
                _slot_cursor = _currentSlotPage * 10;
                selectionChanged = true;
                doubleClickSlot = -1;
                break;

            case KEY_END:
                // Move to the last slot of the current page
                _slot_cursor = (_currentSlotPage * 10) + 9;

                // Prevent overflow in the last page (e.g., last page may have less than 10 slots)
                if (_slot_cursor > (saveLoadTotalSlots - 1)) {
                    _slot_cursor = (saveLoadTotalSlots - 1);
                }

                selectionChanged = true;
                doubleClickSlot = -1;
                break;
            case 506:
                scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_UP;
                break;
            case 504:
                scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_DOWN;
                break;
            case KEY_ARROW_RIGHT:
            case KEY_ARROW_LEFT:
            case 502: { // Mouse click
                int mouseX, mouseY;
                mouseGetPositionInWindow(gLoadSaveWindow, &mouseX, &mouseY);

                // Check if the click was in the "Next Page" button area
                if ((mouseX >= 195 && mouseX <= 280 && mouseY >= 425 && mouseY <= 435) || keyCode == KEY_ARROW_RIGHT) { // coordinates for Next Page button
                    if (_currentSlotPage < (saveLoadTotalSlots / 10) - 1) { // Max 10 pages (0-9)
                        soundPlayFile("ib1p1xx1");
                        loadSaveSetCurrentPage(_currentSlotPage + 1);
                        selectionChanged = true;
                        doubleClickSlot = -1;
                        _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_LOAD_GAME);
                        windowRefresh(gLoadSaveWindow);
                    }
                    break;
                }

                // Check if the click was in the "Previous Page" button area
                if ((mouseX >= 55 && mouseX <= 180 && mouseY >= 425 && mouseY <= 435) || keyCode == KEY_ARROW_LEFT) { // Coordinates for Previous Page button
                    if (_currentSlotPage > 0) {
                        soundPlayFile("ib1p1xx1");
                        loadSaveSetCurrentPage(_currentSlotPage - 1);
                        selectionChanged = true;
                        doubleClickSlot = -1;

                        _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_LOAD_GAME);
                        windowRefresh(gLoadSaveWindow);
                    }
                    break;
                }

                // Calculate the clicked slot, adjusting for pagination
                int relativeSlot = (mouseY - 79) / (3 * fontGetLineHeight() + 4);
                if (relativeSlot < 0) {
                    relativeSlot = 0;
                } else if (relativeSlot > 9) {
                    relativeSlot = 9;
                }

                // Adjust for the current page
                int clickedSlot = (_currentSlotPage * 10) + relativeSlot;

                if (clickedSlot > (saveLoadTotalSlots - 1)) { // Ensure we don't go beyond max slots
                    clickedSlot = (saveLoadTotalSlots - 1);
                }

                _slot_cursor = clickedSlot;
                if (clickedSlot == doubleClickSlot) {
                    keyCode = kLoadSaveActionDone;
                    soundPlayFile("ib1p1xx1");
                }

                selectionChanged = true;
                doubleClickSlot = _slot_cursor;
                scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_NONE;
            } break;

            case KEY_MINUS:
            case KEY_UNDERSCORE:
                brightnessDecrease();
                break;
            case KEY_EQUAL:
            case KEY_PLUS:
                brightnessIncrease();
                break;
            case KEY_RETURN:
                keyCode = kLoadSaveActionDone;
                break;
            case KEY_CTRL_Q:
            case KEY_CTRL_X:
            case KEY_F10:
                showQuitConfirmationDialog();
                if (_game_user_wants_to_quit != GAME_QUIT_REQUEST_NONE) {
                    rc = 0;
                }
                break;
            }
        }

        if (keyCode == kLoadSaveActionDone) {
            if (_LSstatus[_slot_cursor] != SLOT_STATE_EMPTY) {
                rc = 1;
            } else {
                rc = -1;
            }

            selectionChanged = true;
            scrollDirection = LOAD_SAVE_SCROLL_DIRECTION_NONE;
        }

        if (scrollDirection != LOAD_SAVE_SCROLL_DIRECTION_NONE) {
            unsigned int scrollVelocity = 4;
            bool isScrolling = false;
            int scrollCounter = 0;
            do {
                sharedFpsLimiter.mark();

                unsigned int start = getTicks();
                scrollCounter += 1;

                if ((!isScrolling && scrollCounter == 1) || (isScrolling && scrollCounter > 14.4)) {
                    isScrolling = true;

                    if (scrollCounter > 14.4) {
                        scrollVelocity += 1;
                        if (scrollVelocity > 24) {
                            scrollVelocity = 24;
                        }
                    }
                    // handle scrolling between pages via buttons
                    if (scrollDirection == LOAD_SAVE_SCROLL_DIRECTION_UP) {
                        _slot_cursor--;

                        // If moving up past the first slot of the page, go to the previous page
                        if (_slot_cursor < _currentSlotPage * 10) {
                            if (_currentSlotPage > 0) {
                                _currentSlotPage--;
                                _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_LOAD_GAME);
                                windowRefresh(gLoadSaveWindow);
                                _slot_cursor = (_currentSlotPage * 10) + 9; // Move to the last slot of the previous page
                            } else {
                                _slot_cursor = 0; // Don't go under
                            }
                        }
                    } else { // LOAD_SAVE_SCROLL_DIRECTION_DOWN
                        soundPlayFile("ib1p1xx1");
                        _slot_cursor++;

                        // If moving down past the last slot of the page, go to the next page
                        if (_slot_cursor > (_currentSlotPage * 10) + 9) {
                            if (_currentSlotPage < (saveLoadTotalSlots / 10) - 1) { // Max pages: 0-9
                                _currentSlotPage++;
                                _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_LOAD_GAME);
                                windowRefresh(gLoadSaveWindow);
                                _slot_cursor = _currentSlotPage * 10; // Move to the first slot of the next page
                            } else {
                                _slot_cursor = (saveLoadTotalSlots - 1); // Prevent overflow (last slot overall)
                            }
                        }
                    }

                    switch (_LSstatus[_slot_cursor]) {
                    case SLOT_STATE_EMPTY:
                    case SLOT_STATE_ERROR:
                    case SLOT_STATE_UNSUPPORTED_VERSION:
                        blitBufferToBuffer(_loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getData(),
                            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
                            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getHeight(),
                            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
                            gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 39 + 340,
                            LS_WINDOW_WIDTH);
                        break;
                    default:
                        _LoadTumbSlot(_slot_cursor);
                        blitBufferToBuffer(_loadsaveFrmImages[LOAD_SAVE_FRM_BACKGROUND].getData() + LS_WINDOW_WIDTH * 39 + 340,
                            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
                            _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getHeight(),
                            LS_WINDOW_WIDTH,
                            gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 39 + 340,
                            LS_WINDOW_WIDTH);
                        blitBufferToBuffer(_thumbnail_image,
                            LS_PREVIEW_WIDTH - 1,
                            LS_PREVIEW_HEIGHT - 1,
                            LS_PREVIEW_WIDTH,
                            gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366,
                            LS_WINDOW_WIDTH);
                        break;
                    }

                    _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_LOAD_GAME);
                    _DrawInfoBox(_slot_cursor);
                    windowRefresh(gLoadSaveWindow);
                }

                if (scrollCounter > 14.4) {
                    delay_ms(1000 / scrollVelocity - (getTicks() - start));
                } else {
                    delay_ms(1000 / 24 - (getTicks() - start));
                }

                keyCode = inputGetInput();

                renderPresent();
                sharedFpsLimiter.throttle();
            } while (keyCode != 505 && keyCode != 503);
        } else {
            if (selectionChanged) {
                switch (_LSstatus[_slot_cursor]) {
                case SLOT_STATE_EMPTY:
                case SLOT_STATE_ERROR:
                case SLOT_STATE_UNSUPPORTED_VERSION:
                    blitBufferToBuffer(_loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getData(),
                        _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
                        _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getHeight(),
                        _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
                        gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 39 + 340,
                        LS_WINDOW_WIDTH);
                    break;
                default:
                    _LoadTumbSlot(_slot_cursor);
                    blitBufferToBuffer(_loadsaveFrmImages[LOAD_SAVE_FRM_BACKGROUND].getData() + LS_WINDOW_WIDTH * 39 + 340,
                        _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getWidth(),
                        _loadsaveFrmImages[LOAD_SAVE_FRM_PREVIEW_COVER].getHeight(),
                        LS_WINDOW_WIDTH,
                        gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 39 + 340,
                        LS_WINDOW_WIDTH);
                    blitBufferToBuffer(_thumbnail_image,
                        LS_PREVIEW_WIDTH - 1,
                        LS_PREVIEW_HEIGHT - 1,
                        LS_PREVIEW_WIDTH,
                        gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 58 + 366,
                        LS_WINDOW_WIDTH);
                    break;
                }

                _DrawInfoBox(_slot_cursor);
                _ShowSlotList(LOAD_SAVE_WINDOW_TYPE_LOAD_GAME);
            }

            windowRefresh(gLoadSaveWindow);

            _dbleclkcntr -= 1;
            if (_dbleclkcntr == 0) {
                _dbleclkcntr = 24;
                doubleClickSlot = -1;
            }
        }

        if (rc == 1) {
            switch (_LSstatus[_slot_cursor]) {
            case SLOT_STATE_UNSUPPORTED_VERSION:
                soundPlayFile("iisxxxx1");
                strcpy(_str0, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 134));
                strcpy(_str1, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 136));
                strcpy(_str2, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 135));
                showDialogBox(_str0, body, 2, 169, 116, COLOR_AMBER, nullptr, COLOR_AMBER, DIALOG_BOX_LARGE);
                rc = -1;
                break;
            case SLOT_STATE_ERROR:
                soundPlayFile("iisxxxx1");
                strcpy(_str0, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 134));
                strcpy(_str1, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 136));
                showDialogBox(_str0, body, 1, 169, 116, COLOR_AMBER, nullptr, COLOR_AMBER, DIALOG_BOX_LARGE);
                rc = -1;
                break;
            default:
                if (lsgLoadGameInSlot(_slot_cursor) == -1) {
                    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
                    soundPlayFile("iisxxxx1");
                    strcpy(_str0, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 134));
                    strcpy(_str1, getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 135));
                    showDialogBox(_str0, body, 1, 169, 116, COLOR_AMBER, nullptr, COLOR_AMBER, DIALOG_BOX_LARGE);
                    mapNewMap();
                    _game_user_wants_to_quit = GAME_QUIT_REQUEST_MAIN_MENU;
                    rc = -1;
                }
                break;
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (mode == LOAD_SAVE_MODE_FROM_MAIN_MENU && rc == 0) {
        paletteFadeTo(gPaletteBlack);
    }

    lsgWindowFree(mode == LOAD_SAVE_MODE_FROM_MAIN_MENU
            ? LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU
            : LOAD_SAVE_WINDOW_TYPE_LOAD_GAME);

    pipboyMessageListFree();

    if (mode == LOAD_SAVE_MODE_QUICK) {
        if (rc == 1) {
            _quick_done = true;
        }
    }

    return rc;
}

void lsgDevSetLoadGameSlot(int slot)
{
    gDevLoadGameSlot = slot;
}

int lsgGetTotalSlotCount()
{
    return saveLoadTotalSlots;
}

int loadsaveGetCurrentSlot()
{
    return _slot_cursor;
}

void loadsaveSetCurrentSlot(int page, int slot)
{
    int newSlot = page * slotsPerPage + slot;
    _slot_cursor = std::clamp(newSlot, 0, saveLoadTotalSlots - 1);
    _currentSlotPage = _slot_cursor / slotsPerPage;
}

int loadsaveGetCurrentPage()
{
    return _currentSlotPage;
}

int loadsaveGetCurrentSlotInPage()
{
    return _slot_cursor % slotsPerPage;
}

// 0x47D2E4
static int lsgWindowInit(int windowType)
{
    gLoadSaveWindowOldFont = fontGetCurrent();
    fontSetCurrent(103);

    gLoadSaveWindowIsoWasEnabled = false;
    if (!messageListInit(&gLoadSaveMessageList)) {
        return -1;
    }

    snprintf(_str, sizeof(_str), "%s%s", asc_5186C8, LSGAME_MSG_NAME);
    if (!messageListLoad(&gLoadSaveMessageList, _str)) {
        return -1;
    }
    messageListRepositorySetStandardMessageList(STANDARD_MESSAGE_LIST_LSGAME, &gLoadSaveMessageList);

    _snapshot = (unsigned char*)internal_malloc(61632);
    if (_snapshot == nullptr) {
        loadSaveMessageListReset();
        fontSetCurrent(gLoadSaveWindowOldFont);
        return -1;
    }

    _thumbnail_image = _snapshot;
    _snapshotBuf = _snapshot + LS_PREVIEW_SIZE;

    if (windowType != LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU) {
        gLoadSaveWindowIsoWasEnabled = isoDisable();
    }

    colorCycleDisable();

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    if (windowType == LOAD_SAVE_WINDOW_TYPE_SAVE_GAME || windowType == LOAD_SAVE_WINDOW_TYPE_PICK_QUICK_SAVE_SLOT) {
        bool gameMouseWasVisible = gameMouseObjectsIsVisible();
        if (gameMouseWasVisible) {
            gameMouseObjectsHide();
        }

        mouseHideCursor();
        tileWindowRefresh();
        mouseShowCursor();

        if (gameMouseWasVisible) {
            gameMouseObjectsShow();
        }

        // For preview take 640x380 area in the center of isometric window.
        Window* window = windowGetWindow(gIsoWindow);
        unsigned char* isoWindowBuffer = window->buffer
            + window->width * (window->height - ORIGINAL_ISO_WINDOW_HEIGHT) / 2
            + (window->width - ORIGINAL_ISO_WINDOW_WIDTH) / 2;
        blitBufferToBufferStretch(isoWindowBuffer,
            ORIGINAL_ISO_WINDOW_WIDTH,
            ORIGINAL_ISO_WINDOW_HEIGHT,
            windowGetWidth(gIsoWindow),
            _snapshotBuf,
            LS_PREVIEW_WIDTH,
            LS_PREVIEW_HEIGHT,
            LS_PREVIEW_WIDTH);
    }

    for (int index = 0; index < LOAD_SAVE_FRM_COUNT; index++) {
        int fid = buildFid(OBJ_TYPE_INTERFACE, gLoadSaveFrmIds[index]);
        if (!_loadsaveFrmImages[index].lock(fid)) {
            while (--index >= 0) {
                _loadsaveFrmImages[index].unlock();
            }
            internal_free(_snapshot);
            loadSaveMessageListReset();
            fontSetCurrent(gLoadSaveWindowOldFont);

            if (windowType != LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU) {
                if (gLoadSaveWindowIsoWasEnabled) {
                    isoEnable();
                }
            }

            colorCycleEnable();
            gameMouseSetCursor(MOUSE_CURSOR_ARROW);
            return -1;
        }
    }

    int lsWindowX = (screenGetWidth() - LS_WINDOW_WIDTH) / 2;
    int lsWindowY = (screenGetHeight() - LS_WINDOW_HEIGHT) / 2;
    gLoadSaveWindow = windowCreate(lsWindowX,
        lsWindowY,
        LS_WINDOW_WIDTH,
        LS_WINDOW_HEIGHT,
        256,
        WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (gLoadSaveWindow == -1) {
        for (int index = 0; index < LOAD_SAVE_FRM_COUNT; index++) {
            _loadsaveFrmImages[index].unlock();
        }
        internal_free(_snapshot);
        loadSaveMessageListReset();
        fontSetCurrent(gLoadSaveWindowOldFont);

        if (windowType != LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU) {
            if (gLoadSaveWindowIsoWasEnabled) {
                isoEnable();
            }
        }

        colorCycleEnable();
        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
        return -1;
    }

    gLoadSaveWindowBuffer = windowGetBuffer(gLoadSaveWindow);
    memcpy(gLoadSaveWindowBuffer, _loadsaveFrmImages[LOAD_SAVE_FRM_BACKGROUND].getData(), LS_WINDOW_WIDTH * LS_WINDOW_HEIGHT);

    int messageId;
    switch (windowType) {
    case LOAD_SAVE_WINDOW_TYPE_SAVE_GAME:
        // SAVE GAME
        messageId = 102;
        break;
    case LOAD_SAVE_WINDOW_TYPE_PICK_QUICK_SAVE_SLOT:
        // PICK A QUICK SAVE SLOT
        messageId = 103;
        break;
    case LOAD_SAVE_WINDOW_TYPE_LOAD_GAME:
    case LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU:
        // LOAD GAME
        messageId = 100;
        break;
    case LOAD_SAVE_WINDOW_TYPE_PICK_QUICK_LOAD_SLOT:
        // PICK A QUICK LOAD SLOT
        messageId = 101;
        break;
    default:
        assert(false && "Should be unreachable");
    }

    char* msg;

    msg = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, messageId);
    fontDrawText(gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 27 + 48, msg, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, COLOR_DARK_YELLOW);

    // DONE
    msg = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 104);
    fontDrawText(gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 348 + 410, msg, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, COLOR_DARK_YELLOW);

    // CANCEL
    msg = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 105);
    fontDrawText(gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 348 + 515, msg, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, COLOR_DARK_YELLOW);

    int btn;

    btn = buttonCreate(gLoadSaveWindow,
        391,
        349,
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].getWidth(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].getHeight(),
        -1,
        -1,
        -1,
        kLoadSaveActionDone,
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_NORMAL].getData(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    btn = buttonCreate(gLoadSaveWindow,
        495,
        349,
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].getWidth(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].getHeight(),
        -1,
        -1,
        -1,
        501,
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_NORMAL].getData(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    btn = buttonCreate(gLoadSaveWindow,
        35,
        58,
        _loadsaveFrmImages[LOAD_SAVE_FRM_ARROW_UP_PRESSED].getWidth(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_ARROW_UP_PRESSED].getHeight(),
        -1,
        505,
        506,
        505,
        _loadsaveFrmImages[LOAD_SAVE_FRM_ARROW_UP_NORMAL].getData(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_ARROW_UP_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    btn = buttonCreate(gLoadSaveWindow,
        35,
        _loadsaveFrmImages[LOAD_SAVE_FRM_ARROW_UP_PRESSED].getHeight() + 58,
        _loadsaveFrmImages[LOAD_SAVE_FRM_ARROW_DOWN_PRESSED].getWidth(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_ARROW_DOWN_PRESSED].getHeight(),
        -1,
        503,
        504,
        503,
        _loadsaveFrmImages[LOAD_SAVE_FRM_ARROW_DOWN_NORMAL].getData(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_ARROW_DOWN_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    // tweaked bounds to accomodate Next/Previous buttons
    buttonCreate(gLoadSaveWindow, 55, 87, 230, 348, -1, -1, -1, 502, nullptr, nullptr, nullptr, BUTTON_FLAG_TRANSPARENT);

    fontSetCurrent(101);

    return 0;
}

// 0x47D824
static int lsgWindowFree(int windowType)
{
    loadSavePersistSelectedSlot();

    windowDestroy(gLoadSaveWindow);
    fontSetCurrent(gLoadSaveWindowOldFont);
    loadSaveMessageListReset();

    for (int index = 0; index < LOAD_SAVE_FRM_COUNT; index++) {
        _loadsaveFrmImages[index].unlock();
    }

    internal_free(_snapshot);

    if (windowType != LOAD_SAVE_WINDOW_TYPE_LOAD_GAME_FROM_MAIN_MENU) {
        if (gLoadSaveWindowIsoWasEnabled) {
            isoEnable();
        }
    }

    colorCycleEnable();
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    touch_set_touchscreen_mode(false);

    return 0;
}

#if defined(__EMSCRIPTEN__)
// clang-format off
EM_ASYNC_JS(void, do_save_idbfs_loadsave, (), {
    await new Promise((resolve, reject) => FS.syncfs(err => err ? reject(err) : resolve()))
});
// clang-format on
#endif

// 0x47D88C
static int lsgPerformSaveGame()
{
    _ls_error_code = 0;
    _map_backup_count = -1;
    gameMouseSetCursor(MOUSE_CURSOR_WAIT_PLANET);

    backgroundSoundPause();

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s", _patches, "SAVEGAME");
    compat_mkdir(_gmpath);

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s\\%s%.2d", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);
    compat_mkdir(_gmpath);

    // snprintf above may truncate if _patches path is extremely long;
    // use snprintf with remaining buffer space instead of unbounded strcat.
    snprintf(_gmpath + strlen(_gmpath), sizeof(_gmpath) - strlen(_gmpath), "\\" PROTO_DIR_NAME);
    compat_mkdir(_gmpath);

    char* protoBasePath = _gmpath + strlen(_gmpath);

    strcpy(protoBasePath, "\\" CRITTERS_DIR_NAME);
    compat_mkdir(_gmpath);

    strcpy(protoBasePath, "\\" ITEMS_DIR_NAME);
    compat_mkdir(_gmpath);

    if (_SaveBackup() == -1) {
        debugPrint("\nLOADSAVE: ** Error: can't backup save file! Aborting save. **\n");
        _partyMemberUnPrepSave();
        backgroundSoundResume();
        return -1;
    }

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
    strcat(_gmpath, "SAVE.DAT");

    // Snapshot the relative save path into a LOCAL before the handler loop.
    // _gmpath is a global that the save handlers clobber mid-save (e.g.
    // _GameMap2Slot rewrites it to the slot dir / AUTOMAP.DB.SAV paths) — the
    // post-loop rename must not re-read it or the destination becomes garbage.
    char _saveDatRel[COMPAT_MAX_PATH];
    snprintf(_saveDatRel, sizeof(_saveDatRel), "%s", _gmpath);

    // Build temp file path for atomic write (temp-file-then-rename, same pattern
    // used by sfallgv.sav at lines 2050-2086).
    char _saveDatTmp[COMPAT_MAX_PATH];
    snprintf(_saveDatTmp, sizeof(_saveDatTmp), "%s.tmp", _gmpath);

    debugPrint("\nLOADSAVE: Save name: %s\n", _gmpath);

    // C-02 (CRITICAL): Open the temp save file "w+b" (read-write), not "wb"
    // (write-only). The header-CRC read-back (lsgSaveHeaderInSlot) and the
    // handler-CRC read-back (handler loop below) both fread() from this stream
    // to compute CRC32s. On a write-only stream fread() returns 0, the
    // read-back silently fails, and every save aborted with
    // "Error writing save game header!" / "Error reading save data for CRC".
    _flptr = fileOpen(_saveDatTmp, "w+b");
    if (_flptr == nullptr) {
        debugPrint("\nLOADSAVE: ** Error opening save game for writing! **\n");
        _RestoreSave();
        snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
        MapDirErase(_gmpath, "BAK");
        _partyMemberUnPrepSave();
        backgroundSoundResume();
        return -1;
    }

    long pos = fileTell(_flptr);
    if (lsgSaveHeaderInSlot(_slot_cursor) == -1) {
        debugPrint("\nLOADSAVE: ** Error writing save game header! **\n");
        debugPrint("LOADSAVE: Save file header size written: %ld bytes.\n", fileTell(_flptr) - pos);
        fileClose(_flptr);
        compat_remove(_saveDatTmp);
        _RestoreSave();
        snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
        MapDirErase(_gmpath, "BAK");
        _partyMemberUnPrepSave();
        backgroundSoundResume();
        return -1;
    }

    // M-62 (MEDIUM): Ensure the worldmap save handler writes the fork-format
    // stream. New saves are always written by this build in the current fork
    // format (1.4R), which includes the gScriptWorldMapMulti float — even when
    // the save slot previously held an upstream/vanilla 1.2R save.
    gLoadedSaveVersionMajor = VERSION_MINOR;

    for (int index = 0; index < LOAD_SAVE_HANDLER_COUNT; index++) {
        long chunkStart = fileTell(_flptr);
        SaveGameHandler* handler = _master_save_list[index];
        // SFALL: Write placeholder CRC32 before handler data (version 1.3+).
        // The CRC covers only the handler's data bytes (not the CRC field itself).
        // Computed after the handler writes, then patched back with fileSeek.
        fileWriteUInt32(_flptr, 0); // placeholder CRC
        long dataStart = fileTell(_flptr);

        long pos = chunkStart;
        if (handler(_flptr) == -1) {
            debugPrint("\nLOADSAVE: ** Error writing save function #%d data! **\n", index);
            fileClose(_flptr);
            compat_remove(_saveDatTmp);
            _RestoreSave();
            snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
            MapDirErase(_gmpath, "BAK");
            _partyMemberUnPrepSave();
            backgroundSoundResume();
            return -1;
        }

        long dataEnd = fileTell(_flptr);
        long dataSize = dataEnd - dataStart;

        // SFALL: Compute CRC over the handler's written data and patch it in.
        if (dataSize > 0) {
            unsigned char* buffer = (unsigned char*)internal_malloc(dataSize);
            if (buffer != nullptr) {
                memset(buffer, 0, dataSize);
                fileSeek(_flptr, dataStart, SEEK_SET);
                if (fileRead(buffer, 1, dataSize, _flptr) != dataSize) {
                    internal_free(buffer);
                    debugPrint("\nLOADSAVE: ** Error reading save data for CRC in function #%d! **\n", index);
                    fileClose(_flptr);
                    compat_remove(_saveDatTmp);
                    _RestoreSave();
                    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
                    MapDirErase(_gmpath, "BAK");
                    _partyMemberUnPrepSave();
                    backgroundSoundResume();
                    return -1;
                }
                unsigned int crc = _crc32Compute(buffer, dataSize);
                internal_free(buffer);
                fileSeek(_flptr, chunkStart, SEEK_SET);
                fileWriteUInt32(_flptr, crc);
                fileSeek(_flptr, dataEnd, SEEK_SET);
            } else {
                debugPrint("\nLOADSAVE: ** Error allocating CRC buffer for save function #%d! **\n", index);
                fileClose(_flptr);
                compat_remove(_saveDatTmp);
                _RestoreSave();
                snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
                MapDirErase(_gmpath, "BAK");
                _partyMemberUnPrepSave();
                backgroundSoundResume();
                return -1;
            }
        }

        debugPrint("LOADSAVE: Save function #%d data size written: %ld bytes (CRC).\n", index, fileTell(_flptr) - pos);
    }

    debugPrint("LOADSAVE: Total save data written: %ld bytes.\n", fileTell(_flptr));

    // I2F-031: Commit SAVE.DAT first, then sfallgv.sav.
    // If a crash occurs between these two commits, the worst case is
    // new SAVE.DAT + old (or missing) sfallgv.sav. Missing or corrupt
    // sfallgv.sav is handled gracefully by loading defaults (F-M042).
    // The previous ordering (sfallgv first, SAVE.DAT second) could leave
    // new sfallgv with old SAVE.DAT — a semantic mismatch that was not
    // detectable during load.
    fileClose(_flptr);
    _flptr = nullptr;

    // Atomically rename temp file to final SAVE.DAT.
    // NOTE: fileOpen above resolves the relative temp path through the
    // directory xbase registered from _patches (e.g. "data"), so the file
    // physically lives at "<_patches>\SAVEGAME\SLOTxx\SAVE.DAT.tmp". The
    // rename must therefore use the _patches-prefixed paths — a bare
    // CWD-relative rename points at "./SAVEGAME/..." which does not exist,
    // and the save silently fails with "Error renaming temp save file"
    // (regression introduced by the F-61 atomic-save rewrite; the sibling
    // _SaveBackup already builds _patches-prefixed paths for its rename).
    char saveDatTmpFull[COMPAT_MAX_PATH];
    char saveDatFull[COMPAT_MAX_PATH];
    snprintf(saveDatTmpFull, sizeof(saveDatTmpFull), "%s\\%s", _patches, _saveDatTmp);
    // NOTE: use the pre-loop snapshot _saveDatRel, NOT _gmpath — the global was
    // clobbered by the save handlers above (e.g. _GameMap2Slot leaves it as the
    // AUTOMAP.DB.SAV path), which would corrupt the rename destination.
    snprintf(saveDatFull, sizeof(saveDatFull), "%s\\%s", _patches, _saveDatRel);
    if (compat_rename(saveDatTmpFull, saveDatFull) != 0) {
        debugPrint("\nLOADSAVE: ** Error renaming temp save file to SAVE.DAT! **\n");
        compat_remove(saveDatTmpFull);
        _RestoreSave();
        snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
        MapDirErase(_gmpath, "BAK");
        _partyMemberUnPrepSave();
        backgroundSoundResume();
        return -1;
    }

    // SAVE.DAT is committed. Now write sfallgv.sav atomically.
    {
        char sfPath[COMPAT_MAX_PATH];
        char tmpPath[COMPAT_MAX_PATH];
        snprintf(sfPath, sizeof(sfPath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
        snprintf(tmpPath, sizeof(tmpPath), "%ssfallgv.tmp", sfPath);
        strcat(sfPath, "sfallgv.sav");

        // save NEW-2 (MEDIUM): Open the sfallgv temp file "w+b" (read-write),
        // not "wb". sfallSaveGameData appends a CRC32 trailer over the whole
        // payload and needs to read back the written bytes to compute it —
        // the same read-back pattern as the SAVE.DAT handler/header CRCs.
        File* sfFile = fileOpen(tmpPath, "w+b");
        if (sfFile != nullptr) {
            bool saved = sfallSaveGameData(sfFile);
            fileClose(sfFile);
            // Same _patches-prefix requirement as the SAVE.DAT rename above:
            // fileOpen resolved tmpPath through the directory xbase, so the
            // physical file lives under <_patches>\SAVEGAME\...
            char sfTmpFull[COMPAT_MAX_PATH];
            char sfFull[COMPAT_MAX_PATH];
            snprintf(sfTmpFull, sizeof(sfTmpFull), "%s\\%s", _patches, tmpPath);
            snprintf(sfFull, sizeof(sfFull), "%s\\%s", _patches, sfPath);
            if (!saved || compat_rename(sfTmpFull, sfFull) != 0) {
                // sfallgv.sav write or rename failed after SAVE.DAT was
                // already committed. Clean up the temp file and restore
                // the previous save state (which will revert SAVE.DAT).
                compat_remove(sfTmpFull);
                _RestoreSave();
                snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
                MapDirErase(_gmpath, "BAK");
                _partyMemberUnPrepSave();
                backgroundSoundResume();
                return -1;
            }
        } else {
            // sfallgv.sav temp file open failed. SAVE.DAT is already
            // committed — restore the previous save state.
            debugPrint("\nLOADSAVE: ** Error opening sfallgv.sav temp file for writing! **\n");
            _RestoreSave();
            snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
            MapDirErase(_gmpath, "BAK");
            _partyMemberUnPrepSave();
            backgroundSoundResume();
            return -1;
        }
    }

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
    MapDirErase(_gmpath, "BAK");

#if defined(__EMSCRIPTEN__)
    do_save_idbfs_loadsave();
#endif

    gLoadSaveMessageListItem.num = 140;
    if (messageListGetItem(&gLoadSaveMessageList, &gLoadSaveMessageListItem)) {
        displayMonitorAddMessage(gLoadSaveMessageListItem.text);
    } else {
        debugPrint("\nError: Couldn't find LoadSave Message!");
    }

    backgroundSoundResume();

    return 0;
}

// 0x47DC60
bool _isLoadingGame()
{
    return _loadingGame;
}

int mapIdBeingLoaded()
{
    return _loadingMapId;
}

// 0x47DC68
static int lsgLoadGameInSlot(int slot)
{
    if (slot < 0 || slot >= saveLoadTotalSlots) {
        return -1;
    }
    assert(slot == _slot_cursor);

    _loadingGame = true;

    if (isInCombat()) {
        interfaceBarEndButtonsHide(false);
        _combat_over_from_load();
        gameMouseSetCursor(MOUSE_CURSOR_WAIT_PLANET);
    }

    // SFALL: Call "before start" event
    sfallOnBeforeGameStart();

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
    strcat(_gmpath, "SAVE.DAT");

    _flptr = fileOpen(_gmpath, "rb");
    if (_flptr == nullptr) {
        // Auto-recover from backup if SAVE.DAT was corrupted by a crash
        // during a non-atomic direct write. The atomic temp-file-then-rename
        // pattern (above) prevents this in the future, but saves made before
        // this fix may still have orphaned .BAK files.
        char _saveDatBak[COMPAT_MAX_PATH];
        snprintf(_saveDatBak, sizeof(_saveDatBak), "%s.BAK", _gmpath);
        File* bakFile = fileOpen(_saveDatBak, "rb");
        if (bakFile != nullptr) {
            fileClose(bakFile);
            debugPrint("\nLOADSAVE: SAVE.DAT missing, recovering from SAVE.DAT.BAK...\n");
            // Same _patches-prefix requirement as the save rename above:
            // fileOpen resolved the relative path through the directory xbase,
            // so the physical file lives under <_patches>\SAVEGAME\... — a
            // bare CWD-relative rename points at "./SAVEGAME/..." (missing).
            char saveDatBakFull[COMPAT_MAX_PATH];
            char gmpathFull[COMPAT_MAX_PATH];
            snprintf(saveDatBakFull, sizeof(saveDatBakFull), "%s\\%s", _patches, _saveDatBak);
            snprintf(gmpathFull, sizeof(gmpathFull), "%s\\%s", _patches, _gmpath);
            if (compat_rename(saveDatBakFull, gmpathFull) == 0) {
                _flptr = fileOpen(_gmpath, "rb");
                if (_flptr != nullptr) {
                    debugPrint("LOADSAVE: Successfully recovered save from backup.\n");
                    // fall through to normal load
                }
            }
        }
        if (_flptr == nullptr) {
            debugPrint("\nLOADSAVE: ** Error opening load game file for reading! **\n");
            _loadingGame = false;
            return -1;
        }
    }

    long pos = fileTell(_flptr);
    if (lsgLoadHeaderInSlot(slot) == -1) {
        debugPrint("\nLOADSAVE: ** Error reading save  game header! **\n");
        fileClose(_flptr);
        gameReset();
        _loadingGame = false;
        return -1;
    }

    LoadSaveSlotData* ptr = &(_LSData[slot]);
    _loadingMapId = _LSData[slot].map;
    debugPrint("\nLOADSAVE: Load name: %s\n", ptr->description);

    debugPrint("LOADSAVE: Load file header size read: %ld bytes.\n", fileTell(_flptr) - pos);

    // SFALL: Handler chunk CRC verification (version 1.3+). Each handler
    // chunk is prefixed with a 4-byte CRC32 computed over the handler's data
    // bytes. On load, the CRC is read first, then the handler reads its data,
    // and a recomputed CRC is compared against the stored value. Mismatch
    // means the chunk was corrupted on disk or the save format diverged.
    // Version 1.2 saves have no CRC — they load without verification.
    bool hasHandlerCrc = (ptr->versionMajor >= SAVE_FORMAT_CRC_VERSION_MAJOR);

    // M-62 (MEDIUM): Set the worldmap save-format version gate to the loaded
    // save's versionMajor BEFORE the handler loop runs. wmWorldMap_load reads
    // the fork-added gScriptWorldMapMulti float only when versionMajor >= 3;
    // for upstream/vanilla 1.2R saves (versionMajor == 2) it must skip the
    // float or the first city's x is consumed and the stream misaligns.
    gLoadedSaveVersionMajor = ptr->versionMajor;

    // C-04 (HIGH): Select the handler list and count by save-format version.
    //   versionMajor <= 3 (1.2R/1.3R): legacy 27-chunk layout — index 26 is
    //     _EndLoad (pre-pass-15). Using the 28-chunk list here would run
    //     lightLoad at index 26 on a save that has no light data (EOF/garbage).
    //   versionMajor >= 4 (1.4R): 28-chunk layout — index 26 is lightLoad,
    //     index 27 is _EndLoad.
    const bool has28Handlers = (ptr->versionMajor >= SAVE_FORMAT_28_HANDLERS_VERSION_MAJOR);
    const int loadHandlerCount = has28Handlers ? LOAD_SAVE_HANDLER_COUNT : LOAD_SAVE_LEGACY_HANDLER_COUNT;
    LoadGameHandler** loadList = has28Handlers ? _master_load_list : _master_load_list_legacy;

    for (int index = 0; index < loadHandlerCount; index += 1) {
        long pos = fileTell(_flptr);
        LoadGameHandler* handler = loadList[index];

        unsigned int storedCrc = 0;
        if (hasHandlerCrc) {
            if (fileReadUInt32(_flptr, &storedCrc) != 0) {
                debugPrint("\nLOADSAVE: ** Error reading CRC for load function #%d! **\n", index);
                fileClose(_flptr);
                gameReset();
                _loadingGame = false;
                return -1;
            }
        }
        long dataStart = fileTell(_flptr);

        if (handler(_flptr) == -1) {
            debugPrint("\nLOADSAVE: ** Error reading load function #%d data! **\n", index);
            debugPrint("LOADSAVE: Load function #%d data size read: %ld bytes.\n", index, fileTell(_flptr) - pos);
            fileClose(_flptr);
            gameReset();
            _loadingGame = false;
            _loadingMapId = -1;
            return -1;
        }

        long dataEnd = fileTell(_flptr);
        long dataSize = dataEnd - dataStart;

        if (hasHandlerCrc) {
            // SFALL: Verify CRC over the handler's data bytes.
            unsigned int computedCrc = 0;
            bool crcComputed = false;

            if (dataSize > 0) {
                unsigned char* buffer = (unsigned char*)internal_malloc(dataSize);
                if (buffer != nullptr) {
                    memset(buffer, 0, dataSize);
                    fileSeek(_flptr, dataStart, SEEK_SET);
                    if (fileRead(buffer, 1, dataSize, _flptr) == dataSize) {
                        computedCrc = _crc32Compute(buffer, dataSize);
                        crcComputed = true;
                    }
                    internal_free(buffer);
                    fileSeek(_flptr, dataEnd, SEEK_SET);
                }
            }

            bool crcOk = false;
            if (dataSize == 0) {
                // Zero-length handler data: stored CRC should be 0 (the placeholder).
                crcOk = (storedCrc == 0);
            } else if (crcComputed) {
                crcOk = (computedCrc == storedCrc);
            }

            if (!crcOk) {
                debugPrint("\nLOADSAVE: ** CRC mismatch for load function #%d! (stored=%08x, computed=%08x) **\n",
                    index, storedCrc, dataSize == 0 ? 0u : computedCrc);
                displayMonitorAddMessage("Save data integrity check failed!");
                fileClose(_flptr);
                gameReset();
                _loadingGame = false;
                return -1;
            }
        }

        debugPrint("LOADSAVE: Load function #%d data size read: %ld bytes.\n", index, fileTell(_flptr) - pos);
    }

    _loadingMapId = -1;

    debugPrint("LOADSAVE: Total load data read: %ld bytes.\n", fileTell(_flptr));
    fileClose(_flptr);

    // SFALL: Load sfallgv.sav.
    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
    strcat(_gmpath, "sfallgv.sav");

    _flptr = fileOpen(_gmpath, "rb");
    if (_flptr != nullptr) {
        bool loaded = sfallLoadGameData(_flptr);
        fileClose(_flptr);
        if (!loaded) {
            // sfallgv.sav exists but is corrupt. sfallLoadGameData() may
            // have partially loaded state (globals, arrays, metarules)
            // before failing. Re-clear all four sfall state domains so
            // no partial state survives — matching the state after a
            // missing sfallgv.sav where _PrepLoad's gameReset() clears
            // everything. Without this re-clear, partial globals, arrays,
            // metarule overrides, or opcode state from the corrupt save
            // persist into the game session and would be serialized on
            // next save.
            // UM-57: Also reset opcode state (hit chance mods, perk/trait
            // overrides, XP modifier, etc.) that sfallLoadGameData may
            // have partially initialized before failing.
            sfall_gl_vars_reset();
            sfallArraysReset();
            sfall_metarules_reset();
            sfallOpcodesReset();
            debugPrint("\nLOADSAVE: ** sfallgv.sav corrupt — sfall state not loaded (using defaults) **\n");
            displayMonitorAddMessage("sfallgv.sav corrupt — sfall data not restored.");
        }
    } else {
        // F2-023: sfallgv.sav is missing for this save slot.
        // gameReset() (called by _PrepLoad handler 0) already cleared
        // sfall globals to defaults, so the engine will function — but
        // sfall state (globals, arrays, metarules) from the original save
        // is lost. Inform the user rather than silently skipping.
        debugPrint("\nLOADSAVE: ** sfallgv.sav not found — sfall state not loaded (using defaults) **\n");
        displayMonitorAddMessage("sfallgv.sav not found — sfall data not restored.");
    }

    snprintf(_str, sizeof(_str), "%s\\", "MAPS");
    MapDirErase(_str, "BAK");
    _proto_dude_update_gender();

    // Game Loaded.
    gLoadSaveMessageListItem.num = 141;
    if (messageListGetItem(&gLoadSaveMessageList, &gLoadSaveMessageListItem) == 1) {
        displayMonitorAddMessage(gLoadSaveMessageListItem.text);
    } else {
        debugPrint("\nError: Couldn't find LoadSave Message!");
    }

    _loadingGame = false;

    // SFALL: Increment game load counter for game_loaded() tri-state (F-058).
    sfall_gl_scr_increment_load_count();
    // SFALL: Start global scripts.
    sfall_gl_scr_exec_start_proc();
    // SFALL: Call "after start" event
    sfallOnAfterGameStarted();
    gGameLoaded = true;

    return 0;
}

// 0x47DF10
static int lsgSaveHeaderInSlot(int slot)
{
    _ls_error_code = 4;

    long headerStart = fileTell(_flptr);

    LoadSaveSlotData* ptr = &(_LSData[slot]);
    strncpy(ptr->signature, LOAD_SAVE_SIGNATURE, 24);

    if (fileWrite(ptr->signature, 1, 24, _flptr) == -1) {
        return -1;
    }

    short temp[3];
    temp[0] = VERSION_MAJOR;
    temp[1] = VERSION_MINOR;

    ptr->versionMinor = temp[0];
    ptr->versionMajor = temp[1];

    if (fileWriteInt16List(_flptr, temp, 2) == -1) {
        return -1;
    }

    ptr->versionRelease = VERSION_RELEASE;
    if (fileWriteUInt8(_flptr, VERSION_RELEASE) == -1) {
        return -1;
    }

    char* characterName = critterGetName(gDude);
    strncpy(ptr->characterName, characterName, 32);
    // I2-084: strncpy does not guarantee null termination when source >= 32 chars.
    ptr->characterName[sizeof(ptr->characterName) - 1] = '\0';

    if (fileWrite(ptr->characterName, 32, 1, _flptr) != 1) {
        return -1;
    }

    if (fileWrite(ptr->description, 30, 1, _flptr) != 1) {
        return -1;
    }

    time_t now = time(nullptr);
    struct tm* local = localtime(&now);

    temp[0] = local->tm_mday;
    temp[1] = local->tm_mon + 1;
    temp[2] = local->tm_year + 1900;

    ptr->fileDay = temp[0];
    ptr->fileMonth = temp[1];
    ptr->fileYear = temp[2];
    ptr->fileTime = local->tm_hour + local->tm_min;

    if (fileWriteInt16List(_flptr, temp, 3) == -1) {
        return -1;
    }

    if (_db_fwriteLong(_flptr, ptr->fileTime) == -1) {
        return -1;
    }

    int month;
    int day;
    int year;
    gameTimeGetDate(&month, &day, &year);

    temp[0] = month;
    temp[1] = day;
    temp[2] = year;
    ptr->gameTime = gameTimeGetTime();

    if (fileWriteInt16List(_flptr, temp, 3) == -1) {
        return -1;
    }

    if (fileWriteUInt32(_flptr, ptr->gameTime) == -1) {
        return -1;
    }

    ptr->elevation = gElevation;
    if (fileWriteInt16(_flptr, ptr->elevation) == -1) {
        return -1;
    }

    ptr->map = mapGetCurrentMap();
    if (fileWriteInt16(_flptr, ptr->map) == -1) {
        return -1;
    }

    char mapName[128];
    strcpy(mapName, gMapHeader.name);

    // NOTE: Uppercased from "sav".
    char* v1 = _strmfe(_str, mapName, "SAV");
    strncpy(ptr->fileName, v1, 16);
    ptr->fileName[sizeof(ptr->fileName) - 1] = '\0'; // F-M6: guarantee null termination
    if (fileWrite(ptr->fileName, 16, 1, _flptr) != 1) {
        return -1;
    }

    if (fileWrite(_snapshotBuf, LS_PREVIEW_SIZE, 1, _flptr) != 1) {
        return -1;
    }

    memset(mapName, 0, 128);
    if (fileWrite(mapName, 1, 128, _flptr) != 128) {
        return -1;
    }

    // Compute and write header CRC32 covering all header fields.
    // This protects the entire header (signature, version, characterName,
    // description, dates, gameTime, elevation, map, fileName, preview, padding)
    // and specifically guards versionMajor against corruption (F2-20, F2-21).
    // save NEW-4 (MEDIUM): a malloc failure here must ABORT the save, exactly
    // like the handler-loop malloc failure (see "Error allocating CRC buffer").
    // Silently omitting the header CRC produces a save that the load path
    // classifies CORRUPT (it reads the first handler bytes as a CRC and
    // mismatches) — a permanent, undetectable-at-save-time corruption.
    long headerEnd = fileTell(_flptr);
    long headerSize = headerEnd - headerStart;
    if (headerSize > 0) {
        unsigned char* headerBuf = (unsigned char*)internal_malloc(headerSize);
        if (headerBuf == nullptr) {
            debugPrint("\nLOADSAVE: ** Error allocating header CRC buffer! **\n");
            return -1;
        }
        memset(headerBuf, 0, headerSize);
        fileSeek(_flptr, headerStart, SEEK_SET);
        if (fileRead(headerBuf, 1, headerSize, _flptr) != headerSize) {
            internal_free(headerBuf);
            return -1;
        }
        unsigned int headerCrc = _crc32Compute(headerBuf, headerSize);
        internal_free(headerBuf);
        fileSeek(_flptr, headerEnd, SEEK_SET);
        if (fileWriteUInt32(_flptr, headerCrc) == -1) {
            return -1;
        }
    }

    _ls_error_code = 0;

    return 0;
}

// 0x47E2E4
static int lsgLoadHeaderInSlot(int slot)
{
    _ls_error_code = 3;

    long headerStart = fileTell(_flptr);

    LoadSaveSlotData* ptr = &(_LSData[slot]);

    if (fileRead(ptr->signature, 1, 24, _flptr) != 24) {
        return -1;
    }

    if (strncmp(ptr->signature, LOAD_SAVE_SIGNATURE, 18) != 0) {
        debugPrint("\nLOADSAVE: ** Invalid save file on load! **\n");
        _ls_error_code = 2;
        return -1;
    }

    short v8[3];
    if (fileReadInt16List(_flptr, v8, 2) == -1) {
        return -1;
    }

    ptr->versionMinor = v8[0];
    ptr->versionMajor = v8[1];

    if (fileReadUInt8(_flptr, &(ptr->versionRelease)) == -1) {
        return -1;
    }

    // C-03/C-04 (save-format pass): Accept versionMajor (on disk: stores
    // VERSION_MINOR) 2 (1.2R legacy), 3 (1.3R legacy), and 4 (1.4R current).
    if (ptr->versionMinor != 1 || ptr->versionRelease != 'R'
        || (ptr->versionMajor != 2 && ptr->versionMajor != 3 && ptr->versionMajor != 4)) {
        debugPrint("\nLOADSAVE: Load slot #%d Version: %d.%d%c\n", slot, ptr->versionMinor, ptr->versionMajor, ptr->versionRelease);
        _ls_error_code = 1;
        return -1;
    }

    // NEW-1 residual (LOW): Guarantee NUL termination on load. A crafted save
    // can omit the NUL byte inside characterName/description; downstream
    // consumers (_ShowSlotList strcpy, fontDrawText, dudeSetName) then read
    // past the field. The write side already NUL-terminates (I2-084); the load
    // side must enforce the same invariant (the strcpy-overflow claim was
    // REJECTED — byte 91 padding bounds the read — but the NUL gap is real).
    if (fileRead(ptr->characterName, 32, 1, _flptr) != 1) {
        return -1;
    }
    ptr->characterName[sizeof(ptr->characterName) - 1] = '\0';

    if (fileRead(ptr->description, 30, 1, _flptr) != 1) {
        return -1;
    }
    ptr->description[sizeof(ptr->description) - 1] = '\0';

    if (fileReadInt16List(_flptr, v8, 3) == -1) {
        return -1;
    }

    ptr->fileMonth = v8[0];
    ptr->fileDay = v8[1];
    ptr->fileYear = v8[2];

    if (_db_freadInt(_flptr, &(ptr->fileTime)) == -1) {
        return -1;
    }

    if (fileReadInt16List(_flptr, v8, 3) == -1) {
        return -1;
    }

    ptr->gameMonth = v8[0];
    ptr->gameDay = v8[1];
    ptr->gameYear = v8[2];

    if (fileReadUInt32(_flptr, &(ptr->gameTime)) == -1) {
        return -1;
    }

    if (fileReadInt16(_flptr, &(ptr->elevation)) == -1) {
        return -1;
    }

    if (fileReadInt16(_flptr, &(ptr->map)) == -1) {
        return -1;
    }

    if (fileRead(ptr->fileName, 1, 16, _flptr) != 16) {
        return -1;
    }

    if (fileSeek(_flptr, LS_PREVIEW_SIZE, SEEK_CUR) != 0) {
        return -1;
    }

    if (fileSeek(_flptr, 128, 1) != 0) {
        return -1;
    }

    // C-03 (CRITICAL) / save NEW-3 (constraint) / save NEW-4 (write side):
    // Header-CRC interpretation. The 4-byte header CRC covers versionMajor and
    // every other header field (F2-20/F2-21). The version field selects ONLY
    // the legacy-vs-CRC interpretation and the handler layout — it never gates
    // the CRC read inside the CRC era (NEW-3: gating on versionMajor alone
    // would let a version flip disable the CRC that is supposed to detect the
    // flip). Within the CRC era (versionMajor 3/4) the header CRC is read and
    // verified UNCONDITIONALLY:
    //   versionMajor == 2 (1.2R legacy): no header CRC at all — seek to headerEnd.
    //   versionMajor == 3 (1.3R): pass-7..10 saves predate the header CRC — the
    //     next 4 bytes are handler-0's zero placeholder, so a stored value of 0
    //     means "no header CRC present" and is accepted. Pass-11+ 1.3R saves
    //     carry a garbage crc32-of-zeros header CRC (the C-02 read-back failure)
    //     and are rejected here (documented drop — unrecoverable).
    //   versionMajor == 4 (1.4R): header CRC always present; verified
    //     unconditionally. A 4->3 flip changes the header bytes → mismatch.
    long headerEnd = fileTell(_flptr);

    if (ptr->versionMajor == 2) {
        // 1.2R legacy: no header CRC in the stream. Seek back to header end so
        // the handler loop starts at handler-0's data.
        fileSeek(_flptr, headerEnd, SEEK_SET);
    } else {
        unsigned int storedHeaderCrc;
        if (fileReadUInt32(_flptr, &storedHeaderCrc) != 0) {
            // CRC-era save but fewer than 4 bytes remain after the header —
            // truncated/corrupt (the C-03 bug was reading handler data as a
            // CRC; a genuine 1.2R save never reaches this branch).
            debugPrint("\nLOADSAVE: ** Header CRC read failed (truncated save) **\n");
            _ls_error_code = 2;
            return -1;
        }

        long headerSize = headerEnd - headerStart;
        if (headerSize > 0) {
            unsigned char* headerBuf = (unsigned char*)internal_malloc(headerSize);
            if (headerBuf == nullptr) {
                // Fail closed, matching the handler-loop CRC behavior (a
                // malloc failure there also aborts the load). A CRC-era save
                // whose header CRC cannot be verified must not load silently.
                debugPrint("\nLOADSAVE: ** Error allocating header CRC buffer! **\n");
                _ls_error_code = 2;
                return -1;
            }
            memset(headerBuf, 0, headerSize);
            fileSeek(_flptr, headerStart, SEEK_SET);
            if (fileRead(headerBuf, 1, headerSize, _flptr) != headerSize) {
                internal_free(headerBuf);
                _ls_error_code = 2;
                return -1;
            }
            unsigned int computedCrc = _crc32Compute(headerBuf, headerSize);
            internal_free(headerBuf);
            if (computedCrc != storedHeaderCrc) {
                if (storedHeaderCrc == 0 && ptr->versionMajor == SAVE_FORMAT_CRC_VERSION_MAJOR) {
                    // Pass-7..10 1.3R save: no header CRC was written; the
                    // 4 bytes just read are handler-0's zero placeholder.
                    // Accept and seek back to header end so the handler
                    // loop consumes the placeholder as handler-0's CRC.
                    fileSeek(_flptr, headerEnd, SEEK_SET);
                    _ls_error_code = 0;
                    return 0;
                }
                debugPrint("\nLOADSAVE: ** Header CRC mismatch! (stored=%08x, computed=%08x) **\n",
                    storedHeaderCrc, computedCrc);
                _ls_error_code = 2;
                return -1;
            }
        }
        // CRC verified (or headerSize <= 0). Seek back past the CRC to the
        // header end position so subsequent handler CRC reads see the correct
        // offset. The handler loop in lsgLoadGameInSlot starts reading from
        // the current position.
        fileSeek(_flptr, headerEnd + 4, SEEK_SET);
    }

    _ls_error_code = 0;

    return 0;
}

// 0x47E5D0
static int _GetSlotList()
{
    int index = 0;
    for (; index < saveLoadTotalSlots; index += 1) {
        snprintf(_str, sizeof(_str), "%s\\%s%.2d\\%s", "SAVEGAME", "SLOT", index + 1, "SAVE.DAT");

        int fileSize;
        if (dbGetFileSize(_str, &fileSize) != 0) {
            _LSstatus[index] = SLOT_STATE_EMPTY;
        } else {
            _flptr = fileOpen(_str, "rb");

            if (_flptr == nullptr) {
                debugPrint("\nLOADSAVE: ** Error opening save  game for reading! **\n");
                return -1;
            }

            if (lsgLoadHeaderInSlot(index) == -1) {
                if (_ls_error_code == 1) {
                    debugPrint("LOADSAVE: ** save file #%d is an older version! **\n", _slot_cursor);
                    _LSstatus[index] = SLOT_STATE_UNSUPPORTED_VERSION;
                } else {
                    debugPrint("LOADSAVE: ** Save file #%d corrupt! **", index);
                    _LSstatus[index] = SLOT_STATE_ERROR;
                }
            } else {
                _LSstatus[index] = SLOT_STATE_OCCUPIED;
            }

            fileClose(_flptr);
        }
    }
    return index;
}

// 0x47E6D8

static void _ShowSlotList(int windowType)
{
    // Clear display area
    bufferFill(gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 87 + 55, 230, 353, LS_WINDOW_WIDTH, gLoadSaveWindowBuffer[LS_WINDOW_WIDTH * 86 + 55] & 0xFF);

    int y = 87;
    int startIndex = _currentSlotPage * slotsPerPage;
    int endIndex = startIndex + slotsPerPage;
    if (endIndex > saveLoadTotalSlots) endIndex = saveLoadTotalSlots;

    for (int index = startIndex; index < endIndex; index++) {
        int color = index == _slot_cursor ? COLOR_LIGHT_YELLOW : COLOR_GREEN;
        const char* text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, windowType != 0 ? 110 : 109);
        snprintf(_str, sizeof(_str), "[   %s %.2d:   ]", text, index + 1);
        fontDrawText(gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * y + 55, _str, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, color);

        y += fontGetLineHeight();
        switch (_LSstatus[index]) {
        case SLOT_STATE_OCCUPIED:
            strcpy(_str, _LSData[index].description);
            break;
        case SLOT_STATE_EMPTY:
            // - EMPTY -
            text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 111);
            snprintf(_str, sizeof(_str), "       %s", text);
            break;
        case SLOT_STATE_ERROR:
            // - CORRUPT SAVE FILE -
            text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 112);
            snprintf(_str, sizeof(_str), "%s", text);
            color = COLOR_AMBER;
            break;
        case SLOT_STATE_UNSUPPORTED_VERSION:
            // - OLD VERSION -
            text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 113);
            snprintf(_str, sizeof(_str), " %s", text);
            color = COLOR_AMBER;
            break;
        }

        fontDrawText(gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * y + 55, _str, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, color);
        y += 2 * fontGetLineHeight() + 4;
    }

    // Pagination navigation
    if (saveLoadTotalSlots > 10) {
        int activeColor = COLOR_GREEN;
        int inactiveColor = COLOR_LIGHT_GREEN_2;

        {
            MessageListItem messageListItemBack = { 201, 0, nullptr, nullptr };
            // TODO: localize "BACK" and "MORE"
            char backText[] = "BACK";
            if (!messageListGetItem(&gPipboyMessageList, &messageListItemBack)) {
                debugPrint("Error: Couldn't find LoadSave Message!");
                messageListItemBack.text = backText;
            }
            fontDrawText(
                gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * (y + 0) + 95,
                messageListItemBack.text,
                LS_WINDOW_WIDTH,
                LS_WINDOW_WIDTH,
                _currentSlotPage > 0 ? activeColor : inactiveColor);
        }
        {
            MessageListItem messageListItemMore = { 200, 0, nullptr, nullptr };
            char moreText[] = "MORE";
            if (!messageListGetItem(&gPipboyMessageList, &messageListItemMore)) {
                debugPrint("Error: Couldn't find LoadSave Message!");
                messageListItemMore.text = moreText;
            }
            fontDrawText(gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * (y + 0) + 210,
                messageListItemMore.text,
                LS_WINDOW_WIDTH,
                LS_WINDOW_WIDTH,
                _currentSlotPage < saveLoadPages - 1 ? activeColor : inactiveColor);
        }
    }
}

// 0x47E8E0
static void _DrawInfoBox(int slot)
{
    blitBufferToBuffer(_loadsaveFrmImages[LOAD_SAVE_FRM_BACKGROUND].getData() + LS_WINDOW_WIDTH * 253 + 396, 164, 60, LS_WINDOW_WIDTH, gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 253 + 396, 640);

    unsigned char* dest;
    const char* text;
    int color = COLOR_GREEN;

    switch (_LSstatus[slot]) {
    case SLOT_STATE_OCCUPIED:
        if (1) {
            LoadSaveSlotData* ptr = &(_LSData[slot]);
            // raise this one pixel as well to match above
            fontDrawText(gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 253 + 396, ptr->characterName, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, color);

            snprintf(_str,
                sizeof(_str),
                "%.2d %s %.4d   %.4d",
                ptr->gameDay,
                getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 116 + ptr->gameMonth),
                ptr->gameYear,
                100 * ((ptr->gameTime / 600) / 60 % 24) + (ptr->gameTime / 600) % 60);

            int v2 = fontGetLineHeight();
            fontDrawText(gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * (255 + v2) + 397, _str, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, color);

            snprintf(_str,
                sizeof(_str),
                "%s %s",
                mapGetCityName(ptr->map),
                mapGetName(ptr->map, ptr->elevation));

            int y = v2 + 2 + v2 + 255;
            short beginnings[WORD_WRAP_MAX_COUNT];
            short count;
            if (wordWrap(_str, 164, beginnings, &count) == 0) {
                for (int index = 0; index < count - 1; index += 1) {
                    char* beginning = _str + beginnings[index];
                    char* ending = _str + beginnings[index + 1];

                    // Calculate length of the substring
                    size_t lineLength = ending - beginning;

                    // Create a temporary buffer to hold the substring
                    char temp[256]; // Ensure the buffer size is sufficient
                    strncpy(temp, beginning, lineLength);
                    temp[lineLength] = '\0'; // Null-terminate the copied substring

                    // Add a one-pixel shift to the x-coordinate for the second and subsequent lines for tapering readout display
                    int xShift = (index > 0) ? 2 : 0;

                    // Draw the substring
                    fontDrawText(gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * y + 399 + xShift, temp, 164, LS_WINDOW_WIDTH, color);
                    y += v2 + 2;
                }
            }
        }
        return;
    case SLOT_STATE_EMPTY:
        // Empty.
        text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 114);
        dest = gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 262 + 404;
        break;
    case SLOT_STATE_ERROR:
        // Error!
        text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 115);
        dest = gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 262 + 404;
        color = COLOR_AMBER;
        break;
    case SLOT_STATE_UNSUPPORTED_VERSION:
        // Old version.
        text = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 116);
        dest = gLoadSaveWindowBuffer + LS_WINDOW_WIDTH * 262 + 400;
        color = COLOR_AMBER;
        break;
    default:
        assert(false && "Should be unreachable");
    }

    fontDrawText(dest, text, LS_WINDOW_WIDTH, LS_WINDOW_WIDTH, color);
}

// 0x47EC48
static int _LoadTumbSlot(int slot)
{
    if (_LSstatus[_slot_cursor] != SLOT_STATE_EMPTY
        && _LSstatus[_slot_cursor] != SLOT_STATE_ERROR
        && _LSstatus[_slot_cursor] != SLOT_STATE_UNSUPPORTED_VERSION) {
        snprintf(_str, sizeof(_str), "%s\\%s%.2d\\%s", "SAVEGAME", "SLOT", _slot_cursor + 1, "SAVE.DAT");
        debugPrint(" Filename %s\n", _str);

        File* stream = fileOpen(_str, "rb");
        if (stream == nullptr) {
            debugPrint("\nLOADSAVE: ** (A) Error reading thumbnail #%d! **\n", slot);
            return -1;
        }

        if (fileSeek(stream, 131, SEEK_SET) != 0) {
            debugPrint("\nLOADSAVE: ** (B) Error reading thumbnail #%d! **\n", slot);
            fileClose(stream);
            return -1;
        }

        if (fileRead(_thumbnail_image, LS_PREVIEW_SIZE, 1, stream) != 1) {
            debugPrint("\nLOADSAVE: ** (C) Error reading thumbnail #%d! **\n", slot);
            fileClose(stream);
            return -1;
        }

        fileClose(stream);
    }

    return 0;
}

// 0x47ED5C
static int _GetComment(int slot)
{
    // Maintain original position in original resolution, otherwise center it.
    int commentWindowX = screenGetWidth() != 640
        ? (screenGetWidth() - _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth()) / 2
        : LS_COMMENT_WINDOW_X;
    int commentWindowY = screenGetHeight() != 480
        ? (screenGetHeight() - _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getHeight()) / 2
        : LS_COMMENT_WINDOW_Y;
    int window = windowCreate(commentWindowX,
        commentWindowY,
        _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getHeight(),
        256,
        WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (window == -1) {
        return -1;
    }

    unsigned char* windowBuffer = windowGetBuffer(window);
    memcpy(windowBuffer,
        _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getData(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getHeight() * _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth());

    fontSetCurrent(103);

    const char* msg;

    // DONE
    msg = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 104);
    fontDrawText(windowBuffer + _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth() * 57 + 56,
        msg,
        _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth(),
        COLOR_DARK_YELLOW);

    // CANCEL
    msg = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 105);
    fontDrawText(windowBuffer + _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth() * 57 + 181,
        msg,
        _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth(),
        COLOR_DARK_YELLOW);

    // DESCRIPTION
    msg = getmsg(&gLoadSaveMessageList, &gLoadSaveMessageListItem, 130);

    char title[260];
    strcpy(title, msg);

    int width = fontGetStringWidth(title);
    fontDrawText(windowBuffer + _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth() * 7 + (_loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth() - width) / 2,
        title,
        _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth(),
        COLOR_DARK_YELLOW);

    fontSetCurrent(101);

    int btn;

    // DONE
    btn = buttonCreate(window,
        34,
        58,
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].getWidth(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].getHeight(),
        -1,
        -1,
        -1,
        507,
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_NORMAL].getData(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    // CANCEL
    btn = buttonCreate(window,
        160,
        58,
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].getWidth(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].getHeight(),
        -1,
        -1,
        -1,
        508,
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_NORMAL].getData(),
        _loadsaveFrmImages[LOAD_SAVE_FRM_RED_BUTTON_PRESSED].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (btn != -1) {
        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    windowRefresh(window);

    char description[LOAD_SAVE_DESCRIPTION_LENGTH];
    if (_LSstatus[_slot_cursor] == SLOT_STATE_OCCUPIED) {
        strncpy(description, _LSData[slot].description, LOAD_SAVE_DESCRIPTION_LENGTH);
        // I2-085: strncpy does not guarantee null termination when source >= LOAD_SAVE_DESCRIPTION_LENGTH.
        description[LOAD_SAVE_DESCRIPTION_LENGTH - 1] = '\0';
    } else {
        memset(description, '\0', LOAD_SAVE_DESCRIPTION_LENGTH);
    }

    int rc;

    int backgroundColor = *(_loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getData() + _loadsaveFrmImages[LOAD_SAVE_FRM_BOX].getWidth() * 35 + 24);
    if (_get_input_str2(window, 507, 508, description, LOAD_SAVE_DESCRIPTION_LENGTH - 1, 24, 35, COLOR_GREEN, backgroundColor, 0) == 0) {
        strncpy(_LSData[slot].description, description, LOAD_SAVE_DESCRIPTION_LENGTH);
        _LSData[slot].description[LOAD_SAVE_DESCRIPTION_LENGTH - 1] = '\0';
        rc = 1;
    } else {
        rc = 0;
    }

    windowDestroy(window);

    return rc;
}

// 0x47F084
static int _get_input_str2(int win, int doneKeyCode, int cancelKeyCode, char* description, int maxLength, int x, int y, int textColor, int backgroundColor, int flags)
{
    int cursorWidth = fontGetStringWidth("_") - 4;
    int windowWidth = windowGetWidth(win);
    int lineHeight = fontGetLineHeight();
    unsigned char* windowBuffer = windowGetBuffer(win);
    if (maxLength > 255) {
        maxLength = 255;
    }

    char text[256];
    strcpy(text, description);

    size_t textLength = strlen(text);
    text[textLength] = ' ';
    text[textLength + 1] = '\0';

    int nameWidth = fontGetStringWidth(text);

    bufferFill(windowBuffer + windowWidth * y + x, nameWidth, lineHeight, windowWidth, backgroundColor);
    fontDrawText(windowBuffer + windowWidth * y + x, text, windowWidth, windowWidth, textColor);

    windowRefresh(win);
    renderPresent();

    beginTextInput();

    int blinkCounter = 3;
    bool blink = false;

    int v1 = 0;

    int rc = 1;
    while (rc == 1) {
        sharedFpsLimiter.mark();

        int tick = getTicks();

        int keyCode = inputGetInput();
        if ((keyCode & 0x80000000) == 0) {
            v1++;
        }

        if (keyCode == doneKeyCode || keyCode == KEY_RETURN) {
            rc = 0;
        } else if (keyCode == cancelKeyCode || keyCode == KEY_ESCAPE) {
            rc = -1;
        } else {
            if ((keyCode == KEY_DELETE || keyCode == KEY_BACKSPACE) && textLength > 0) {
                bufferFill(windowBuffer + windowWidth * y + x, fontGetStringWidth(text), lineHeight, windowWidth, backgroundColor);

                // TODO: Probably incorrect, needs testing.
                if (v1 == 1) {
                    textLength = 1;
                }

                text[textLength - 1] = ' ';
                text[textLength] = '\0';
                fontDrawText(windowBuffer + windowWidth * y + x, text, windowWidth, windowWidth, textColor);
                textLength--;
            } else if ((keyCode >= KEY_FIRST_INPUT_CHARACTER && keyCode <= KEY_LAST_INPUT_CHARACTER) && textLength < maxLength) {
                if ((flags & 0x01) != 0) {
                    if (!_isdoschar(keyCode)) {
                        break;
                    }
                }

                bufferFill(windowBuffer + windowWidth * y + x, fontGetStringWidth(text), lineHeight, windowWidth, backgroundColor);

                text[textLength] = keyCode & 0xFF;
                text[textLength + 1] = ' ';
                text[textLength + 2] = '\0';
                fontDrawText(windowBuffer + windowWidth * y + x, text, windowWidth, windowWidth, textColor);
                textLength++;

                windowRefresh(win);
            }
        }

        blinkCounter -= 1;
        if (blinkCounter == 0) {
            blinkCounter = 3;
            blink = !blink;

            int color = blink ? backgroundColor : textColor;
            bufferFill(windowBuffer + windowWidth * y + x + fontGetStringWidth(text) - cursorWidth, cursorWidth, lineHeight - 2, windowWidth, color);
            windowRefresh(win);
        }

        delay_ms(1000 / 24 - (getTicks() - tick));

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    endTextInput();

    if (rc == 0) {
        text[textLength] = '\0';
        strcpy(description, text);
    }

    return rc;
}

// 0x47F48C
static int _DummyFunc(File* stream)
{
    return 0;
}

// 0x47F490
static int _PrepLoad(File* stream)
{
    gameReset();
    gameMouseSetCursor(MOUSE_CURSOR_WAIT_PLANET);
    gMapHeader.name[0] = '\0';
    gameTimeSetTime(_LSData[_slot_cursor].gameTime);
    return 0;
}

// 0x47F4C8
static int _EndLoad(File* stream)
{
    wmMapMusicStart();
    dudeSetName(_LSData[_slot_cursor].characterName);
    interfaceBarRefresh();
    indicatorBarRefresh();
    tileWindowRefresh();
    if (isInCombat()) {
        scriptsRequestCombat(nullptr);
    }
    return 0;
}

// 0x47F510
static int _GameMap2Slot(File* stream)
{
    if (_partyMemberPrepSave() == -1) {
        return -1;
    }

    if (_map_save_in_game(false) == -1) {
        return -1;
    }

    for (int index = 1; index < gPartyMemberDescriptionsLength; index += 1) {
        int pid = gPartyMemberPids[index];
        if (pid == -2) {
            continue;
        }

        char path[COMPAT_MAX_PATH];
        if (_proto_list_str(pid, path) != 0) {
            continue;
        }

        const char* critterItemPath = (pid >> 24) == OBJ_TYPE_CRITTER
            ? PROTO_DIR_NAME "\\" CRITTERS_DIR_NAME
            : PROTO_DIR_NAME "\\" ITEMS_DIR_NAME;
        snprintf(_str0, sizeof(_str0), "%s\\%s\\%s", _patches, critterItemPath, path);
        snprintf(_str1, sizeof(_str1), "%s\\%s\\%s%.2d\\%s\\%s", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1, critterItemPath, path);
        if (fileCopyCompressed(_str0, _str1) == -1) {
            return -1;
        }
    }

    snprintf(_str0, sizeof(_str0), "%s\\*.%s", "MAPS", "SAV");

    char** fileNameList;
    int fileNameListLength = fileNameListInit(_str0, &fileNameList);
    if (fileNameListLength == -1) {
        return -1;
    }

    if (fileWriteInt32(stream, fileNameListLength) == -1) {
        fileNameListFree(&fileNameList, 0);
        return -1;
    }

    if (fileNameListLength == 0) {
        fileNameListFree(&fileNameList, 0);
        return -1;
    }

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);

    if (MapDirErase(_gmpath, "SAV") == -1) {
        fileNameListFree(&fileNameList, 0);
        return -1;
    }

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);
    _strmfe(_str0, "AUTOMAP.DB", "SAV");
    strcat(_gmpath, _str0);
    compat_remove(_gmpath);

    for (int index = 0; index < fileNameListLength; index += 1) {
        char* string = fileNameList[index];
        if (fileWrite(string, strlen(string) + 1, 1, stream) == -1) {
            fileNameListFree(&fileNameList, 0);
            return -1;
        }

        snprintf(_str0, sizeof(_str0), "%s\\%s\\%s", _patches, "MAPS", string);
        snprintf(_str1, sizeof(_str1), "%s\\%s\\%s%.2d\\%s", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1, string);
        if (fileCopyCompressed(_str0, _str1) == -1) {
            fileNameListFree(&fileNameList, 0);
            return -1;
        }
    }

    fileNameListFree(&fileNameList, 0);

    _strmfe(_str0, "AUTOMAP.DB", "SAV");
    snprintf(_str1, sizeof(_str1), "%s\\%s\\%s%.2d\\%s", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1, _str0);
    snprintf(_str0, sizeof(_str0), "%s\\%s\\%s", _patches, "MAPS", "AUTOMAP.DB");

    if (fileCopyCompressed(_str0, _str1) == -1) {
        return -1;
    }

    snprintf(_str0, sizeof(_str0), "%s\\%s", "MAPS", "AUTOMAP.DB");
    File* inStream = fileOpen(_str0, "rb");
    if (inStream == nullptr) {
        return -1;
    }

    int fileSize = fileGetSize(inStream);
    if (fileSize == -1) {
        fileClose(inStream);
        return -1;
    }

    fileClose(inStream);

    if (fileWriteInt32(stream, fileSize) == -1) {
        return -1;
    }

    if (_partyMemberUnPrepSave() == -1) {
        return -1;
    }

    return 0;
}

// SlotMap2Game
// 0x47F990
static int _SlotMap2Game(File* stream)
{
    debugPrint("LOADSAVE: in SlotMap2Game\n");

    int fileNameListLength;
    if (fileReadInt32(stream, &fileNameListLength) == -1) {
        debugPrint("LOADSAVE: returning 1\n");
        return -1;
    }

    if (fileNameListLength == 0) {
        debugPrint("LOADSAVE: returning 2\n");
        return -1;
    }

    snprintf(_str0, sizeof(_str0), "%s\\", PROTO_DIR_NAME "\\" CRITTERS_DIR_NAME);

    if (MapDirErase(_str0, PROTO_FILE_EXT) == -1) {
        debugPrint("LOADSAVE: returning 3\n");
        return -1;
    }

    snprintf(_str0, sizeof(_str0), "%s\\", PROTO_DIR_NAME "\\" ITEMS_DIR_NAME);
    if (MapDirErase(_str0, PROTO_FILE_EXT) == -1) {
        debugPrint("LOADSAVE: returning 4\n");
        return -1;
    }

    snprintf(_str0, sizeof(_str0), "%s\\", "MAPS");
    if (MapDirErase(_str0, "SAV") == -1) {
        debugPrint("LOADSAVE: returning 5\n");
        return -1;
    }

    snprintf(_str0, sizeof(_str0), "%s\\%s\\%s", _patches, "MAPS", "AUTOMAP.DB");
    compat_remove(_str0);

    for (int index = 1; index < gPartyMemberDescriptionsLength; index += 1) {
        int pid = gPartyMemberPids[index];
        if (pid != -2) {
            char protoPath[COMPAT_MAX_PATH];
            if (_proto_list_str(pid, protoPath) == 0) {
                const char* basePath = objectTypeFromPid(pid) == OBJ_TYPE_CRITTER
                    ? PROTO_DIR_NAME "\\" CRITTERS_DIR_NAME
                    : PROTO_DIR_NAME "\\" ITEMS_DIR_NAME;
                snprintf(_str0, sizeof(_str0), "%s\\%s\\%s", _patches, basePath, protoPath);
                snprintf(_str1, sizeof(_str1), "%s\\%s\\%s%.2d\\%s\\%s", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1, basePath, protoPath);

                if (_gzdecompress_file(_str1, _str0) == -1) {
                    debugPrint("LOADSAVE: returning 6\n");
                    return -1;
                }
            }
        }
    }

    for (int index = 0; index < fileNameListLength; index += 1) {
        char fileName[COMPAT_MAX_PATH];
        if (_mygets(fileName, stream) == -1) {
            break;
        }

        // Reject file names containing path traversal components ("..", "/",
        // absolute paths) to prevent crafted saves from escaping the save
        // directory (F2-22). Matches existing checks in sfall_ext.cc:224
        // and sfall_metarules.cc:1269.
        if (compat_path_contains_traversal(fileName)) {
            debugPrint("\nLOADSAVE: ** Rejecting unsafe map file name (path traversal): %s **\n", fileName);
            continue;
        }

        snprintf(_str0, sizeof(_str0), "%s\\%s\\%s%.2d\\%s", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1, fileName);
        snprintf(_str1, sizeof(_str1), "%s\\%s\\%s", _patches, "MAPS", fileName);

        if (_gzdecompress_file(_str0, _str1) == -1) {
            debugPrint("LOADSAVE: returning 7\n");
            return -1;
        }
    }

    const char* automapFileName = _strmfe(_str1, "AUTOMAP.DB", "SAV");
    snprintf(_str0, sizeof(_str0), "%s\\%s\\%s%.2d\\%s", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1, automapFileName);
    snprintf(_str1, sizeof(_str1), "%s\\%s\\%s", _patches, "MAPS", "AUTOMAP.DB");
    if (fileCopyDecompressed(_str0, _str1) == -1) {
        debugPrint("LOADSAVE: returning 8\n");
        return -1;
    }

    snprintf(_str1, sizeof(_str1), "%s\\%s", "MAPS", "AUTOMAP.DB");

    int v12;
    if (fileReadInt32(stream, &v12) == -1) {
        debugPrint("LOADSAVE: returning 9\n");
        return -1;
    }

    if (mapLoadSaved(_LSData[_slot_cursor].fileName) == -1) {
        debugPrint("LOADSAVE: returning 13\n");
        return -1;
    }

    return 0;
}

// Reads a null-terminated string from a save game stream into dest.
// Maximum readable length is 15 non-null characters + null terminator (16 bytes
// total), matching the save format's fileName field size.
//
// Returns 0 on success, -1 on error (buffer overflow without null termination,
// or EOF before a complete string is read).
//
// FIX: The original implementation had two bugs:
//   (a) 14-byte null-terminated strings (13 chars + null) were erroneously
//       rejected because the post-loop check `index == 0` failed to
//       distinguish "buffer full, null found" from "buffer full, no null."
//   (b) 15+ non-null bytes returned SUCCESS without null termination,
//       because index reached -1 before a null byte was encountered.
// The corrected version uses an explicit count-based loop that properly
// tracks whether a null terminator was found.
static int _mygets(char* dest, File* stream)
{
    static const int kMaxChars = 15;
    int count;
    for (count = 0; count < kMaxChars; count++) {
        int c = fileReadChar(stream);
        if (c == -1) {
            // EOF before null terminator — error.
            dest[count] = '\0';
            return -1;
        }
        dest[count] = c & 0xFF;
        if (c == '\0') {
            // Null terminator found within the buffer — success.
            return 0;
        }
    }
    // Read exactly kMaxChars non-null bytes. Read one more byte —
    // this must be the null terminator.
    int c = fileReadChar(stream);
    if (c == -1) {
        // EOF at the null position — treat as valid (last byte implied null).
        dest[kMaxChars] = '\0';
        return 0;
    }
    dest[kMaxChars] = c & 0xFF;
    if (c == '\0') {
        // 15 chars + null = 16 bytes total — valid.
        return 0;
    }
    // Buffer full without a null terminator — error.
    // Null-terminate what we have to prevent unbounded string reads.
    dest[kMaxChars] = '\0';
    return -1;
}

// 0x47FE58
static int _copy_file(const char* existingFileName, const char* newFileName)
{
    File* stream1;
    File* stream2;
    int length;
    int chunk_length;
    void* buf;
    int result;

    stream1 = nullptr;
    stream2 = nullptr;
    buf = nullptr;
    result = -1;

    stream1 = fileOpen(existingFileName, "rb");
    if (stream1 == nullptr) {
        goto out;
    }

    length = fileGetSize(stream1);
    if (length == -1) {
        goto out;
    }

    stream2 = fileOpen(newFileName, "wb");
    if (stream2 == nullptr) {
        goto out;
    }

    buf = internal_malloc(0xFFFF);
    if (buf == nullptr) {
        goto out;
    }

    while (length != 0) {
        chunk_length = std::min(length, 0xFFFF);

        if (fileRead(buf, chunk_length, 1, stream1) != 1) {
            break;
        }

        if (fileWrite(buf, chunk_length, 1, stream2) != 1) {
            break;
        }

        length -= chunk_length;
    }

    if (length != 0) {
        goto out;
    }

    result = 0;

out:

    if (stream1 != nullptr) {
        fileClose(stream1);
    }

    if (stream2 != nullptr) {
        fileClose(stream2);
    }

    if (buf != nullptr) {
        internal_free(buf);
    }

    return result;
}

// InitLoadSave
// 0x48000C
void lsgInit()
{
    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s\\", "MAPS");
    MapDirErase(path, "SAV");
}

// 0x480040
int MapDirErase(const char* relativePath, const char* extension)
{
    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s*.%s", relativePath, extension);

    char** fileList;
    int fileListLength = fileNameListInit(path, &fileList);
    while (--fileListLength >= 0) {
        snprintf(path, sizeof(path), "%s\\%s%s", _patches, relativePath, fileList[fileListLength]);
        compat_remove(path);
    }
    fileNameListFree(&fileList, 0);

    return 0;
}

// 0x4800C8
int _MapDirEraseFile_(const char* relativePath, const char* fileName)
{
    char path[COMPAT_MAX_PATH];

    snprintf(path, sizeof(path), "%s\\%s%s", _patches, relativePath, fileName);
    if (compat_remove(path) != 0) {
        return -1;
    }

    return 0;
}

// 0x480104
static int _SaveBackup()
{
    debugPrint("\nLOADSAVE: Backing up save slot files..\n");

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);
    strcpy(_str0, _gmpath);

    strcat(_str0, "SAVE.DAT");

    _strmfe(_str1, _str0, "BAK");

    File* stream1 = fileOpen(_str0, "rb");
    if (stream1 != nullptr) {
        fileClose(stream1);
        if (compat_rename(_str0, _str1) != 0) {
            return -1;
        }
    }

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
    snprintf(_str0, sizeof(_str0), "%s*.%s", _gmpath, "SAV");

    char** fileList;
    int fileListLength = fileNameListInit(_str0, &fileList);
    if (fileListLength == -1) {
        return -1;
    }

    _map_backup_count = fileListLength;

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);
    for (int index = fileListLength - 1; index >= 0; index--) {
        strcpy(_str0, _gmpath);
        strcat(_str0, fileList[index]);

        _strmfe(_str1, _str0, "BAK");
        if (compat_rename(_str0, _str1) != 0) {
            fileNameListFree(&fileList, 0);
            _map_backup_count = 0;
            return -1;
        }
    }

    fileNameListFree(&fileList, 0);

    debugPrint("\nLOADSAVE: %d map files backed up.\n", fileListLength);

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);

    char* v1 = _strmfe(_str2, "AUTOMAP.DB", "SAV");
    snprintf(_str0, sizeof(_str0), "%s\\%s", _gmpath, v1);

    char* v2 = _strmfe(_str2, "AUTOMAP.DB", "BAK");
    snprintf(_str1, sizeof(_str1), "%s\\%s", _gmpath, v2);

    _automap_db_flag = false;

    File* stream2 = fileOpen(_str0, "rb");
    if (stream2 != nullptr) {
        fileClose(stream2);

        if (_copy_file(_str0, _str1) == -1) {
            _map_backup_count = 0;
            return -1;
        }

        _automap_db_flag = true;
    }

    // F-036: Backup sfallgv.sav alongside SAVE.DAT.
    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
    snprintf(_str0, sizeof(_str0), "%ssfallgv.sav", _gmpath);
    snprintf(_str1, sizeof(_str1), "%ssfallgv.bak", _gmpath);

    File* sfallgvBackupStream = fileOpen(_str0, "rb");
    if (sfallgvBackupStream != nullptr) {
        fileClose(sfallgvBackupStream);
        if (compat_rename(_str0, _str1) != 0) {
            _map_backup_count = 0;
            return -1;
        }
    }

    return 0;
}

// 0x4803D8
static int _RestoreSave()
{
    debugPrint("\nLOADSAVE: Restoring save file backup...\n");

    _EraseSave();

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);
    strcpy(_str0, _gmpath);
    strcat(_str0, "SAVE.DAT");
    _strmfe(_str1, _str0, "BAK");
    compat_remove(_str0);

    if (compat_rename(_str1, _str0) != 0) {
        _EraseSave();
        return -1;
    }

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
    snprintf(_str0, sizeof(_str0), "%s*.%s", _gmpath, "BAK");

    char** fileList;
    int fileListLength = fileNameListInit(_str0, &fileList);
    if (fileListLength == -1) {
        return -1;
    }

    if (fileListLength != _map_backup_count) {
        // save N-01 (MEDIUM): NEVER erase the slot on backup-count mismatch.
        // A crash between _SaveBackup and _RestoreSave (or a partial-backup
        // failure) can leave the slot all-.BAK; erasing here would destroy
        // the player's previous save (the freshly-restored SAVE.DAT plus any
        // still-.BAK maps). Instead, restore whatever .BAK files exist — the
        // rename loop below handles every file present, and the caller's
        // subsequent MapDirErase("BAK") cleans up any leftover backups.
        debugPrint("\nLOADSAVE: ** Backup count mismatch (backup=%d, restore=%d) — restoring what exists **\n",
            _map_backup_count, fileListLength);
        // Fall through to the restore loop. The rename loop renames every
        // .BAK file in the list back to .SAV; files whose .BAK is missing
        // simply leave no .SAV (best effort, never destructive).
    }

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);

    for (int index = fileListLength - 1; index >= 0; index--) {
        strcpy(_str0, _gmpath);
        strcat(_str0, fileList[index]);
        _strmfe(_str1, _str0, "SAV");
        compat_remove(_str1);
        if (compat_rename(_str0, _str1) != 0) {
            fileNameListFree(&fileList, 0);
            _EraseSave();
            return -1;
        }
    }

    fileNameListFree(&fileList, 0);

    if (!_automap_db_flag) {
        return 0;
    }

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);
    char* v1 = _strmfe(_str2, "AUTOMAP.DB", "BAK");
    strcpy(_str0, _gmpath);
    strcat(_str0, v1);

    char* v2 = _strmfe(_str2, "AUTOMAP.DB", "SAV");
    strcpy(_str1, _gmpath);
    strcat(_str1, v2);

    if (compat_rename(_str0, _str1) != 0) {
        _EraseSave();
        return -1;
    }

    // F-036: Restore sfallgv.sav from backup. Non-fatal: the backup
    // may not exist if this save was created before the F-036 fix.
    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
    snprintf(_str0, sizeof(_str0), "%ssfallgv.sav", _gmpath);
    snprintf(_str1, sizeof(_str1), "%ssfallgv.bak", _gmpath);
    compat_remove(_str0);
    if (compat_rename(_str1, _str0) != 0) {
        // Restore from backup failed — clean up the leftover .bak file
        // so case-sensitive filesystems don't accumulate orphaned backups.
        debugPrint("\nLOADSAVE: Warning, failed to restore sfallgv.sav from backup.\n");
        compat_remove(_str1);
    }

    return 0;
}

// 0x480710
static int _LoadObjDudeCid(File* stream)
{
    int value;

    if (fileReadInt32(stream, &value) == -1) {
        return -1;
    }

    gDude->cid = value;

    return 0;
}

// 0x480734
static int _SaveObjDudeCid(File* stream)
{
    return fileWriteInt32(stream, gDude->cid);
}

// 0x480754
static int _EraseSave()
{
    debugPrint("\nLOADSAVE: Erasing save(bad) slot...\n");

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);
    // Construct full path with snprintf to avoid potential overflow from
    // strcat after truncated snprintf (H-17).
    snprintf(_str0, sizeof(_str0), "%sSAVE.DAT", _gmpath);
    compat_remove(_str0);

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
    snprintf(_str0, sizeof(_str0), "%s*.%s", _gmpath, "SAV");

    char** fileList;
    int fileListLength = fileNameListInit(_str0, &fileList);
    if (fileListLength == -1) {
        return -1;
    }

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);
    for (int index = fileListLength - 1; index >= 0; index--) {
        snprintf(_str0, sizeof(_str0), "%s%s", _gmpath, fileList[index]);
        compat_remove(_str0);
    }

    fileNameListFree(&fileList, 0);

    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s\\%s%.2d\\", _patches, "SAVEGAME", "SLOT", _slot_cursor + 1);

    char* v1 = _strmfe(_str1, "AUTOMAP.DB", "SAV");
    snprintf(_str0, sizeof(_str0), "%s%s", _gmpath, v1);

    compat_remove(_str0);

    // F-036: Remove sfallgv.sav during erase.
    snprintf(_gmpath, sizeof(_gmpath), "%s\\%s%.2d\\", "SAVEGAME", "SLOT", _slot_cursor + 1);
    snprintf(_str0, sizeof(_str0), "%ssfallgv.sav", _gmpath);
    compat_remove(_str0);

    return 0;
}

} // namespace fallout
