#ifndef RA2OB_SRC_GAME_HPP_
#define RA2OB_SRC_GAME_HPP_

#include <algorithm>
#include <array>
#include <sstream>
#include <string>
#include <thread>  // NOLINT
#include <vector>

#include "./Viewer.hpp"
// clang-format off
#include <psapi.h> // NOLINT
#include <TlHelp32.h>
// clang-format on

namespace Ra2ob {

class Game {
public:
    static Game& getInstance();

    Game(const Game&)           = delete;
    void operator=(const Game&) = delete;

    void getHandle();
    DisplayMode getDisplayMode(bool fullscreen, bool windowed, bool border);
    void initAddrs();
    std::string getMapNameFromFile(const std::string& mapFile);

    void loadNumericsFromJson(std::string filePath = F_PANELOFFSETS);
    void loadUnitsFromJson(std::string filePath = F_UNITOFFSETS);
    void initStrTypes();
    void initArrays();
    void initGameInfo();

    int hasPlayer();

    void refreshInfo();
    void getBuildingInfo(tagBuildingInfo* bi, int addr, int offset_0, int offset_1, UnitType utype);
    void refreshBuildingInfos();
    void refreshSuperTimer();
    void refreshColors();
    void refreshStatusInfos();
    void refreshScoreInfos();
    void refreshGameInfos();

    void structBuild();

    void restart(bool valid);

    void detectTask(int interval = 500);
    void fetchTask(int inferval = 500);
    void startLoop();

    // 获取游戏实际时间(秒)
    int getGameRealTime() {
        return r.getInt(GAMETIMEOFFSET);
    }

    tagNumerics _numerics;
    tagUnits _units;
    tagGameInfo _gameInfo;

    StrName _strName;
    StrCountry _strCountry;

    std::array<tagBuildingInfo, MAXPLAYER> _buildingInfos;
    std::array<tagSuperTimer, MAXPLAYER> _superTimers;
    std::array<tagStatusInfo, MAXPLAYER> _statusInfos;
    std::array<tagScoreInfo, MAXPLAYER> _scoreInfos;

    std::array<std::string, MAXPLAYER> _colors;

    std::array<bool, MAXPLAYER> _players;
    std::array<uint32_t, MAXPLAYER> _playerBases;

    std::array<uint32_t, MAXPLAYER> _buildings;
    std::array<uint32_t, MAXPLAYER> _infantrys;
    std::array<uint32_t, MAXPLAYER> _tanks;
    std::array<uint32_t, MAXPLAYER> _aircrafts;

    std::array<uint32_t, MAXPLAYER> _buildings_valid;
    std::array<uint32_t, MAXPLAYER> _infantrys_valid;
    std::array<uint32_t, MAXPLAYER> _tanks_valid;
    std::array<uint32_t, MAXPLAYER> _aircrafts_valid;

    std::array<uint32_t, MAXPLAYER> _houseTypes;

    std::array<uint32_t, MAXPLAYER> _playerTeamNumber;
    std::array<bool, MAXPLAYER> _playerDefeatFlag;
    std::array<bool, MAXPLAYER> _playerGameoverFlag;
    std::array<bool, MAXPLAYER> _playerWinnerFlag;

    Reader r;
    Viewer viewer;
    Version version        = Version::Yr;
    bool isReplay          = false;
    std::string mapName    = "";
    std::string mapNameUtf = "";

private:
    Game();
    ~Game();
};

inline Game& Game::getInstance() {
    static Game instance;
    return instance;
}

inline Game::Game() {
    loadNumericsFromJson();
    loadUnitsFromJson();
    initStrTypes();
    initArrays();
    initGameInfo();
}

inline Game::~Game() {
    if (r.getHandle() != nullptr) {
        CloseHandle(r.getHandle());
    }
}

/**
 * Get game handle, set Reader.
 */
inline void Game::getHandle() {
    DWORD pid = 0;

    std::wstring w_name = L"gamemd-spawn.exe";
    std::string name    = "gamemd-spawn.exe";

    HANDLE hProcessSnap = INVALID_HANDLE_VALUE;
    hProcessSnap        = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    HANDLE hThreadSnap = INVALID_HANDLE_VALUE;
    hThreadSnap        = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create process snapshot\n";
        return;
    }

    if (hThreadSnap == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create thread snapshot\n";
        return;
    }

    PROCESSENTRY32 processInfo{};
    processInfo.dwSize = sizeof(PROCESSENTRY32);

    for (BOOL success = Process32First(hProcessSnap, &processInfo); success;
         success      = Process32Next(hProcessSnap, &processInfo)) {
        BOOL processMatch = false;

#ifdef UNICODE
        if (wcscmp(processInfo.szExeFile, w_name.c_str()) == 0) {
            processMatch = true;
        }
#else
        if (name == processInfo.szExeFile) {
            processMatch = true;
        }
#endif

        if (processMatch) {
            THREADENTRY32 threadInfo{};
            threadInfo.dwSize = sizeof(THREADENTRY32);

            pid = processInfo.th32ProcessID;

            int thread_nums = 0;

            for (BOOL success = Thread32First(hThreadSnap, &threadInfo); success;
                 success      = Thread32Next(hThreadSnap, &threadInfo)) {
                if (threadInfo.th32OwnerProcessID == pid) {
                    thread_nums++;
                }
            }

            if (thread_nums != 0) {
                break;
            }

            pid = 0;
        }
    }

    if (pid == 0) {
        r = Reader(nullptr);
        return;
    }

    HANDLE pHandle = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_READ,
        FALSE, pid);

    if (pHandle == nullptr) {
        std::cerr << "Could not open process\n";
        r = Reader(nullptr);
        return;
    }

    r = Reader(pHandle);

#ifdef UNICODE
    wchar_t exePath[256];
    GetModuleFileNameEx(pHandle, NULL, exePath, sizeof(exePath));
    std::string filePath = utf16ToGbk(exePath);
#else
    char exePath[256];
    GetModuleFileNameEx(pHandle, NULL, exePath, sizeof(exePath));
    std::string filePath = exePath;
#endif

    std::string gamePath = filePath;
    std::string destPart = "gamemd-spawn.exe";

    // Get info from spawnini
    std::string spawnini = "spawn.ini";
    std::string spawniniPath =
        filePath.replace(filePath.find(destPart), destPart.length(), spawnini);
    destPart = spawnini;

    std::string spawniniSettings = "Settings";
    IniFile sif(spawniniPath, spawniniSettings);
    std::string gameVersion = "GameVersion";
    std::string ra2Mode     = "Ra2Mode";
    std::string recordFile  = "RecordFile";
    std::string lbMapName   = "LBMapName";

    if (sif.isItemExist(gameVersion)) {
        if (sif.getItemBool(gameVersion)) {
            version = Version::Ra2;
        } else {
            version = Version::Yr;
        }
    } else if (sif.isItemExist(ra2Mode)) {
        if (sif.getItemBool(ra2Mode)) {
            version = Version::Ra2;
        } else {
            version = Version::Yr;
        }
    }

    if (sif.isItemExist(recordFile)) {
        isReplay = true;
    } else {
        isReplay = false;
    }

    bool isFromMapFile = false;
    if (sif.isItemExist(lbMapName)) {
        mapName = sif.getItem(lbMapName);
        isFromMapFile = false;
    } else {
        // 如果LBMapName不存在，尝试读取mapfile字段
        std::string mapFile = "mapfile";
        if (sif.isItemExist(mapFile)) {
            std::string mapFileValue = sif.getItem(mapFile);
            mapName = getMapNameFromFile(mapFileValue);
            isFromMapFile = true;
        }
    }

    // 对mapName进行编码转换，确保正确显示中文
    if (mapName.empty()) {
        mapNameUtf = "";
    } else {
        if (isFromMapFile) {
            // 如果mapName来自getMapNameFromFile，源代码中的中文字符串已经是UTF-8编码，直接使用
            mapNameUtf = mapName;
        } else {
            // 如果mapName来自LBMapName，ini文件通常使用GBK编码，需要转换为UTF-8
            mapNameUtf = utf16ToUtf8(gbkToUtf16(mapName.c_str()).c_str());
        }
    }

    // Get info from RA2MD.ini
    std::string ra2mdini = "RA2MD.ini";
    std::string ra2mdiniPath =
        filePath.replace(filePath.find(destPart), destPart.length(), ra2mdini);
    destPart = ra2mdini;

    std::string ra2mdiniSettings = "Video";
    IniFile rif(ra2mdiniPath, ra2mdiniSettings);
    std::string screenWidth  = "ScreenWidth";
    std::string screenHeight = "ScreenHeight";
    int sw                   = 0;
    int sh                   = 0;

    if (rif.isItemExist(screenWidth)) {
        sw = rif.getItemInt(screenWidth);
    }

    if (rif.isItemExist(screenHeight)) {
        sh = rif.getItemInt(screenHeight);
    }

    // Get info from ddraw.ini
    std::string ddrawini = "ddraw.ini";
    std::string ddrawiniPath =
        filePath.replace(filePath.find(destPart), destPart.length(), ddrawini);
    destPart = ddrawini;

    std::string ddrawiniSettings = "ddraw";
    IniFile dif(ddrawiniPath, ddrawiniSettings);
    std::string fullScreen = "fullscreen";
    std::string windowed   = "windowed";
    std::string border     = "border";
    std::string renderer   = "renderer";

    bool fs_bool  = false;
    bool win_bool = false;
    bool bor_bool = false;
    std::string renderer_string;

    if (dif.isItemExist(fullScreen)) {
        fs_bool = dif.getItemBool(fullScreen);
    }

    if (dif.isItemExist(windowed)) {
        win_bool = dif.getItemBool(windowed);
    }

    if (dif.isItemExist(border)) {
        bor_bool = dif.getItemBool(border);
    }

    if (dif.isItemExist(renderer)) {
        renderer_string = dif.getItem(renderer);
    }

    DisplayMode dm = getDisplayMode(fs_bool, win_bool, bor_bool);

    _gameInfo.debug.setting.pid          = pid;
    _gameInfo.debug.setting.gamePath     = gamePath;
    _gameInfo.debug.setting.platform     = "";
    _gameInfo.debug.setting.version      = version;
    _gameInfo.debug.setting.isReplay     = isReplay;
    _gameInfo.debug.setting.mapName      = mapName;
    _gameInfo.debug.setting.screenWidth  = sw;
    _gameInfo.debug.setting.screenHeight = sh;
    _gameInfo.debug.setting.fullScreen   = fs_bool;
    _gameInfo.debug.setting.windowed     = win_bool;
    _gameInfo.debug.setting.border       = bor_bool;
    _gameInfo.debug.setting.renderer     = renderer_string;

    switch (dm) {
        case DisplayMode::Fullscreen:
            _gameInfo.debug.setting.display = "Fullscreen Exclusive";
            break;
        case DisplayMode::Windowed:
            _gameInfo.debug.setting.display = "Windowed";
            break;
        case DisplayMode::BorderlessWindowed:
            _gameInfo.debug.setting.display = "Borderless Windowed";
            break;
        case DisplayMode::StretchedFullscreen:
            _gameInfo.debug.setting.display = "Stretched Fullscreen";
            break;
        default:
            _gameInfo.debug.setting.display = "Unknown Displaymode";
    }
}

inline DisplayMode Game::getDisplayMode(bool fullscreen, bool windowed, bool border) {
    if (!windowed) {
        return DisplayMode::Fullscreen;
    } else if (!fullscreen) {
        if (border) {
            return DisplayMode::Windowed;
        } else {
            return DisplayMode::BorderlessWindowed;
        }
    } else if (fullscreen) {
        return DisplayMode::StretchedFullscreen;
    } else {
        return DisplayMode::Unknown;
    }
}

/**
 * Initialize all the addresses.
 */
inline void Game::initAddrs() {
    if (nullptr == r.getHandle()) {
        std::cerr << "No valid process handle, call Game::getHandle() first.\n";
    }

    uint32_t fixed              = r.getAddr(FIXEDOFFSET);
    uint32_t classBaseArray     = r.getAddr(CLASSBASEARRAYOFFSET);
    uint32_t playerBaseArrayPtr = fixed + PLAYERBASEARRAYPTROFFSET;

    bool isObserverFlag = true;
    bool isThisGameOver = false;

    for (int i = 0; i < MAXPLAYER; i++, playerBaseArrayPtr += 4) {
        uint32_t playerBase = r.getAddr(playerBaseArrayPtr);

        _players[i] = false;

        if (playerBase != INVALIDCLASS) {
            uint32_t realPlayerBase = r.getAddr(playerBase * 4 + classBaseArray);
            int cur_c               = r.getInt(realPlayerBase + CURRENTPLAYEROFFSET);

            if (cur_c == 0x1010000 || cur_c == 0x101) {
                isObserverFlag = false;
            }

            bool isDefeated = r.getBool(realPlayerBase + ISDEFEATEDOFFSET);
            bool isGameOver = r.getBool(realPlayerBase + ISGAMEOVEROFFSET);
            bool isWinner   = r.getBool(realPlayerBase + ISWINNEROFFSET);

            _playerTeamNumber[i]   = r.getInt(realPlayerBase + TEAMNUMBEROFFSET);
            _playerDefeatFlag[i]   = isDefeated;
            _playerGameoverFlag[i] = isGameOver;
            _playerWinnerFlag[i]   = isWinner;

            if (isGameOver || isWinner) {
                isThisGameOver = true;
            }

            _players[i]     = true;
            _playerBases[i] = realPlayerBase;

            _buildings[i] = r.getAddr(realPlayerBase + BUILDINGOFFSET);
            _tanks[i]     = r.getAddr(realPlayerBase + TANKOFFSET);
            _infantrys[i] = r.getAddr(realPlayerBase + INFANTRYOFFSET);
            _aircrafts[i] = r.getAddr(realPlayerBase + AIRCRAFTOFFSET);

            _buildings_valid[i] = r.getAddr(realPlayerBase + BUILDINGOFFSET + 4);
            _tanks_valid[i]     = r.getAddr(realPlayerBase + TANKOFFSET + 4);
            _infantrys_valid[i] = r.getAddr(realPlayerBase + INFANTRYOFFSET + 4);
            _aircrafts_valid[i] = r.getAddr(realPlayerBase + AIRCRAFTOFFSET + 4);

            _houseTypes[i] = r.getAddr(realPlayerBase + HOUSETYPEOFFSET);
        }
    }

    _gameInfo.isObserver = isObserverFlag || isReplay;
    _gameInfo.isGameOver = isThisGameOver;
}

inline void Game::loadNumericsFromJson(std::string filePath) {
    json data = readJsonFromFile(filePath);

    _numerics.items.clear();

    for (auto& it : data) {
        std::string offset = it["Offset"];
        uint32_t s_offset  = std::stoul(offset, nullptr, 16);
        Numeric n(it["Name"], s_offset);
        _numerics.items.push_back(n);
    }
}

inline void Game::loadUnitsFromJson(std::string filePath) {
    json data = readJsonFromFile(filePath);

    _units.items.clear();

    for (auto& ut : data.items()) {
        UnitType s_ut;

        if (ut.key() == "Building") {
            s_ut = UnitType::Building;
        } else if (ut.key() == "Tank") {
            s_ut = UnitType::Tank;
        } else if (ut.key() == "Infantry") {
            s_ut = UnitType::Infantry;
        } else if (ut.key() == "Aircraft") {
            s_ut = UnitType::Aircraft;
        } else {
            s_ut = UnitType::Unknown;
        }

        for (auto& u : data[ut.key()]) {
            if (u.empty()) {
                continue;
            }

            bool s_show = true;
            if (u.contains("Show") && u["Show"] == 0) {
                s_show = false;
            }

            std::string offset = u["Offset"];
            uint32_t s_offset  = std::stoul(offset, nullptr, 16);

            int s_index = 99;
            if (u.contains("Index")) {
                s_index = u["Index"];
            }

            Unit ub(u["Name"], s_offset, s_ut, s_index, s_show);

            std::string s_invalid;
            if (u.contains("Invalid")) {
                ub.setInvalid(u["Invalid"]);
            }

            _units.items.push_back(ub);
        }
    }
}

inline void Game::initStrTypes() {
    _strName    = StrName();
    _strCountry = StrCountry();
}

inline void Game::initArrays() {
    _players     = std::array<bool, MAXPLAYER>{};
    _playerBases = std::array<uint32_t, MAXPLAYER>{};

    _buildings = std::array<uint32_t, MAXPLAYER>{};
    _infantrys = std::array<uint32_t, MAXPLAYER>{};
    _tanks     = std::array<uint32_t, MAXPLAYER>{};
    _aircrafts = std::array<uint32_t, MAXPLAYER>{};

    _buildings_valid = std::array<uint32_t, MAXPLAYER>{};
    _infantrys_valid = std::array<uint32_t, MAXPLAYER>{};
    _tanks_valid     = std::array<uint32_t, MAXPLAYER>{};
    _aircrafts_valid = std::array<uint32_t, MAXPLAYER>{};

    _houseTypes    = std::array<uint32_t, MAXPLAYER>{};
    _buildingInfos = std::array<tagBuildingInfo, MAXPLAYER>{};
    _superTimers   = std::array<tagSuperTimer, MAXPLAYER>{};
    _statusInfos   = std::array<tagStatusInfo, MAXPLAYER>{};
    _scoreInfos    = std::array<tagScoreInfo, MAXPLAYER>{};
    _colors        = std::array<std::string, MAXPLAYER>{};
    _colors.fill("0x000000");

    _playerTeamNumber   = std::array<uint32_t, MAXPLAYER>{};
    _playerDefeatFlag   = std::array<bool, MAXPLAYER>{};
    _playerGameoverFlag = std::array<bool, MAXPLAYER>{};
    _playerWinnerFlag   = std::array<bool, MAXPLAYER>{};
}

inline void Game::initGameInfo() { _gameInfo = tagGameInfo{}; }

/**
 * Return valid player number.
 */
inline int Game::hasPlayer() {
    int count = 0;

    for (bool it : _players) {
        if (it) {
            count++;
        }
    }
    if (count == 0) {
        std::cerr << "No valid player." << std::endl;
    }

    return count;
}

/**
 * Fetch the data in all the base class.
 */
inline void Game::refreshInfo() {
    if (!hasPlayer()) {
        std::cerr << "No valid player to show info.\n";
        _gameInfo.valid = false;
        return;
    }

    try {
        // 对每一步操作添加错误处理，防止一个操作失败导致整个刷新失败
        
        // 刷新数值信息
        for (auto& it : _numerics.items) {
            try {
                it.fetchData(r, _playerBases);
            } catch (...) {
                std::cerr << "Error fetching numeric data for " << it.getName() << std::endl;
            }
        }

        // 刷新单位信息
        for (auto& it : _units.items) {
            try {
                if (it.getUnitType() == UnitType::Building) {
                    it.fetchData(r, _buildings, _buildings_valid);
                } else if (it.getUnitType() == UnitType::Infantry) {
                    it.fetchData(r, _infantrys, _infantrys_valid);
                } else if (it.getUnitType() == UnitType::Tank) {
                    it.fetchData(r, _tanks, _tanks_valid);
                } else {
                    it.fetchData(r, _aircrafts, _aircrafts_valid);
                }
            } catch (...) {
                std::cerr << "Error fetching unit data for " << it.getName() << std::endl;
            }
        }

        try {
            _strName.fetchData(r, _playerBases);
        } catch (...) {
            std::cerr << "Error fetching player names" << std::endl;
        }
        
        try {
            _strCountry.fetchData(r, _houseTypes);
        } catch (...) {
            std::cerr << "Error fetching country data" << std::endl;
        }

        // 对每个刷新函数单独进行错误处理
        try {
            refreshBuildingInfos();
        } catch (...) {
            std::cerr << "Error refreshing building infos" << std::endl;
        }
        
        try {
            refreshSuperTimer();
        } catch (...) {
            std::cerr << "Error refreshing super timer" << std::endl;
        }
        
        try {
            refreshColors();
        } catch (...) {
            std::cerr << "Error refreshing colors" << std::endl;
        }
        
        try {
            refreshStatusInfos();
        } catch (...) {
            std::cerr << "Error refreshing status infos" << std::endl;
        }
        
        try {
            refreshScoreInfos();
        } catch (...) {
            std::cerr << "Error refreshing score infos" << std::endl;
        }

        try {
            refreshGameInfos();
        } catch (...) {
            std::cerr << "Error refreshing game infos" << std::endl;
        }
    } catch (...) {
        std::cerr << "Unexpected error in refreshInfo" << std::endl;
        _gameInfo.valid = false;
    }
}

inline void Game::getBuildingInfo(tagBuildingInfo* bi, int addr, int offset_0, int offset_1,
                                  UnitType utype) {
    uint32_t base = r.getAddr(addr + offset_0);
    
    // 如果基地址无效，直接返回
    if (base == 1 || base == 0) {
        return;
    }

    int currentCD = r.getInt(base + P_TIMEOFFSET);
    bool status   = r.getBool(base + P_STATUSOFFSET);

    uint32_t current = r.getAddr(base + P_CURRENTOFFSET);
    // 检查地址有效性
    if (current == 1 || current == 0) {
        return;
    }
    
    uint32_t type = r.getAddr(current + offset_1);
    // 检查地址有效性
    if (type == 1 || type == 0) {
        return;
    }
    
    int offset = r.getInt(type + P_ARRAYINDEXOFFSET);

    // Count same production item.
    int queueLength = r.getInt(base + P_QUEUELENGTHOFFSET);
    
    // 安全检查：设置队列长度的合理上限，防止异常值导致死循环
    const int MAX_QUEUE_LENGTH = 100;
    if (queueLength < 0 || queueLength > MAX_QUEUE_LENGTH) {
        queueLength = 0;  // 如果长度异常，则不处理队列
    }
    
    uint32_t queueBase = r.getAddr(base + P_QUEUEPTROFFSET);
    // 检查队列基址的有效性
    if (queueBase == 1 || queueBase == 0) {
        queueLength = 0;  // 如果队列基址无效，则不处理队列
    }
    
    int count = 1;
    for (int i = 0; i < queueLength; i++) {
        uint32_t curAddr = r.getAddr(queueBase + i * 4);
        // 检查当前地址的有效性
        if (curAddr == 1 || curAddr == 0) {
            break;  // 如果地址无效，终止循环
        }
        
        int curOffset = r.getInt(curAddr + P_ARRAYINDEXOFFSET);
        if (curOffset != offset) {
            break;
        }
        count++;
    }

    Version v = version;
    auto it   = std::find_if(
        _units.items.begin(), _units.items.end(),
        [offset, utype, v](const Unit& u) { return u.checkOffset(offset * 4, utype, v); });

    std::string name;

    if (it != _units.items.end()) {
        name = it->getName();

        tagBuildingNode bn = tagBuildingNode(name);

        bn.progress = currentCD;
        bn.status   = status;
        bn.number   = count;

        bi->list.push_back(bn);
    }
}

inline void Game::refreshBuildingInfos() {
    for (int i = 0; i < MAXPLAYER; i++) {
        if (!_players[i]) {
            continue;
        }

        tagBuildingInfo bi;

        uint32_t addr = _playerBases[i];

        getBuildingInfo(&bi, addr, P_AIRCRAFTOFFSET, P_UNITTYPEOFFSET, UnitType::Aircraft);
        getBuildingInfo(&bi, addr, P_BUILDINGFIRSTOFFSET, P_BUILDINGTYPEOFFSET, UnitType::Building);
        getBuildingInfo(&bi, addr, P_BUILDINGSECONDOFFSET, P_BUILDINGTYPEOFFSET,
                        UnitType::Building);
        getBuildingInfo(&bi, addr, P_INFANTRYOFFSET, P_INFANTRYTYPEOFFSET, UnitType::Infantry);
        getBuildingInfo(&bi, addr, P_TANKOFFSET, P_UNITTYPEOFFSET, UnitType::Tank);
        getBuildingInfo(&bi, addr, P_SHIPOFFSET, P_UNITTYPEOFFSET, UnitType::Tank);

        _buildingInfos[i] = bi;
    }
}

inline void Game::refreshSuperTimer() {
    int superNums = r.getInt(SUPERTIMEROFFSET + SUPERTIMERNUMSOFFSET);

    uint32_t vectorAddr = r.getAddr(SUPERTIMEROFFSET + SUPERTIMEVECTOROFFSET);
    std::array<tagSuperTimer, MAXPLAYER> sts;

    for (int i = 0; i < superNums; i++) {
        uint32_t curAddr = r.getAddr(vectorAddr + i * 4);
        int start        = r.getInt(curAddr + SUPERTIMESTARTOFFSET);
        int left         = r.getInt(curAddr + SUPERTIMELEFTOFFSET);
        uint32_t owner   = r.getAddr(curAddr + SUPERTIMEOWNEROFFSET);

        uint32_t typeAddr = r.getAddr(curAddr + SUPERTIMETYPEOFFSET);
        int duration      = r.getInt(typeAddr + SUPERTIMEDURATIONOFFSET);
        std::string name  = r.getString(typeAddr + SUPERTIMENAMEOFFSET);

        for (int j = 0; j < MAXPLAYER; j++) {
            if (owner == _playerBases[j]) {
                tagSuperNode sn(name, duration);
                if (start == -1) {
                    sn.status = 1;
                    sts[j].list.push_back(sn);
                    continue;
                }
                int currentFrame = r.getInt(GAMEFRAMEOFFSET);
                sn.left          = left - (currentFrame - start);
                sts[j].list.push_back(sn);
            }
        }
    }

    _superTimers = sts;
}

inline void Game::refreshColors() {
    for (int i = 0; i < MAXPLAYER; i++) {
        if (!_players[i]) {
            continue;
        }

        uint32_t addr  = _playerBases[i];
        uint32_t color = r.getColor(addr + COLOROFFSET);

        color = (color & 0x00FF00) | (color << 16 & 0xFF0000) | (color >> 16 & 0x0000FF);

        std::stringstream ss;
        ss << std::hex << color;
        _colors[i] = ss.str();
    }
}

inline void Game::refreshStatusInfos() {
    for (int i = 0; i < MAXPLAYER; i++) {
        if (!_players[i]) {
            continue;
        }

        tagStatusInfo si;

        uint32_t addr = _playerBases[i];

        int teamNumber       = r.getInt(addr + TEAMNUMBEROFFSET);
        int infantrySelfHeal = r.getInt(addr + INFANTRYSELFHEALOFFSET);
        int unitSelfHeal     = r.getInt(addr + UNITSELFHEALOFFSET);

        si.teamNumber       = teamNumber;
        si.infantrySelfHeal = infantrySelfHeal;
        si.unitSelfHeal     = unitSelfHeal;

        _statusInfos[i] = si;
    }
}

inline void Game::refreshScoreInfos() {
    for (int i = 0; i < MAXPLAYER; i++) {
        if (!_players[i]) {
            continue;
        }

        tagScoreInfo si;
        uint32_t addr = _playerBases[i];

        int totalKills               = 0;
        uint32_t killedUnitsBase     = addr + KILLEDUNITSOFHOUSES;
        uint32_t killedBuildingsBase = addr + KILLEDBUILDINGSOFHOUSES;

        for (int j = 0; j < 20; j++) {
            totalKills += r.getInt(killedUnitsBase + j * 4);
            totalKills += r.getInt(killedBuildingsBase + j * 4);
        }
        si.kills = totalKills;

        int totalKilledUnits     = r.getInt(addr + TOTALKILLEDUNITS);
        int totalKilledBuildings = r.getInt(addr + TOTALKILLEDBUILDINGS);
        si.lost                  = totalKilledUnits + totalKilledBuildings;

        int factoryProducedBuildingTypesCount =
            r.getInt(addr + FACTORYPRODUCEDBUILDINGTYPES + FACTORYTYPESOFFSET);
        int factoryProducedUnitTypesCount =
            r.getInt(addr + FACTORYPRODUCEDUNITTYPES + FACTORYTYPESOFFSET);
        int factoryProducedInfantryTypesCount =
            r.getInt(addr + FACTORYPRODUCEDINFANTRYTYPES + FACTORYTYPESOFFSET);
        int factoryProducedAircraftTypesCount =
            r.getInt(addr + FACTORYPRODUCEDAIRCRAFTTYPES + FACTORYTYPESOFFSET);

        int alliveUnitTypes     = r.getInt(addr + ALLIVEUNITTYPES + FACTORYTYPESOFFSET);
        int alliveInfantryTypes = r.getInt(addr + ALLIVEINFANTRYTYPES + FACTORYTYPESOFFSET);
        int alliveAircraftTypes = r.getInt(addr + ALLIVEAIRCRAFTTYPES + FACTORYTYPESOFFSET);

        int totalAlive = alliveUnitTypes + alliveInfantryTypes + alliveAircraftTypes;

        int totalBuilt = factoryProducedBuildingTypesCount + factoryProducedUnitTypesCount +
                         factoryProducedInfantryTypesCount + factoryProducedAircraftTypesCount +
                         totalKilledUnits + totalKilledBuildings;

        si.built = totalBuilt;
        si.alive = totalAlive;

        _scoreInfos[i] = si;
    }
}

inline void Game::refreshGameInfos() {
    if (version == Version::Yr) {
        _gameInfo.gameVersion = "Yr";
    } else {
        _gameInfo.gameVersion = "Ra2";
    }

    // 从内存中读取当前帧和实际游戏时间
    _gameInfo.currentFrame = r.getInt(GAMEFRAMEOFFSET);
    _gameInfo.realGameTime = r.getInt(GAMETIMEOFFSET);

    _gameInfo.mapName    = mapName;
    _gameInfo.mapNameUtf = mapNameUtf;

    _gameInfo.isGamePaused = r.getBool(GAMEPAUSEOFFSET);

    int playersNum         = 0;
    int defeatedPlayersNum = 0;

    for (int i = 0; i < MAXPLAYER; i++) {
        if (!_players[i] || _strCountry.getValueByIndex(i) == "") {
            continue;
        }
        playersNum++;

        if (_gameInfo.debug.playerDefeatFlag[i]) {
            defeatedPlayersNum++;
        }
    }

    _gameInfo.allPlayers  = playersNum;
    _gameInfo.leftPlayers = playersNum - defeatedPlayersNum;
}

inline void Game::structBuild() {
    for (int i = 0; i < MAXPLAYER; i++) {
        tagPlayer p;

        // Filter invalid players
        if (!_players[i] || _strCountry.getValueByIndex(i) == "") {
            p.valid = false;
            continue;
        }

        // Panel info
        tagPanelInfo pi;
        pi.playerName    = _strName.getValueByIndex(i);
        pi.playerNameUtf = _strName.getValueByIndexUtf(i);
        pi.balance       = _numerics.getItem("Balance").getValueByIndex(i);
        pi.creditSpent   = _numerics.getItem("Credit Spent").getValueByIndex(i);
        pi.powerDrain    = _numerics.getItem("Power Drain").getValueByIndex(i);
        pi.powerOutput   = _numerics.getItem("Power Output").getValueByIndex(i);
        pi.color         = _colors[i];
        pi.country       = _strCountry.getValueByIndex(i);

        // Units info
        tagUnitsInfo ui;
        for (auto& it : _units.items) {
            tagUnitSingle us;
            us.unitName = it.getName();
            us.index    = it.getUnitIndex();
            us.num      = it.getValueByIndex(i);
            us.show     = it.checkShow();
            ui.units.push_back(us);
        }

        std::sort(ui.units.begin(), ui.units.end(),
                  [](const tagUnitSingle& a, const tagUnitSingle& b) { return a.index < b.index; });

        // Players info
        p.valid      = true;
        p.panel      = pi;
        p.units      = ui;
        p.building   = _buildingInfos[i];
        p.superTimer = _superTimers[i];
        p.status     = _statusInfos[i];
        p.score      = _scoreInfos[i];

        // Game info
        _gameInfo.players[i]            = p;
        _gameInfo.debug.playerBase[i]   = _playerBases[i];
        _gameInfo.debug.buildingBase[i] = _buildings[i];
        _gameInfo.debug.infantryBase[i] = _infantrys[i];
        _gameInfo.debug.tankBase[i]     = _tanks[i];
        _gameInfo.debug.aircraftBase[i] = _aircrafts[i];
        _gameInfo.debug.houseType[i]    = _houseTypes[i];

        // Info flags
        _gameInfo.debug.playerTeamNumber[i]   = _playerTeamNumber[i];
        _gameInfo.debug.playerDefeatFlag[i]   = _playerDefeatFlag[i];
        _gameInfo.debug.playerGameoverFlag[i] = _playerGameoverFlag[i];
        _gameInfo.debug.playerWinnerFlag[i]   = _playerWinnerFlag[i];
    }
}

inline void Game::restart(bool valid) {
    if (!valid) {
        return;
    }

    if (r.getHandle() != nullptr) {
        CloseHandle(r.getHandle());
    }

    std::cout << "Handle Closed.\n";

    initStrTypes();
    initArrays();
    initGameInfo();
}

inline void Game::startLoop() {
    std::thread d_thread(std::bind(&Game::detectTask, this, 100));

    std::thread f_thread(std::bind(&Game::fetchTask, this, 10));

    d_thread.detach();
    f_thread.detach();
}

inline void Game::detectTask(int interval) {
    while (true) {
        getHandle();
        if (r.getHandle() != nullptr) {
            _gameInfo.valid = true;
            initAddrs();
        } else {
            restart(_gameInfo.valid);
            _gameInfo.valid = false;
        }

        Sleep(interval);
    }
}

inline void Game::fetchTask(int interval) {
    while (true) {
        if (_gameInfo.valid) {
            refreshInfo();
            structBuild();
            initAddrs();
        }

        Sleep(interval);
    }
}

inline std::string Game::getMapNameFromFile(const std::string& mapFile) {
    // 根据mapfile值映射地图名称
    if (mapFile == "662fb982b276020c5d3d40d598ef61ea.ra2mp") {
        return "2025冰天王-星夜冰天";
    } else if (mapFile == "4690472432de83d8aeb0e1b63a2dfcec.ra2mp") {
        return "2025冰天王-星夜黄金冰天";
    } else if (mapFile == "db801b2e6541882c5fe2df5e24ab3f0a.ra2mp") {
        return "2025冰天王-黄金冰天";
    } else if (mapFile == "003a3f7e37563db34ff17f53d282948d.ra2mp") {
        return "2025冰天王-宝石冰天";
    } else {
        // 如果没有匹配的mapfile，直接返回空字符串
        return "";
    }
}

}  // end of namespace Ra2ob

#endif  // RA2OB_SRC_GAME_HPP_
