#include "devices/DeviceBus.h"
#include "devices/KeyboardDevice.h"
#include "devices/MouseDevice.h"
#include "devices/SpeechDevice.h"
#include "lang/Lexer.h"
#include "lang/Parser.h"
#include "lang/Vm.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static int g_failed = 0;

static void check(bool cond, const char* msg) {
    if (!cond) {
        std::cerr << "FAIL: " << msg << "\n";
        ++g_failed;
    } else {
        std::cout << "ok: " << msg << "\n";
    }
}

static Program parse(const std::string& src) {
    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    return parser.parse();
}

int main() {
    {
        auto tokens = Lexer("// hi\nKey.Enter = said(\"jump\")\n").tokenize();
        bool sawSaid = false;
        bool sawString = false;
        for (const auto& t : tokens) {
            if (t.kind == TokenKind::Identifier && t.text == "said") sawSaid = true;
            if (t.kind == TokenKind::String && t.text == "jump") sawString = true;
        }
        check(sawSaid && sawString, "lexer said() and string");
    }
    {
        auto tokens = Lexer("' comment\n% also\nREM basic\nEnter = true\n").tokenize();
        bool sawEnter = false;
        for (const auto& t : tokens)
            if (t.kind == TokenKind::Identifier && t.text == "enter") sawEnter = true;
        check(sawEnter, "lexer skips ', %, REM comments");
    }
    {
        Program p = parse("Key.Enter = said(\"jump\")\n");
        check(p.error.empty(), "parse assignment said()");
        check(p.statements.size() == 1, "one statement");
        auto* as = dynamic_cast<AssignStmt*>(p.statements[0].get());
        check(as != nullptr, "is assignment");
        auto* call = as ? dynamic_cast<CallExpr*>(as->value.get()) : nullptr;
        check(call && call->name == "said", "rhs is said()");
    }
    {
        Program p = parse("Enter = said(\"jump\")\n");
        check(p.error.empty(), "bare key assignment");
    }
    {
        Program p = parse("if said(\"open notepad\", 5) then\n  Press(Key.LeftWindows)\n  wait 100 ms\n  Type(\"notepad\")\nend if\n");
        check(p.error.empty(), "parse if/then/wait/press/type");
        auto* ifs = dynamic_cast<IfStmt*>(p.statements[0].get());
        check(ifs && ifs->thenBody.size() == 3, "if body has 3 statements");
    }
    {
        Program p = parse("Key.F1 = said(\"fire\", 2) or said(\"shoot\", 2)\n");
        check(p.error.empty(), "parse or of said()");
        auto* asOr = dynamic_cast<AssignStmt*>(p.statements.empty() ? nullptr : p.statements[0].get());
        check(asOr != nullptr, "or-of-said is an assignment");
    }
    {
        Program p = parse("debug = microphone.CompletePhrase\nsay(\"Ready\")\n");
        check(p.error.empty(), "parse debug and say");
    }
    {
        Program p = parse("said(\"hello\") => Key.Space\n");
        check(p.error.empty(), "parse reverse arrow assignment");
        auto* as = dynamic_cast<AssignStmt*>(p.statements[0].get());
        check(as && as->op == AssignOp::Arrow, "arrow op");
    }
    {
        DeviceBus bus;
        bus.dryRun = true;
        bus.keyboard().dryRun = true;
        Vm vm;
        vm.load(parse("Key.Enter = said(\"jump\")\n"));
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        check(!bus.keyboard().heldThisFrame().count("enter"), "enter not held before speech");
        bus.speech().injectRecognition("jump", 0, false);
        bus.beginFrame();
        // beginFrame poll() clears inject — inject AFTER poll
        bus.speech().injectRecognition("jump", 0, false);
        vm.tick(bus);
        bus.endFrame();
        check(bus.keyboard().heldThisFrame().count("enter") == 1, "enter held after said jump");

        auto phrases = vm.collectSaidPhrases();
        check(phrases.size() == 1 && phrases[0] == "jump", "collect said phrases");
    }
    {
        DeviceBus bus;
        bus.dryRun = true;
        bus.keyboard().dryRun = true;
        Vm vm;
        vm.load(parse("var.x = 3\ndebug = var.x\n"));
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        check(bus.getVar("x").asNumber() == 3, "var assignment");
        check(bus.consumeDebug() == "3", "debug output");
    }
    {
        DeviceBus bus;
        bus.dryRun = true;
        Vm vm;
        vm.load(parse("p5.A = true\n"));
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        check(!bus.warnings().empty(), "unsupported device warning");
    }
    {
        Program p = parse("midi.BassDrum1 = true\n"
                          "midi.C5 = space\n"
                          "midi.MiddleC = Key.Space\n"
                          "midi.Volume = 50%\n"
                          "midi.Channel4.Instrument = 1\n"
                          "midi.FSharp8 = Enter\n"
                          "if pressed(Space) then midi.C5Velocity = 80%\n"
                          "debug = 25% of 8\n");
        if (!p.error.empty()) std::cerr << "midi parse: " << p.error << " line " << p.errorLine << "\n";
        check(p.error.empty(), "parse MIDI note/volume/percent script");
        DeviceBus bus;
        bus.dryRun = true;
        Vm vm;
        Program run = parse("midi.BassDrum1 = true\n"
                            "midi.Volume = 50%\n"
                            "midi.Instrument = 5\n"
                            "debug = 25% of 8\n");
        if (!run.error.empty()) std::cerr << "midi run parse: " << run.error << "\n";
        vm.load(std::move(run));
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        check(bus.warnings().empty(), "midi is a supported device");
        check(bus.get("midi.volume").asNumber() > 0.49 && bus.get("midi.volume").asNumber() < 0.51,
              "midi.Volume 50% is 0.5");
        check(bus.get("midi.instrument").asNumber() == 5, "midi.Instrument stores 5");
        check(bus.get("debug").asNumber() == 2, "25% of 8 is 2");
        check(bus.get("midi.firstnote").asNumber() == -1, "midi.FirstNote is -1 with no input");
    }
    {
        DeviceBus bus;
        bus.dryRun = true;
        Vm vm;
        vm.load(parse("wiimote.A = true\n"
                      "PPJoy.Analog0 = 0.5\n"
                      "debug = joystick.x\n"
                      "Key.Enter = XInput.A or Any.Cross\n"));
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        check(bus.warnings().empty(), "controller devices do not warn");
        check(bus.get("wiimote.A").isTruthy(), "wiimote.A can be set without hardware");
        check(bus.get("ppjoy.analog0").asNumber() == 0.5, "PPJoy analog0 stores value");
        Program p = parse("if joystick.Button1 then\n  mouse.LeftButton = true\nendif\n"
                          "debug = wiimote.Pitch\n"
                          "debug = XInput1.Joy1X\n"
                          "Any.Vibration1 = 0.2\n");
        if (!p.error.empty()) std::cerr << "controller parse: " << p.error << " line " << p.errorLine << "\n";
        check(p.error.empty(), "parse joystick/wiimote/xinput/any script");
    }
    {
        Program p = parse("if starting then var.Control = 1\n"
                          "W = XInput1.Joy1Y > 0.2\n"
                          "S = XInput1.Joy1Y < -0.2\n"
                          "A = XInput1.Joy1X < -0.2\n"
                          "D = XInput1.Joy1X > 0.2\n"
                          "Mouse.DirectInputX = Mouse.DirectInputX + 15*deadzone(XInput1.Joy2X)\n"
                          "Mouse.DirectInputY = Mouse.DirectInputY - 15*deadzone(XInput1.Joy2Y)\n"
                          "key.1 = XInput1.Up\n"
                          "key.2 = XInput1.Down\n"
                          "KEY.space = XInput1.A\n"
                          "key.c = XInput1.B\n"
                          "R = XInput1.X\n"
                          "key.e= XInput1.Y\n"
                          "key.r  = XInput1.RightShoulder\n"
                          "key.shift = XInput1.LeftShoulder\n"
                          "Mouse.LeftButton = XInput1.RightTrigger\n"
                          "Mouse.RightButton = XInput1.LeftTrigger\n"
                          "Escape = XInput1.Start\n"
                          "if XInput1.RightTrigger then\n"
                          "   if PPJoy1.Digital0 then\n"
                          "      XInput1.Vibration1 = 1\n"
                          "   else\n"
                          "      XInput1.Vibration2 = 1\n"
                          "   end\n"
                          "   wait 1 s\n"
                          "   if PPJoy1.Digital0 then\n"
                          "      XInput1.Vibration1 = 0\n"
                          "   else\n"
                          "      XInput1.Vibration2 = 0\n"
                          "   end IF\n"
                          "end IF\n");
        if (!p.error.empty()) std::cerr << "guru xbox parse: " << p.error << " line " << p.errorLine << "\n";
        check(p.error.empty(), "parse Guru Xbox 360 script");
    }
    {
        Program p = parse("Wiimote.Led1 = true\n"
                          "if wiimote.PointerVisible but not var.PointerBump then\n"
                          "mouse.x = wiimote.PointerX\n"
                          "mouse.y = wiimote.PointerY\n"
                          "end if\n"
                          "mouse.LeftButton = Wiimote.A and KeepDown(Wiimote.PointerVisible,0.5s)\n"
                          "mouse.RightButton = Wiimote.B and KeepDown(Wiimote.PointerVisible,0.5s)\n"
                          "mouse.MiddleButton = Wiimote.One and KeepDown(Wiimote.PointerVisible,0.5s)\n"
                          "if wiimote.Plus then\n"
                          "mouse.WheelUp = true\n"
                          "wait 30ms\n"
                          "mouse.WheelUp = false\n"
                          "wait 30ms\n"
                          "end if\n"
                          "if wiimote.Minus then\n"
                          "mouse.WheelDown = true\n"
                          "wait 30ms\n"
                          "mouse.WheelDown = false\n"
                          "wait 30ms\n"
                          "end if\n"
                          "key.W = Wiimote.Nunchuk.JoyY < -0.4\n"
                          "key.S = Wiimote.Nunchuk.JoyY > 0.4\n"
                          "key.A = Wiimote.Nunchuk.JoyX < -0.4\n"
                          "key.D = Wiimote.Nunchuk.JoyX > 0.4\n"
                          "key.Space = wiimote.Nunchuk.Z\n"
                          "key.LeftControl = wiimote.Nunchuk.C\n"
                          "key.Tab = wiimote.Down\n"
                          "key.1 = wiimote.Left\n"
                          "key.2 = wiimote.Right\n"
                          "key.3 = wiimote.Up\n"
                          "key.LeftShift = wiimote.Two\n"
                          "key.Escape = wiimote.Home\n");
        if (!p.error.empty()) std::cerr << "wii pointer parse: " << p.error << " line " << p.errorLine << "\n";
        check(p.error.empty(), "parse Wiimote pointer/nunchuk script");
        DeviceBus bus;
        bus.dryRun = true;
        bus.keyboard().dryRun = true;
        bus.mouse().dryRun = true;
        Vm vm;
        vm.load(std::move(p));
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        check(bus.warnings().empty(), "wiimote pointer script has no unknown-device warnings");
        check(!bus.get("wiimote.pointervisible").isTruthy(), "pointer hidden without IR");
        check(bus.get("wiimote.nunchuk.joyx").asNumber() == 0, "nunchuk JoyX is 0 without hardware");
    }
    {
        DeviceBus bus;
        bus.dryRun = true;
        bus.keyboard().dryRun = true;
        Vm vm;
        vm.load(parse("Key.Space = KeepDown(true, 0.5s)\n"));
        auto t0 = std::chrono::steady_clock::now();
        vm.setNow(t0);
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        check(bus.keyboard().heldThisFrame().count("space") == 1, "KeepDown 0.5s true while held");
        vm.setNow(t0 + std::chrono::milliseconds(200));
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        check(bus.keyboard().heldThisFrame().count("space") == 1, "KeepDown 0.5s still true after 200ms");
    }
    {
        auto tokens = Lexer("KeepDown(Microphone.Said(\"go up\"), .50)\n").tokenize();
        bool sawDotNum = false;
        for (const auto& t : tokens)
            if (t.kind == TokenKind::Number && t.number > 0.49 && t.number < 0.51) sawDotNum = true;
        check(sawDotNum, "lexer .50 as number");
    }
    {
        Program p = parse("Key.1 = KeepDown(Microphone.Said(\"1\"), .10)\nKey.9 = said(\"9\")\n");
        if (!p.error.empty()) std::cerr << "Key.N parse: " << p.error << " line " << p.errorLine << "\n";
        check(p.error.empty(), "parse Key.1 and Key.9");
        auto* as = p.statements.empty() ? nullptr : dynamic_cast<AssignStmt*>(p.statements[0].get());
        auto* mem = as ? dynamic_cast<MemberExpr*>(as->target.get()) : nullptr;
        check(mem && mem->name == "1", "Key.1 member name is 1 not 0.1");
        DeviceBus bus;
        bus.dryRun = true;
        bus.keyboard().dryRun = true;
        Vm vm;
        vm.load(std::move(p));
        bus.beginFrame();
        bus.speech().injectRecognition("one", 0, false);
        vm.tick(bus);
        bus.endFrame();
        check(bus.keyboard().heldThisFrame().count("1") == 1, "saying one holds Key.1");
    }
    {
        Program p = parse("if said(\"walk\")\nw=1\nendif\nif said(\"halt\")\nw=0\nendif\n");
        check(p.error.empty(), "endif does not eat the next if");
        check(p.statements.size() == 2, "two if statements after endif");
    }
    {
        Program p = parse("Key.W = KeepDown(Microphone.Said(\"step foward\"), .50)\n"
                          "toggle(key.ctrl)=said(\"radio\")\n"
                          "mouse.leftbutton = KeepDown(Microphone.Said(\"fire\"), .10)\n"
                          "var.rate = 12 Hz\n"
                          "if KeepDown(Microphone.Said(\"scroll right\"),.50)\n"
                          "wait 0.20/var.rate\n"
                          "end if\n");
        if (!p.error.empty()) std::cerr << "parse err: " << p.error << " line " << p.errorLine << "\n";
        check(p.error.empty(), "parse KeepDown, Microphone.Said, toggle, mouse, Hz");
        Vm vm;
        vm.load(std::move(p));
        auto phrases = vm.collectSaidPhrases();
        bool hasFire = false, hasRadio = false, hasScroll = false;
        for (const auto& s : phrases) {
            if (s == "fire") hasFire = true;
            if (s == "radio") hasRadio = true;
            if (s == "scroll right") hasScroll = true;
        }
        check(hasFire && hasRadio && hasScroll, "collect phrases from Microphone.Said and said()");
    }
    {
        DeviceBus bus;
        bus.dryRun = true;
        bus.keyboard().dryRun = true;
        Vm vm;
        vm.load(parse("Key.Space = KeepDown(said(\"jump\"), .50)\n"));
        auto t0 = std::chrono::steady_clock::now();
        vm.setNow(t0);
        bus.beginFrame();
        bus.speech().injectRecognition("jump", 0, false);
        vm.tick(bus);
        bus.endFrame();
        check(bus.keyboard().heldThisFrame().count("space") == 1, "KeepDown true on said pulse");
        vm.setNow(t0 + std::chrono::milliseconds(200));
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        check(bus.keyboard().heldThisFrame().count("space") == 1, "KeepDown still true after 200ms");
        vm.setNow(t0 + std::chrono::milliseconds(600));
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        check(bus.keyboard().heldThisFrame().count("space") == 0, "KeepDown false after 500ms");
    }
    {
        DeviceBus bus;
        bus.dryRun = true;
        bus.keyboard().dryRun = true;
        Vm vm;
        vm.load(parse("toggle(key.ctrl)=said(\"radio\")\n"));
        bus.beginFrame();
        bus.speech().injectRecognition("radio", 0, false);
        vm.tick(bus);
        bus.endFrame();
        check(bus.keyboard().heldThisFrame().count("ctrl") == 1, "toggle on first said");
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        bus.beginFrame();
        bus.speech().injectRecognition("radio", 0, false);
        vm.tick(bus);
        bus.endFrame();
        check(bus.keyboard().heldThisFrame().count("ctrl") == 0, "toggle off on second said");
    }
    {
        std::ifstream in("scripts/examples/voice_game.pie");
        if (!in) in.open("../../scripts/examples/voice_game.pie");
        if (!in) in.open("../scripts/examples/voice_game.pie");
        check(static_cast<bool>(in), "open voice_game.pie");
        if (in) {
            std::stringstream ss;
            ss << in.rdbuf();
            Program p = parse(ss.str());
            if (!p.error.empty()) std::cerr << "voice_game parse: " << p.error << " line " << p.errorLine << "\n";
            check(p.error.empty(), "parse full voice_game.pie example");
        }
    }
    {
        Program p = parse("if said(\" driving mode\") then\n"
                          "var.mouse ++\n"
                          "wait 2s\n"
                          "endif\n"
                          "if var.mouse > 1 then\n"
                          "var.mouse = 0\n"
                          "Say(\"Mouse Unlocked\")\n"
                          "endif\n"
                          "if var.mouse = true then\n"
                          "if delta(smooth(mouse.DirectInputx))<-2 then\n"
                          "key.A=1\n"
                          "else key.A=0\n"
                          "if delta(smooth(mouse.DirectInputx))>2 then\n"
                          "key.D=1\n"
                          "else key.D=0\n"
                          "endif\n");
        if (!p.error.empty()) std::cerr << "driving parse: " << p.error << " line " << p.errorLine << "\n";
        check(p.error.empty(), "parse driving mode script");
        check(p.statements.size() == 3, "driving mode is three ifs");
    }
    {
        DeviceBus bus;
        bus.dryRun = true;
        Vm vm;
        vm.load(parse("var.mouse ++\nvar.mouse ++\n"));
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        check(bus.getVar("mouse").asNumber() == 2, "var.mouse ++ twice");
    }
    {
        Program p = parse("w=1\ns=0\n");
        check(p.error.empty() && p.statements.size() == 2, "w=1 and s=0 are two statements");
        auto* a0 = p.statements.size() >= 1 ? dynamic_cast<AssignStmt*>(p.statements[0].get()) : nullptr;
        auto* a1 = p.statements.size() >= 2 ? dynamic_cast<AssignStmt*>(p.statements[1].get()) : nullptr;
        auto* n0 = a0 ? dynamic_cast<NameExpr*>(a0->target.get()) : nullptr;
        auto* n1 = a1 ? dynamic_cast<NameExpr*>(a1->target.get()) : nullptr;
        check(n0 && n0->name == "w", "first assign target is w");
        check(n1 && n1->name == "s", "second assign target is s");
        Program w = parse("wait 1s\n");
        check(w.error.empty() && w.statements.size() == 1, "wait 1s still parses");
    }
    {
        Lexer lexer("if said(\"walk\"\nw=1\nendif\nkey.enter =\n");
        Parser parser(lexer.tokenize());
        Program p = parser.parseAll();
        check(!p.errors.empty(), "parseAll reports errors");
        check(!p.error.empty(), "parseAll sets first error");
    }
    {
        Program p = parse("if said(\"walk\") error see me\nw=1\nendif\n");
        check(!p.error.empty(), "junk after said() is a parse error");
        check(p.errorLine == 1, "junk after said() reports line 1");
    }
    {
        DeviceBus bus;
        bus.dryRun = true;
        bus.beginFrame();
        bus.speech().injectRecognition("go backward halt", 0, false);
        check(bus.speech().said("halt", 2), "halt matches inside go backward halt");
        check(bus.speech().said("go backward", 2), "go backward matches concatenated phrase");
        check(!bus.speech().said("go down", 2), "go down does not match go backward halt");
        bus.beginFrame();
        bus.speech().injectRecognition("walk forward", 0, false);
        check(bus.speech().said("walk", 2), "walk matches walk forward");
        check(bus.speech().said("walk forward", 2), "walk forward exact");
        bus.beginFrame();
        bus.speech().injectRecognition("go backwards", 0, false);
        check(bus.speech().said("go backward", 2), "backwards matches backward");
    }
    {
        std::ifstream in("scripts/examples/voice_game.pie");
        if (!in) in.open("../../scripts/examples/voice_game.pie");
        if (!in) in.open("../scripts/examples/voice_game.pie");
        std::stringstream ss;
        ss << in.rdbuf();
        DeviceBus bus;
        bus.dryRun = true;
        bus.keyboard().dryRun = true;
        Vm vm;
        vm.load(parse(ss.str()));
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        Program parsed = parse(ss.str());
        auto* walkIf = parsed.statements.empty() ? nullptr : dynamic_cast<IfStmt*>(parsed.statements[0].get());
        check(walkIf && walkIf->thenBody.size() == 6, "walk if has six key assignments");
        bus.beginFrame();
        bus.speech().injectRecognition("walk", 0, false);
        vm.tick(bus);
        bus.endFrame();
        check(bus.keyboard().heldThisFrame().count("w") == 1, "voice_game walk holds W");
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        check(bus.keyboard().heldThisFrame().count("w") == 1, "voice_game walk still holds W next frame");
        bus.beginFrame();
        bus.speech().injectRecognition("go backward", 0, false);
        vm.tick(bus);
        bus.endFrame();
        check(bus.keyboard().heldThisFrame().count("s") == 1, "voice_game go backward holds S");
        bus.beginFrame();
        bus.speech().injectRecognition("halt", 0, false);
        vm.tick(bus);
        bus.endFrame();
        check(bus.keyboard().heldThisFrame().count("s") == 0, "voice_game halt releases S after go backward");
        check(bus.keyboard().heldThisFrame().count("w") == 0, "voice_game halt releases W");
    }
    {
        DeviceBus bus;
        bus.dryRun = true;
        bus.speech().setSleepWakeEnabled(true);
        bus.beginFrame();
        bus.speech().injectRecognition("walk", 0, false);
        check(bus.speech().said("walk", 2), "walk works while awake");
        bus.beginFrame();
        bus.speech().injectRecognition("go to sleep", 0, false);
        check(bus.speech().sleeping(), "go to sleep enters sleep");
        check(!bus.speech().said("walk", 2), "script commands ignored while sleeping");
        bus.beginFrame();
        bus.speech().injectRecognition("walk", 0, false);
        check(!bus.speech().said("walk", 2), "walk still ignored while sleeping");
        bus.beginFrame();
        bus.speech().injectRecognition("wake up", 0, false);
        check(!bus.speech().sleeping(), "wake up leaves sleep");
        bus.beginFrame();
        bus.speech().injectRecognition("walk", 0, false);
        check(bus.speech().said("walk", 2), "walk works after wake up");
        bus.speech().setSleepWakeEnabled(false);
        bus.beginFrame();
        bus.speech().injectRecognition("go to sleep", 0, false);
        check(!bus.speech().sleeping(), "sleep feature can be disabled");
        bus.beginFrame();
        bus.speech().injectRecognition("walk", 0, false);
        check(bus.speech().said("walk", 2), "commands still work with sleep disabled");
    }
    {
        Program p = parse("if said(\"left\") then\n  var.speed = [-3, 0]\nend if\n"
                          "if said(\"fast\") then\n  var.speed = var.speed * 2\nend if\n"
                          "mouse.DirectInput2D += var.speed\n");
        if (!p.error.empty()) std::cerr << "vector script parse: " << p.error << " line " << p.errorLine << "\n";
        check(p.error.empty(), "parse DirectInput2D vector script");
    }
    {
        DeviceBus bus;
        bus.dryRun = true;
        bus.mouse().dryRun = true;
        Vm vm;
        vm.load(parse("var.speed = [3, -2]\nmouse.DirectInput2D += var.speed\n"));
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        check(bus.mouse().get("directinputx").asNumber() == 3, "DirectInput2D += x");
        check(bus.mouse().get("directinputy").asNumber() == -2, "DirectInput2D += y");
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        check(bus.mouse().get("directinputx").asNumber() == 6, "DirectInput2D += again");
        vm.load(parse("var.speed = [8, 4]\nvar.speed = var.speed / 2\n"));
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        auto halved = bus.getVar("speed").asVector();
        check(halved.size() == 2 && halved[0] == 4 && halved[1] == 2, "vector divide by scalar");
    }
    {
        auto tokens = Lexer("q = said('change weapon', 2)\n").tokenize();
        bool sawString = false;
        for (const auto& t : tokens)
            if (t.kind == TokenKind::String && t.text == "change weapon") sawString = true;
        check(sawString, "single-quoted said() string");
        Program p = parse("q = said('change weapon', 2)\n"
                          "if said('pause game', 1) then\n"
                          "  press Esc\n"
                          "  wait 10 ms\n"
                          "  release Esc\n"
                          "end if\n"
                          "if said('grenade',1) then\n"
                          "  press mouse.RightButton\n"
                          "  wait 10 ms\n"
                          "  release mouse.RightButton\n"
                          "end if\n");
        if (!p.error.empty()) std::cerr << "press script parse: " << p.error << " line " << p.errorLine << "\n";
        check(p.error.empty(), "parse press Esc and single-quoted said");
    }
    {
        Program p = parse("if starting then\n  cursor2.visible = false\nendif\n"
                          "mouse.Swallow = true\n"
                          "fakemouse.DirectInputX = mouse1.DirectInputX\n"
                          "if mouse2.present but not var.shown2 then\n"
                          "  cursor2.visible = true\n"
                          "endif\n"
                          "var.deltax2 = Delta(mouse2.DirectInputX)\n"
                          "var.velocity = ([(smooth(var.deltaX2) / 24ms) in inches per second, "
                          "(smooth(var.deltaY2) / 24ms) in inches per second] )\n"
                          "var.speed = |var.velocity|\n"
                          "var.Mouse2x = var.Mouse2x pixels + (var.deltax2 in pixels)\n"
                          "if var.Mouse2x < screen.DesktopLeft then var.Mouse2x = screen.DesktopLeft\n"
                          "Cursor2.PosX = var.Mouse2x\n"
                          "Cursor2.LeftButton = mouse2.LeftButton\n");
        if (!p.error.empty()) std::cerr << "mouse party parse: " << p.error << " line " << p.errorLine << "\n";
        check(p.error.empty(), "parse mouse party constructs");
        check(p.statements.size() >= 8, "mouse party has top-level statements");
    }
    {
        DeviceBus bus;
        bus.dryRun = true;
        bus.mouse().dryRun = true;
        Vm vm;
        vm.load(parse("if starting then\n  var.n = 1\nendif\n"
                      "if var.n = 1 but not var.done then\n  var.done = true\nendif\n"
                      "var.speed = |[3, 4]|\n"
                      "debug = screen.DesktopLeft\n"));
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        check(bus.getVar("n").asNumber() == 1, "starting is true on first frame");
        check(bus.getVar("done").isTruthy(), "but not is logical and");
        check(bus.getVar("speed").asNumber() == 5, "|vector| is magnitude");
        bus.beginFrame();
        vm.load(parse("if starting then var.n = 9\nendif\nvar.x = 1\n"));
        // new load resets VM but bus frames already advanced unless reset
        bus.reset();
        vm.load(parse("if starting then var.n = 9\nendif\n"));
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        check(bus.getVar("n").asNumber() == 9, "starting after bus.reset");
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        check(bus.getVar("n").asNumber() == 9, "starting false leaves var.n");
    }
    {
        Program p = parse("var.x = MapRange(0, -1, 1, 0, 1)\n"
                          "var.y = 7.5 mod 3.5\n"
                          "var.z = 8 xor 0\n"
                          "Enter = swallow(Left)\n"
                          "if Pressed(Key.A) then var.p = 1\nendif\n");
        if (!p.error.empty()) std::cerr << "manual leftovers parse: " << p.error << " line " << p.errorLine << "\n";
        check(p.error.empty(), "parse MapRange, mod, xor, swallow, Pressed");
        DeviceBus bus;
        bus.dryRun = true;
        Vm vm;
        vm.load(parse("var.x = MapRange(-1, -1, 1, 0, 1)\nvar.y = 7.5 mod 3.5\nvar.z = 2 ^ 3\n"));
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        check(bus.getVar("x").asNumber() == 0, "MapRange -1..1 to 0..1");
        check(bus.getVar("y").asNumber() == 0.5, "7.5 mod 3.5");
        check(bus.getVar("z").asNumber() == 8, "2 ^ 3 is power");
    }
    {
        Program p = parse("var.s = 0\nfor var.i = 1 to 3\n  var.s = var.s + 1\nend for\n"
                          "while var.s < 5 do var.s = var.s + 1\n"
                          "debug = neither false nor false\n"
                          "var.ok = 0 < 0.3 < 1\n"
                          "var.near = 1 ~= 1\n"
                          "var.diff = 1 isn't 2\n"
                          "Osc1.port = 54934\n"
                          "Osc1.broadcast = true\n"
                          "debug = window.Title\n"
                          "debug = screen.WorkAreaWidth\n"
                          "debug = touchpad.x\n"
                          "debug = classic.A\n"
                          "debug = sixaxis.Cross\n"
                          "SendOsc(\"127.0.0.1\", 54934, \"/foo\", 1)\n");
        if (!p.error.empty()) std::cerr << "leftover parse: " << p.error << " line " << p.errorLine << "\n";
        check(p.error.empty(), "parse while/for/neither/between/osc/window");
        DeviceBus bus;
        bus.dryRun = true;
        Vm vm;
        vm.load(parse("var.s = 0\nfor var.i = 1 to 3\n  var.s = var.s + 1\nend for\n"
                      "var.ok = 0 < 0.3 < 1\n"
                      "var.near = 1 ~= 1\n"
                      "var.nn = neither false nor true\n"
                      "osc1.port = 9000\n"
                      "pie.epsilon = 0.01\n"));
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        check(bus.getVar("s").asNumber() == 3, "for 1 to 3 runs three times");
        check(bus.getVar("ok").isTruthy(), "chained 0 < 0.3 < 1");
        check(bus.getVar("near").isTruthy(), "1 ~= 1");
        check(!bus.getVar("nn").isTruthy(), "neither false nor true is false");
        check(bus.get("osc1.port").asNumber() == 9000, "osc1.port stores");
        check(bus.get("pie.epsilon").asNumber() == 0.01, "pie.epsilon stores");
        check(bus.get("window.title").kind == ValueKind::String, "window.Title is a string");
        check(bus.get("screen.workareawidth").asNumber() > 0, "screen.WorkAreaWidth");
        check(bus.warnings().empty(), "window/osc/screen do not warn");
    }
    {
        Program p = parse("Key.End = true\nHidePie\nBeepAsterisk\nvar.u = Unicode0024\n");
        if (!p.error.empty()) std::cerr << "key leftover parse: " << p.error << " line " << p.errorLine << "\n";
        check(p.error.empty(), "parse Key.End, HidePie, Unicode");
        check(KeyboardDevice::isKnownKey("end"), "End is a known key");
        check(KeyboardDevice::isKnownKey("undo"), "Undo is a known key");
        check(KeyboardDevice::isKnownKey("Unicode0024"), "Unicode0024 is a known key");
        check(KeyboardDevice::isKnownKey("Scan5B"), "Scan5B is a known key");
        DeviceBus bus;
        bus.dryRun = true;
        Vm vm;
        vm.load(parse("debug = Key.End\ndebug = mouse.CursorPos\nKalman(1, 2, 3)\n"));
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        check(!vm.hasError(), "Kalman does not abort the script");
        check(bus.get("mouse.cursorpos").kind == ValueKind::Vector, "mouse.CursorPos is a vector");
    }
    {
        Program p = parse("Key.ctrl+t = KeepDown(Microphone.Said(\"transform\"), .15)\n"
                          "Key.ctrl+j = KeepDown(Microphone.Said(\"new layer\"), .10)\n"
                          "^C = KeepDown(said(\"copy\"), 0.2)\n");
        if (!p.error.empty()) std::cerr << "combo parse: " << p.error << " line " << p.errorLine << "\n";
        check(p.error.empty(), "parse Key.ctrl+t KeepDown combos");
        check(KeyboardDevice::isKnownKey("ctrl+t"), "ctrl+t is a known combo");
        check(KeyboardDevice::isKnownKey("ctrl+j"), "ctrl+j is a known combo");
        DeviceBus bus;
        bus.dryRun = true;
        Vm vm;
        vm.load(parse("Key.ctrl+t = true\n"));
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        check(bus.keyboard().heldThisFrame().count("ctrl") > 0, "ctrl+t holds ctrl");
        check(bus.keyboard().heldThisFrame().count("t") > 0, "ctrl+t holds t");
        vm.load(parse("var.k = KeepDown(true, .15)\n"));
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        check(bus.getVar("k").isTruthy(), "KeepDown .15 parses as 0.15s");
    }
    {
        Program p = parse("Key.alt+backspace = KeepDown(Microphone.Said(\"undo that\"), .10)\n");
        if (!p.error.empty()) std::cerr << "alt+backspace parse: " << p.error << " line " << p.errorLine << "\n";
        check(p.error.empty(), "parse Key.alt+backspace KeepDown");
        check(KeyboardDevice::isKnownKey("alt+backspace"), "alt+backspace is a known combo");
        DeviceBus bus;
        bus.dryRun = true;
        Vm vm;
        vm.load(parse("Key.alt+backspace = true\n"));
        bus.beginFrame();
        vm.tick(bus);
        bus.endFrame();
        check(bus.keyboard().heldThisFrame().count("alt") > 0, "alt+backspace holds alt");
        check(bus.keyboard().heldThisFrame().count("backspace") > 0, "alt+backspace holds backspace");
    }

    if (g_failed) {
        std::cerr << g_failed << " test(s) failed\n";
        return 1;
    }
    std::cout << "all tests passed\n";
    return 0;
}
