#include "SDLIncludes.h"
#include <GL/glew.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <thread>
#include <set>
#include <algorithm>
#include <cctype>
#include <system_error>
#include <sstream>
#include "GameLoader.h"
#include "../Pkgs/imgui/imgui.h"
#include "../Pkgs/imgui/imgui_impl_sdl2.h"
#include "../Pkgs/imgui/imgui_impl_opengl3.h"
#include "../Src/Util/NewConfig.h"
#include "Util/ConfigBuilders.h"
#include "../Src/OSD/SDL/SDLInputSystem.h"
#include "../Src/Inputs/Inputs.h"
#include "OSD/ConfigFile.h"
#include "Main.h"
#include "Gui.h"

#ifdef _WIN32
    #include "../Src/OSD/Windows/DirectInputSystem.h"
#endif // _WIN32

/*
Quick description on the GUI stuff
----------------------------------

- Using imgui because I didn't want to suck in 1000 other dependencies.
- GUI will only show up if supermodel is run without command line parameters, so existing loaders etc should be completely
  uneffected by this code.
- All controls are completely dynamic, they are created from the settings themselves. So if new settings are added/removed
  they will automatically show up in the GUI, no modifications are required to this code.
- To make this work I added some constraints to the options. For example now you can add a min/max for settings, or add a
  list of possible values. Previously without constraints the code might just silently fail or fall over if you passed
  invalid values.
- The constraints are variants. A c++ variant is exactly the same as a C union, just type safe. So for our options we can
  store bool, unsigned, int, float and std::string. More types could be added but this is enough for now.

  TODO
  ----

 - Some options are just enums which are raw numbers and not user friendly. Probably best replaced with strings.
 - Resolution stuff could be handled better, ie push a list of possible resolutions. Might need to combine into
   a single string or something.
 - Per game options. Need some code to get a diff of possible configs I think.
*/


// ---------------------------------------------------------------------------------------------------------------
// ROM folder scan: only games whose "<name>.zip" exists in the ROM folder are shown as owned
// ---------------------------------------------------------------------------------------------------------------

static const bool  s_hideUnownedGames = false;   // true: unowned games are removed from the list
                                                // false: unowned games are blacked out so the list layout is kept

static std::string ToLowerStr(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

// Returns the lower case file names (without extension) of all *.zip files in the given directory
static std::set<std::string> ScanRomZips(const std::string& dir)
{
    std::set<std::string> found;
    std::error_code ec;

    for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code ec2;
        if (!it->is_regular_file(ec2)) {
            continue;
        }
        if (ToLowerStr(it->path().extension().string()) != ".zip") {
            continue;
        }
        found.insert(ToLowerStr(it->path().stem().string()));
    }

    return found;
}

static void WriteGameNode(Util::Config::Node& baseNode, const Util::Config::Node& diffNode, Util::Config::Node& writeNode, const std::string& group)
{
    for (const auto& n : baseNode) {

        if (n.IsLeaf() && n.Exists()) {

            auto& key = n.Key();
            auto  val = n.GetValue();

            if (val) {

                auto vRange = val->GetValueRange();

                if (vRange->GetGroup() != group) {
                    continue;
                }

                auto s1 = baseNode[key].ValueAs<std::string>();
                auto s2 = diffNode[key].ValueAs<std::string>();

                if (s1 != s2) {
                    writeNode[key] = s2;
                }
            }
        }
    }
}

static std::string NodeToString(Util::Config::Node& config)
{
    std::string s;

    for (const auto& n : config) {

        if (n.IsLeaf() && n.Exists()) {

            auto& key = n.Key();

            auto val = config[key].ValueAs<std::string>();

            s += key;
            s += " ";
            s += val;
            s += "\n";
        }
    }

    return s;
}

static void UpdateTempValues(Util::Config::Node& config, const std::string& group, bool init)
{
    for (auto it = config.begin(); it != config.end(); ++it)
    {
        if (it->IsLeaf() && it->Exists()) {

            const auto& key = it->Key();
            auto val = it->GetValue();

            if (val) {

                auto vRange = val->GetValueRange();

                if (vRange) {

                    if (vRange->GetGroup() == group) {

                        auto index = vRange->GetIndex();

                        switch (index) {
                        case 0:             // bool
                        {
                            if (init) { vRange->tempValue = config[key].ValueAs<bool>(); }
                            else { config.Set(key, std::get<bool>(vRange->tempValue)); }
                            break;
                        }
                        case 1:             // unsigned
                        {
                            if (init) { vRange->tempValue = config[key].ValueAs<unsigned>(); }
                            else { config.Set(key, std::get<unsigned>(vRange->tempValue)); }
                            break;
                        }
                        case 2:             // int
                        {
                            if (init) { vRange->tempValue = config[key].ValueAs<int>(); }
                            else { config.Set(key, std::get<int>(vRange->tempValue)); }
                            break;
                        }
                        case 3:             // float
                        {
                            if (init) { vRange->tempValue = config[key].ValueAs<float>(); }
                            else { config.Set(key, std::get<float>(vRange->tempValue)); }
                            break;
                        }
                        case 4:             // std::string
                        {
                            if (init) { vRange->tempValue = config[key].ValueAs<std::string>(); }
                            else { config.Set(key, std::get<std::string>(vRange->tempValue)); }
                            break;
                        }
                        }

                        it->GetValue()->SetValueRange(vRange);      // setting a new key could erase value range so make sure to re-add it
                    }
                }
            }  
        }
    }
}

// Display scale factor (1 = 96 DPI) for sizes given in pixels
static float s_uiScale = 1.0f;

static ImVec2 ScaledSize(float x, float y)
{
    return ImVec2(x * s_uiScale, y * s_uiScale);
}

template <typename T>
static std::string ValueToString(const T& value)
{
    std::ostringstream ss;
    ss << value;
    return ss.str();
}

template <typename T>
static bool ValuesMatch(const T& a, const T& b)
{
    return a == b;
}

// Floats are written to the INI file with 6 significant digits, so compare with a tolerance
static bool ValuesMatch(float a, float b)
{
    return std::fabs(a - b) <= 1e-5f * std::fabs(b);
}

// Combo box for a setting restricted to a list of values, showing the entries' labels if it has
// them. A current value that is not in the list (e.g. set by hand in the INI file or on the
// command line) is shown as an extra "Custom" entry and kept unless another entry is picked.
template <typename T>
static void ValueCombo(const std::string& key, Util::ValueRange& vRange, const T& current)
{
    auto& list = vRange.GetList();
    auto& labels = vRange.GetLabels();
    bool useLabels = labels.size() == list.size();

    std::vector<std::string> items;
    int selectedIndex = -1;

    for (size_t i = 0; i < list.size(); i++) {
        const auto& value = std::get<T>(list[i]);
        items.emplace_back(useLabels ? labels[i] : ValueToString(value));
        if (ValuesMatch(value, current)) {
            selectedIndex = (int)i;
        }
    }

    if (selectedIndex < 0) {
        items.emplace_back("Custom (" + ValueToString(current) + ')');
        selectedIndex = (int)list.size();
    }

    std::vector<const char*> itemPtrs;
    for (auto& s : items) {
        itemPtrs.emplace_back(s.c_str());
    }

    if (ImGui::Combo(key.c_str(), &selectedIndex, itemPtrs.data(), (int)itemPtrs.size()) && selectedIndex < (int)list.size()) {
        vRange.tempValue = list[selectedIndex];
    }
}

// Lets ImGui::InputText edit a std::string directly
static int InputTextResizeCallback(ImGuiInputTextCallbackData* data)
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto str = static_cast<std::string*>(data->UserData);
        str->resize(data->BufTextLen);
        data->Buf = str->data();
    }
    return 0;
}

static void CreateControls(Util::Config::Node& config, const std::string& group)
{
    for (auto it = config.begin(); it != config.end(); ++it)
    {
        if (it->IsLeaf() && it->Exists()) {

            const auto& key = it->Key();
            auto val = it->GetValue();

            if (val) {

                auto vRange = val->GetValueRange();

                if (vRange) {

                    if (vRange->GetGroup() == group) {

                        auto index = vRange->GetIndex();
                        auto& list = vRange->GetList();

                        auto ProcessScalar = [&](auto valuePtr, ImGuiDataType type)
                        {
                            using T = std::decay_t<decltype(*valuePtr)>;
                            auto min_ = std::get<T>(vRange->GetMin());
                            auto max_ = std::get<T>(vRange->GetMax());

                            // optional display text: a unit, or a name for the minimum value
                            const char* format = nullptr;
                            if (!vRange->GetSliderMinLabel().empty() && *valuePtr == min_) {
                                format = vRange->GetSliderMinLabel().c_str();
                            }
                            else if (!vRange->GetSliderFormat().empty()) {
                                format = vRange->GetSliderFormat().c_str();
                            }

                            ImGui::SliderScalar(key.c_str(), type, valuePtr, &min_, &max_, format);
                        };
                        
                        auto ProcessControls = [&](auto valuePtr, ImGuiDataType type)
                        {
                            if (vRange->HasMinMax()) {
                                ProcessScalar(valuePtr, type);
                            }
                            else if (list.size()) {
                                ValueCombo(key, *vRange, *valuePtr);
                            }
                            else {
                                ImGui::InputScalar(key.c_str(), type, valuePtr);
                            }
                        };


                        switch (index) {
                        case 0:             // bool
                        {
                            auto p = std::get_if<bool>(&vRange->tempValue);
                            ImGui::Checkbox(key.c_str(), p);
                            break;
                        }
                        case 1:             // unsigned
                        {
                            auto p = std::get_if<unsigned>(&vRange->tempValue);
                            ProcessControls(p, ImGuiDataType_U32);
                            break;
                        }
                        case 2:             // int
                        {
                            auto p = std::get_if<int>(&vRange->tempValue);
                            ProcessControls(p, ImGuiDataType_S32);
                            break;
                        }
                        case 3:             // float
                        {
                            auto p = std::get_if<float>(&vRange->tempValue);
                            ProcessControls(p, ImGuiDataType_Float);
                            break;
                        }
                        case 4:             // std::string
                        {
                            auto& option = std::get<std::string>(vRange->tempValue);

                            if (list.size()) {
                                ValueCombo(key, *vRange, option);
                            }
                            else {
                                // edit the string directly, it grows as needed
                                ImGui::InputText(key.c_str(), option.data(), option.capacity() + 1, ImGuiInputTextFlags_CallbackResize, InputTextResizeCallback, &option);
                            }

                            break;
                        }
                        }
                    }
                }
            }
        }
    }
}

static void SetDefaultKeyVal(std::shared_ptr<CInput> input)
{
    std::string key = std::string("Input") + input->id;

    auto defaultConfig = DefaultConfig();

    auto mapping = defaultConfig[key].ValueAs<std::string>();

    // update input with value from our default config
    input->SetMapping(mapping.c_str());
}

static std::vector<std::string> SplitByComma(const std::string& input) 
{
    std::vector<std::string> result;
    size_t start = 0;
    size_t end;

    while ((end = input.find(',', start)) != std::string::npos) {
        result.emplace_back(input.substr(start, end - start));
        start = end + 1;
    }

    // Add the last segment (or the whole string if no commas)
    result.emplace_back(input.substr(start));
    return result;
}

struct KeyBindState
{
    std::shared_ptr<CInput> input;
    bool waitingForInput = false;
    int processKeyCount = 0;            // we need to let the gui draw a frame or two before blocking for key input

    void Reset()
    {
        input = nullptr;
        waitingForInput = false;
        processKeyCount = 0;
    }
};

static void BindKeys(Util::Config::Node& config, KeyBindState& kb, bool openPopup)
{
    bool finish = false;
    bool appendPressed = false;

    if (openPopup) {
        ImGui::OpenPopup("Key Binding");
    }

    if (ImGui::BeginPopupModal("Key Binding", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {

        if (ImGui::BeginTable("ShortcutTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {

            ImGui::TableSetupColumn("Action");
            ImGui::TableSetupColumn("Keys");
            ImGui::TableHeadersRow();

            auto keyList = SplitByComma(kb.input->GetMapping());

            for (auto& k : keyList) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", kb.input->label);
                ImGui::TableNextColumn();
                ImGui::Text("%s", k.c_str());
            }

            ImGui::EndTable();
        }

        ImGui::Spacing(); // Adds default vertical spacing

        if (kb.waitingForInput) {

            ImVec4 color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // bright yellow
            color.w = 0.7f + 0.3f * 0.1f; // modulate alpha

            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::Text("Waiting for key input. Press esc to cancel.");
            ImGui::PopStyleColor();

            kb.processKeyCount++;
        }
        else {
            ImGui::NewLine();
        }
        ImGui::Spacing(); // Adds default vertical spacing


        if (kb.waitingForInput) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Set", ScaledSize(120, 0))) {
            kb.input->ClearMapping();
            appendPressed = true;
        }

        if (ImGui::Button("Append", ScaledSize(120, 0))) {
            appendPressed = true;
        }

        if (ImGui::Button("Clear", ScaledSize(120, 0))) {
            kb.input->ClearMapping();
            kb.input->StoreToConfig(&config);
        }

        if (ImGui::Button("Default", ScaledSize(120, 0))) {
            SetDefaultKeyVal(kb.input);
            kb.input->StoreToConfig(&config);
        }

        if (ImGui::Button("Finish", ScaledSize(120, 0))) {
            ImGui::CloseCurrentPopup();
            finish = true;
        }

        if (kb.waitingForInput) {
            ImGui::EndDisabled();
        }

        ImGui::EndPopup();

        if (kb.processKeyCount > 2) {           // kind of cludge logic. We need to draw at least once to update the GUI, configure is a blocking function so the GUI will basically freeze until we press a button. Time out might make sense

            auto system = kb.input->GetInputSystem();

            system->UngrabMouse();
            kb.input->Configure(true);
            system->GrabMouse();

            kb.input->StoreToConfig(&config);
            kb.processKeyCount = 0;
            kb.waitingForInput = false;
        }

        if (appendPressed) {
            kb.waitingForInput = true;
        }

        if (finish) {
            kb.Reset();
        }
    }
}

static void AddKeys(Util::Config::Node& config, KeyBindState& kb, const std::vector<std::shared_ptr<CInput>>& keyInputs)
{
    bool openPopup = false;

    for (auto& k : keyInputs) {
        auto mapping    = k->GetMapping();
        auto group      = k->GetInputGroup();
        auto label      = k->label;

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%s", group);
        ImGui::TableSetColumnIndex(1);
        if (ImGui::Selectable(label, false, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns)) {
            if (ImGui::IsMouseDoubleClicked(0)) {
                kb.input = k;
                openPopup = true;
            }
        }
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%s", mapping);
    }

    BindKeys(config, kb, openPopup);
}

static void DrawButtonOptions(Util::Config::Node& config, int& selectedGameIndex, bool& exit, bool& saveSettings, std::shared_ptr<CInputs>& inputs, bool closeRequested, bool& exitPromptOpen)
{
    if (ImGui::Button("Load game")) {

        if (selectedGameIndex < 0) {
            ImGui::OpenPopup("Load game");
        }
        else {
            exit = true;
        }
    }

    if (ImGui::BeginPopupModal("Load game", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {

        ImGui::Text("No game selected");
        ImGui::Separator();

        if (ImGui::Button("OK", ScaledSize(120, 0))) { 
            ImGui::CloseCurrentPopup(); 
        }

        ImGui::EndPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Load Defaults")) {
        ImGui::OpenPopup("Confirm Load");
    }

    if (ImGui::BeginPopupModal("Confirm Load", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {

        ImGui::Text("Are you sure you want to load defaults?");
        ImGui::Separator();

        if (ImGui::Button("Yes", ScaledSize(120, 0))) { 
            config = DefaultConfig();
            inputs = nullptr; // rebuilt from the defaults when Key bindings is next shown
            ImGui::CloseCurrentPopup(); 
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ScaledSize(120, 0))) { 
            ImGui::CloseCurrentPopup(); 
        }

        ImGui::EndPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Exit") || closeRequested) {
        ImGui::OpenPopup("Save On Exit");
    }

    exitPromptOpen = ImGui::BeginPopupModal("Save On Exit", NULL, ImGuiWindowFlags_AlwaysAutoResize);
    if (exitPromptOpen) {

        ImGui::Text("Save settings upon exit?");
        ImGui::Separator();

        if (ImGui::Button("Yes", ScaledSize(120, 0))) {
            selectedGameIndex = -1;
            saveSettings = true;
            exit = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("No", ScaledSize(120, 0))) {
            selectedGameIndex = -1;
            saveSettings = false;
            exit = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    {
        const bool disabled = true;

        if (disabled) {
            ImGui::BeginDisabled(); // Disables interaction
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f); // Fade visuals
        }

        static bool perGameSettings = false;
        bool perGamesSettingsCopy = perGameSettings;
        ImGui::Checkbox("Per game settings", &perGamesSettingsCopy);



        if (disabled) {
            ImGui::PopStyleVar();
            ImGui::EndDisabled();
        }
    }
}

static std::shared_ptr<CInputs> GetInputSystem(Util::Config::Node& config, SDL_Window* window)
{
    std::string selectedInputSystem = config["InputSystem"].ValueAs<std::string>();

    std::shared_ptr<CInputSystem> inputSystem;

    if (selectedInputSystem == "sdl")               { inputSystem = std::shared_ptr<CInputSystem>(new CSDLInputSystem(config, false));}
    else if (selectedInputSystem == "sdlgamepad")   { inputSystem = std::shared_ptr<CInputSystem>(new CSDLInputSystem(config, true)); }

#ifdef SUPERMODEL_WIN32
    else if (selectedInputSystem == "dinput")       { inputSystem = std::shared_ptr<CInputSystem>(new CDirectInputSystem(config, window, false, false));}
    else if (selectedInputSystem == "xinput")       { inputSystem = std::shared_ptr<CInputSystem>(new CDirectInputSystem(config, window, false, true)); }
    else if (selectedInputSystem == "rawinput")     { inputSystem = std::shared_ptr<CInputSystem>(new CDirectInputSystem(config, window, true, false)); }
#endif // SUPERMODEL_WIN32

    // initialise 
    if (inputSystem) {
        auto inputs = std::shared_ptr<CInputs>(new CInputs(inputSystem));
        inputs->Initialize();
        inputs->LoadFromConfig(config); 

        int x, y, w, h;
        SDL_GetWindowPosition(window, &x, &y);
        SDL_GL_GetDrawableSize(window, &w, &h);

        inputSystem->SetDisplayGeom(x, y, w, h);
        return inputs;
    }

    return nullptr;
}

static void DrawInputSystemError(const Util::Config::Node& config)
{
    ImGui::TextWrapped("Input system \"%s\" is not available on this platform. Choose another one under Core > InputSystem.",
        config["InputSystem"].ValueAs<std::string>().c_str());
}

// State of the joystick calibration dialog, kept across frames
struct CalibrationState
{
    static constexpr Uint32 SampleTimeMs = 3000; // how long each step samples the axis (same as console calibration)

    std::shared_ptr<CInputs> inputs; // own input system, alive while the dialog is open
    int joyNum = 0;
    int axisNum = -1;
    int step = -1;                   // -1 = choosing joystick and axis, 0-2 = calibration steps, 3 = result
    bool sampling = false;
    Uint32 sampleStart = 0;
    int minVal = 0;                  // range seen while sampling the current step
    int maxVal = 0;
    int posVal = 0;
    int negVal = 0;
    int offVal = 0;
    unsigned deadZone = 0;
    bool valid = false;

    void Reset() { *this = CalibrationState(); }

    // back to choosing an axis, keeping the input system and joystick
    void Restart()
    {
        axisNum = -1;
        step = -1;
        sampling = false;
    }
};

static void AxisValueBar(int value)
{
    float fraction = (value - DEFAULT_JOY_AXISMINVAL) / float(DEFAULT_JOY_AXISMAXVAL - DEFAULT_JOY_AXISMINVAL);
    char text[16];
    std::snprintf(text, sizeof(text), "%d", value);
    ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f), text);
}

// Stores the input system settings to config. Storing replaces values of a different type (numbers read from the INI file are text),
// which drops the value ranges the controls are built from, so they are put back afterwards.
static void StoreInputsToConfig(CInputs& inputs, Util::Config::Node& config)
{
    std::map<std::string, std::shared_ptr<Util::ValueRange>> ranges;
    for (auto& n : config) {
        if (n.IsLeaf() && n.Exists() && n.GetValue()->GetValueRange()) {
            ranges[n.Key()] = n.GetValue()->GetValueRange();
        }
    }

    inputs.StoreToConfig(&config);

    for (auto& r : ranges) {
        auto val = config[r.first].GetValue();
        if (val && !val->GetValueRange()) {
            val->SetValueRange(r.second);
        }
    }
}

// Same procedure as the console calibration (CInputs::CalibrateJoysticks), but without blocking the GUI
static void DrawCalibrationDialog(Util::Config::Node& config, CalibrationState& cal, bool openPopup)
{
    if (openPopup) {
        ImGui::OpenPopup("Joystick Calibration");
    }

    if (!ImGui::BeginPopupModal("Joystick Calibration", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f); // the dialog sizes to its content, so give wrapped text a width

    bool close = false;
    auto system = cal.inputs ? cal.inputs->GetInputSystem() : nullptr;
    int numJoys = 0;
    const JoyDetails* joyDetails = nullptr;

    if (system) {
        system->Poll();
        numJoys = system->GetNumJoysticks();
        if (numJoys == ANY_JOYSTICK) {
            numJoys = 0;    // can't tell joysticks apart, so can't calibrate them
        }
        if (cal.joyNum >= numJoys || !system->GetJoyDetails(cal.joyNum)) {
            cal.Restart();  // joystick went away
            cal.joyNum = 0;
        }
        joyDetails = numJoys > 0 ? system->GetJoyDetails(cal.joyNum) : nullptr;
    }

    if (!system) {
        DrawInputSystemError(config);
        close = ImGui::Button("Close", ScaledSize(120, 0));
    }
    else if (!joyDetails) {
        ImGui::Text("No joysticks attached to calibrate.");
        close = ImGui::Button("Close", ScaledSize(120, 0));
    }
    else if (cal.step < 0) {

        // choose joystick and axis
        std::vector<const char*> joyNames;
        for (int i = 0; i < numJoys; i++) {
            auto details = system->GetJoyDetails(i);
            joyNames.emplace_back(details ? details->name : "?");
        }
        if (ImGui::Combo("Joystick", &cal.joyNum, joyNames.data(), (int)joyNames.size())) {
            cal.axisNum = -1;
        }

        ImGui::Text("Choose the axis to calibrate. Move the control to see which axis it is.");

        if (ImGui::BeginTable("Axes", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Axis", ImGuiTableColumnFlags_WidthFixed, 150.0f * s_uiScale);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 300.0f * s_uiScale);
            for (int axisNum = 0; axisNum < NUM_JOY_AXES; axisNum++) {
                if (!joyDetails->hasAxis[axisNum]) {
                    continue;
                }
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ImGui::Selectable(joyDetails->axisName[axisNum], cal.axisNum == axisNum, ImGuiSelectableFlags_SpanAllColumns)) {
                    cal.axisNum = axisNum;
                }
                ImGui::TableSetColumnIndex(1);
                AxisValueBar(system->GetRawJoyAxisValue(cal.joyNum, axisNum));
            }
            ImGui::EndTable();
        }

        ImGui::BeginDisabled(cal.axisNum < 0);
        if (ImGui::Button("Calibrate", ScaledSize(120, 0))) {
            cal.step = 0;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        close = ImGui::Button("Close", ScaledSize(120, 0));
    }
    else if (cal.step < 3) {

        int value = system->GetRawJoyAxisValue(cal.joyNum, cal.axisNum);

        ImGui::Text("Calibrating %s of joystick '%s'", joyDetails->axisName[cal.axisNum], joyDetails->name);
        ImGui::Separator();
        ImGui::Text("Step %d of 3:", cal.step + 1);
        ImGui::TextUnformatted(CInputSystem::GetJoystickAxisCalibrationInstructions(cal.axisNum, cal.step).c_str());
        AxisValueBar(value);

        if (cal.sampling) {
            cal.minVal = std::min(cal.minVal, value);
            cal.maxVal = std::max(cal.maxVal, value);

            Uint32 elapsed = SDL_GetTicks() - cal.sampleStart;
            ImGui::ProgressBar(std::min(1.0f, elapsed / float(CalibrationState::SampleTimeMs)), ImVec2(-1.0f, 0.0f), "Calibrating, keep holding...");

            if (elapsed >= CalibrationState::SampleTimeMs) {
                cal.sampling = false;
                switch (cal.step) {
                case 0: cal.posVal = (std::abs(cal.maxVal) >= std::abs(cal.minVal) ? cal.maxVal : cal.minVal); break;
                case 1: cal.negVal = (std::abs(cal.minVal) >= std::abs(cal.maxVal) ? cal.minVal : cal.maxVal); break;
                case 2: cal.valid = CInputSystem::CalculateJoystickAxisCalibration(cal.posVal, cal.negVal, cal.minVal, cal.maxVal, cal.offVal, cal.deadZone); break;
                }
                cal.step++;
            }
        }
        else {
            ImGui::NewLine();
            if (ImGui::Button("Start", ScaledSize(120, 0))) {
                cal.sampling = true;
                cal.sampleStart = SDL_GetTicks();
                cal.minVal = value;
                cal.maxVal = value;
            }
            ImGui::SameLine();
        }

        if (ImGui::Button("Cancel", ScaledSize(120, 0))) {
            cal.Restart();
        }
    }
    else if (cal.valid) {

        ImGui::Text("Calibrated axis settings:");
        ImGui::Text(" Min value        = %d", cal.negVal);
        ImGui::Text(" Center/off value = %d", cal.offVal);
        ImGui::Text(" Max value        = %d", cal.posVal);
        ImGui::Text(" Dead zone        = %u %%", cal.deadZone);
        ImGui::Separator();

        if (ImGui::Button("Accept", ScaledSize(120, 0))) {
            system->SetJoystickAxisCalibration(cal.joyNum, cal.axisNum, cal.negVal, cal.offVal, cal.posVal, cal.deadZone);
            StoreInputsToConfig(*cal.inputs, config);
            cal.Restart();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard", ScaledSize(120, 0))) {
            cal.Restart();
        }
    }
    else {

        ImGui::TextWrapped("There was a problem calibrating the axis. This may be because the steps were not followed correctly or the joystick is sending invalid data.");
        if (ImGui::Button("Try again", ScaledSize(120, 0))) {
            cal.step = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ScaledSize(120, 0))) {
            cal.Restart();
        }
    }

    if (close) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::PopTextWrapPos();
    ImGui::EndPopup();

    if (close) {
        cal.Reset();    // releases the input system
    }
}

static Game GetGame(const std::map<std::string, Game>& games, int selectedGameIndex)
{
    if (selectedGameIndex >= 0) {

        int index = 0;
        for (const auto& g : games) {
            if (index == selectedGameIndex) {
                return g.second;
            }
            index++;
        }
    }

    return {};
}

static void GUI(const ImGuiIO& io, Util::Config::Node& config, const std::map<std::string, Game>& games, const std::set<std::string>& ownedZips, int& selectedGameIndex, bool& exit, bool& saveSettings, SDL_Window* window, std::shared_ptr<CInputs>& inputs, KeyBindState& kb, CalibrationState& cal, bool closeRequested, bool& exitPromptOpen)
{
    ImVec4 clear_color = ImVec4(0.0f, 0.5f, 192/255.f, 1.00f);

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

    ImGui::Begin("Custom Window", nullptr, ImGuiWindowFlags_NoTitleBar); // Explicitly set a window name

    ImGui::BeginChild("TableRegion", ScaledSize(0.0f, 200.0f), true, ImGuiWindowFlags_HorizontalScrollbar);

    if (ImGui::BeginTable("Games", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("Title");
        ImGui::TableSetupColumn("Rom Name");
        ImGui::TableSetupColumn("Version");
        ImGui::TableSetupColumn("Year");
        ImGui::TableSetupColumn("Stepping");

        ImGui::TableHeadersRow();

        int row = 0;
        for (const auto& g : games) {

            const bool owned = ownedZips.count(ToLowerStr(g.second.name)) > 0;

            // row must always advance, GetGame() / GetRomPath() index the full games map
            if (!owned && s_hideUnownedGames) {
                row++;
                continue;
            }

            ImGui::TableNextRow();

            if (!owned) {
                // black out the row and make it non-interactive so the layout is kept
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(0, 0, 0, 255));
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(0, 0, 0, 255));
                ImGui::BeginDisabled();
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));
            }

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", g.second.title.c_str());
            ImGui::TableSetColumnIndex(1);
            if (ImGui::Selectable(g.second.name.c_str(), selectedGameIndex == row, ImGuiSelectableFlags_SpanAllColumns)) {
                selectedGameIndex = row;
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                exit = true;
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", g.second.version.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%d", g.second.year);
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%s", g.second.stepping.c_str());

            if (!owned) {
                ImGui::PopStyleColor();
                ImGui::EndDisabled();
            }

            row++;
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();

    // draw button options
    DrawButtonOptions(config, selectedGameIndex, exit, saveSettings, inputs, closeRequested, exitPromptOpen);

    // create a space
    ImGui::Dummy(ScaledSize(0.0f, 20.0f));

    // draw the tabbed options
    if (ImGui::BeginTabBar("MyTabBar", ImGuiTabBarFlags_FittingPolicyResizeDown)) {
        if (ImGui::BeginTabItem("Core")) {
            UpdateTempValues(config, "Core", true);
            CreateControls(config, "Core");
            UpdateTempValues(config, "Core", false);
            ImGui::EndTabItem();
            inputs = nullptr;
        }
        if (ImGui::BeginTabItem("Video")) {
            UpdateTempValues(config, "Video", true);
            CreateControls(config, "Video");
            UpdateTempValues(config, "Video", false);
            ImGui::EndTabItem();
            inputs = nullptr;
        }
        if (ImGui::BeginTabItem("Audio")) {
            UpdateTempValues(config, "Sound", true);
            CreateControls(config, "Sound");
            UpdateTempValues(config, "Sound", false);
            ImGui::EndTabItem();
            inputs = nullptr;
        }
        if (ImGui::BeginTabItem("Networking")) {
            UpdateTempValues(config, "Network", true);
            CreateControls(config, "Network");
            UpdateTempValues(config, "Network", false);
            ImGui::EndTabItem();
            inputs = nullptr;
        }
        if (ImGui::BeginTabItem("Misc")) {
            UpdateTempValues(config, "Misc", true);
            CreateControls(config, "Misc");
            UpdateTempValues(config, "Misc", false);
            ImGui::EndTabItem();
            inputs = nullptr;
        }
        if (ImGui::BeginTabItem("ForceFeedback")) {
            UpdateTempValues(config, "ForceFeedback", true);
            CreateControls(config, "ForceFeedback");
            UpdateTempValues(config, "ForceFeedback", false);
            ImGui::EndTabItem();
            inputs = nullptr;
        }
        if (ImGui::BeginTabItem("Sensitivity")) {
            UpdateTempValues(config, "Sensitivity", true);
            CreateControls(config, "Sensitivity");
            UpdateTempValues(config, "Sensitivity", false);
            bool openCalibration = false;
            if (ImGui::Button("Joystick calibration")) {
                cal.Reset();
                cal.inputs = GetInputSystem(config, window);
                if (cal.inputs) {
                    cal.inputs->GetInputSystem()->SetPollConsumesEvents(false); // the GUI's own event loop needs the events
                }
                openCalibration = true;
            }
            DrawCalibrationDialog(config, cal, openCalibration);
            ImGui::EndTabItem();
            inputs = nullptr;
        }
        if (ImGui::BeginTabItem("Key bindings")) {

            if (inputs == nullptr) {
                inputs = GetInputSystem(config, window);
            }

            if (inputs == nullptr) {
                DrawInputSystemError(config);
            }
            else if (ImGui::BeginTable("KeyTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {

                auto inputList = inputs->GetGameInputs(GetGame(games,selectedGameIndex));

                ImGui::TableSetupColumn("Group");
                ImGui::TableSetupColumn("Action");
                ImGui::TableSetupColumn("Keys");
                ImGui::TableHeadersRow();

                AddKeys(config, kb, inputList);

                ImGui::EndTable();
            }
            
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End(); // Close the window


    // Rendering
    ImGui::Render();
    
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

static std::string GetRomPath(int selectedGame, const std::map<std::string, Game>& games, const std::string& romDirectory)
{
    if (selectedGame >= 0) {
        int index = 0;
        for (auto& g : games) {
            if (selectedGame == index) {
                return (std::filesystem::path(romDirectory) / (g.second.name + ".zip")).string();
            }
            index++;
        }
    }

    return {};  // no game
}

// Scale factor of the primary display, where the window opens (1 = 96 DPI). Windows only: a DPI aware build (per its manifest)
// works in physical pixels, an unaware one is told 96 DPI and gets scaled by Windows. Elsewhere SDL reports the
// monitor's physical DPI rather than the user's scaling preference.
static float GetDPIScale()
{
#ifdef _WIN32
    float ddpi = 96.0f;
    if (SDL_GetDisplayDPI(0, &ddpi, nullptr, nullptr) != 0) {
        SDL_Log("Failed to get DPI: %s", SDL_GetError());
        return 1.0f;
    }
    return std::max(1.0f, ddpi / 96.0f);
#else
    return 1.0f;
#endif
}

std::vector<std::string> RunGUI(const std::string& configPath, const Util::Config::Node& fileConfig)
{
    // Our own copy of the complete config file. The GUI only ever edits the global
    // section but the game-specific sections are carried along so that they survive
    // being written back out.
    Util::Config::Node fullConfig = fileConfig;

    // The global section, merged over the defaults, is what the GUI operates on. The
    // merge is also what attaches the value ranges the controls are built from, so it
    // has to be done here: copying a node does not preserve them.
    Util::Config::Node config("Global");
    Util::Config::MergeINISections(&config, DefaultConfig(), fullConfig);

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! Error: " << SDL_GetError() << std::endl;
        return {};
    }

    // Set OpenGL attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_INPUT_FOCUS);

    // Create SDL window
    s_uiScale = GetDPIScale();
    SDL_Window* window = SDL_CreateWindow("SuperSetup", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, int(950 * s_uiScale), int(600 * s_uiScale), window_flags);
    if (!window) {
        std::cerr << "Window could not be created! Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return {};
    }

    // Create OpenGL context
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "OpenGL context could not be created! Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return {};
    }

    SDL_GL_MakeCurrent(window, glContext);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    io.Fonts->AddFontDefaultVector();

    ImGui::GetIO().IniFilename = nullptr;                      // we don't need to save window positions between runs

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();
    //ImGui::StyleColorsClassic();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(s_uiScale);
    style.FontScaleDpi = s_uiScale;

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, glContext);
    ImGui_ImplOpenGL3_Init("#version 410");

    std::string xmlFile = config["GameXMLFile"].ValueAs<std::string>();
    GameLoader loader(xmlFile);
    auto& games = loader.GetGames();
    std::string scannedRomDirectory = config["ROMDirectory"].ValueAs<std::string>();
    std::set<std::string> ownedZips = ScanRomZips(scannedRomDirectory);
    int selectedGame = -1;  // -1 means no selection
    std::vector<std::string> romFiles;
    std::string path;

    // Main loop
    std::shared_ptr<CInputs> inputs;
    bool saveSettings = true;
    bool running = true;
    KeyBindState kb{};
    CalibrationState cal{};
    bool exitPromptOpen = false;
    bool exit = false;
    SDL_Event event{};

    while (running) {

        // Closing the window asks to save like Exit does; closing it again while asked exits without saving
        bool closeRequested = false;

        while (SDL_PollEvent(&event)) {

            ImGui_ImplSDL2_ProcessEvent(&event);

            if (event.type == SDL_QUIT) {
                if (exitPromptOpen) {
                    goto exitNoSave;
                }
                closeRequested = true;
            }
        }

        // Rescan only when the ROM directory setting changes.
        const std::string romDirectory = config["ROMDirectory"].ValueAs<std::string>();
        if (romDirectory != scannedRomDirectory) {
            scannedRomDirectory = romDirectory;
            ownedZips = ScanRomZips(scannedRomDirectory);
            selectedGame = -1;
        }

        GUI(io, config, games, ownedZips, selectedGame, exit, saveSettings, window, inputs, kb, cal, closeRequested, exitPromptOpen);

        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        SDL_GL_SwapWindow(window);

        if (exit) {
            break;
        }
    }

    path = GetRomPath(selectedGame, games, config["ROMDirectory"].ValueAs<std::string>());
    if (!path.empty()) {
        romFiles.emplace_back(path);
    }
    
    if (saveSettings) {
        // Fold the edited global settings back into the complete config so that the
        // game-specific sections are preserved by the write
        Util::Config::CopyINISection(&fullConfig, config);
        Util::Config::WriteINIFile(configPath, fullConfig, TimestampedConfigFileComment("Updated from GUI."));
    }

exitNoSave:

    // Release the input systems while SDL is still up
    kb.Reset();
    cal.Reset();
    inputs = nullptr;

    // Cleanup resources
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return romFiles;
}
