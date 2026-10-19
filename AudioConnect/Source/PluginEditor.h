// Audio Connect - by MiKeys Dsz Studios
#pragma once
#include "PluginProcessor.h"

#ifndef AC_HAS_POPPINS
 #define AC_HAS_POPPINS 0
#endif
#if AC_HAS_POPPINS
 #include <BinaryData.h>
#endif

namespace ui
{
namespace col
{
    const juce::Colour bg     { 0xff0d0f14 }, card { 0xff161a22 }, line { 0xff262b36 },
                       text   { 0xffe9ecf3 }, dim  { 0xff8b93a5 }, accent { 0xff7c5cff },
                       teal   { 0xff2ee6c5 }, warn { 0xffff6b6b };
}

inline juce::Colour senderColour (int i)
{
    static const juce::uint32 p[] { 0xff7c5cff, 0xff2ee6c5, 0xffffb347, 0xffff6b9d, 0xff5cb8ff, 0xffa6e22e };
    return juce::Colour (p[(size_t) i % 6]);
}

// Poppins: embedded from Assets/ when present, otherwise an installed "Poppins", otherwise system font
struct Fonts
{
    static juce::Typeface::Ptr load (bool semi)
    {
       #if AC_HAS_POPPINS
        static auto reg = juce::Typeface::createSystemTypefaceFor (BinaryData::PoppinsRegular_ttf,  BinaryData::PoppinsRegular_ttfSize);
        static auto sem = juce::Typeface::createSystemTypefaceFor (BinaryData::PoppinsSemiBold_ttf, BinaryData::PoppinsSemiBold_ttfSize);
        return semi ? sem : reg;
       #else
        juce::ignoreUnused (semi);
        return nullptr;
       #endif
    }
    static juce::Font make (float h, bool semi)
    {
        if (auto tf = load (semi)) return juce::Font (juce::FontOptions (tf).withHeight (h));
        return juce::Font (juce::FontOptions ("Poppins", h, semi ? juce::Font::bold : juce::Font::plain));
    }
    static juce::Font regular (float h) { return make (h, false); }
    static juce::Font semi (float h)    { return make (h, true); }
};

class AcLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AcLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, col::bg);
        setColour (juce::ComboBox::backgroundColourId, col::bg);
        setColour (juce::ComboBox::outlineColourId, col::line);
        setColour (juce::ComboBox::textColourId, col::text);
        setColour (juce::ComboBox::arrowColourId, col::dim);
        setColour (juce::PopupMenu::backgroundColourId, col::card);
        setColour (juce::PopupMenu::textColourId, col::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent);
        setColour (juce::TextButton::buttonColourId, col::line);
        setColour (juce::TextButton::textColourOffId, col::text);
        setColour (juce::Slider::thumbColourId, col::teal);
        setColour (juce::Slider::trackColourId, col::accent);
        setColour (juce::Slider::backgroundColourId, col::line);
        setColour (juce::Slider::textBoxTextColourId, col::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, col::text);
        setColour (juce::TextEditor::backgroundColourId, col::bg);
        setColour (juce::TextEditor::textColourId, col::text);
        setColour (juce::TextEditor::outlineColourId, col::line);
        setColour (juce::TextEditor::focusedOutlineColourId, col::accent);
        if (auto tf = Fonts::load (false)) setDefaultSansSerifTypeface (tf);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override        { return Fonts::regular (12.0f); }
    juce::Font getLabelFont (juce::Label&) override              { return Fonts::regular (12.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return Fonts::semi (12.0f); }
    juce::Font getPopupMenuFont() override                       { return Fonts::regular (12.5f); }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        juce::Rectangle<float> sw (r.getX(), r.getCentreY() - 9.0f, 34.0f, 18.0f);
        const bool on = b.getToggleState();
        g.setColour (on ? col::accent : juce::Colour (0xff2a2f3b));
        g.fillRoundedRectangle (sw, 9.0f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (on ? sw.getRight() - 16.0f : sw.getX() + 2.0f, sw.getY() + 2.0f, 14.0f, 14.0f);
        g.setColour (col::text);
        g.setFont (Fonts::regular (12.0f));
        g.drawText (b.getButtonText(), (int) sw.getRight() + 8, (int) r.getY(),
                    (int) (r.getWidth() - sw.getWidth() - 8.0f), (int) r.getHeight(),
                    juce::Justification::centredLeft);
    }
};

//==============================================================================
class DevicePanel : public juce::Component, private juce::ChangeListener
{
public:
    DevicePanel (ac::RouterEngine& e, int idx, const juce::String& t, const juce::String& s)
        : engine (e), dev (e.device (idx)), title (t), sub (s), index (idx)
    {
        addAndMakeVisible (typeBox);
        addAndMakeVisible (deviceBox);
        addAndMakeVisible (rescanBtn);
        rescanBtn.setButtonText ("Rescan");
        typeBox.setTextWhenNothingSelected ("Driver");
        deviceBox.setTextWhenNothingSelected ("Select output device...");

        rescanBtn.onClick = [this] { dev.rescan(); engine.changed.sendChangeMessage(); };

        if (index == 0)
        {
            addAndMakeVisible (autoBtn);
            autoBtn.setButtonText ("Auto-detect");
            autoBtn.onClick = [this] { dev.rescan(); dev.autoDetect(); };
        }
        else
        {
            addAndMakeVisible (autoBtn);
            autoBtn.setButtonText ("Use PC default");
            autoBtn.onClick = [this] { dev.openSystemDefault(); };
        }

        typeBox.onChange = [this] { if (! updating) { updating = true; fillDevices(); updating = false; } };
        deviceBox.onChange = [this]
        {
            if (updating || deviceBox.getSelectedId() <= 0) return;
            dev.open (typeBox.getText(), deviceBox.getText());
        };

        engine.changed.addChangeListener (this);
        refresh();
    }
    ~DevicePanel() override { engine.changed.removeChangeListener (this); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (col::card);  g.fillRoundedRectangle (b, 12.0f);
        g.setColour (col::line);  g.drawRoundedRectangle (b.reduced (0.5f), 12.0f, 1.0f);

        g.setColour (col::text);  g.setFont (Fonts::semi (13.0f));
        g.drawText (title, 14, 8, getWidth() - 28, 18, juce::Justification::left);

        const bool open = dev.isOpen();
        g.setColour (open ? col::teal : col::warn);
        g.fillEllipse (14.0f, 31.0f, 7.0f, 7.0f);
        g.setColour (col::dim);  g.setFont (Fonts::regular (10.5f));
        juce::String status = open ? juce::String (dev.getNumOutputs()) + " outputs  |  "
                                         + juce::String (dev.getSampleRate() / 1000.0, 1) + " kHz  |  "
                                         + juce::String (dev.getBufferSize()) + " smp"
                                   : (dev.getLastError().isNotEmpty() ? dev.getLastError() : sub);
        g.drawText (status, 26, 27, getWidth() - 40, 16, juce::Justification::left, true);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (12, 0);
        b.removeFromTop (48);
        typeBox.setBounds (b.removeFromTop (26));   b.removeFromTop (6);
        deviceBox.setBounds (b.removeFromTop (26)); b.removeFromTop (8);
        auto row = b.removeFromTop (24);
        autoBtn.setBounds (row.removeFromLeft (row.getWidth() / 2 - 3));
        row.removeFromLeft (6);
        rescanBtn.setBounds (row);
    }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }

    void fillDevices()
    {
        deviceBox.clear (juce::dontSendNotification);
        auto names = dev.getDeviceNames (typeBox.getText());
        deviceBox.addItemList (names, 1);
        if (dev.isOpen() && typeBox.getText() == dev.getTypeName())
        {
            const int i = names.indexOf (dev.getDeviceName());
            if (i >= 0) deviceBox.setSelectedItemIndex (i, juce::dontSendNotification);
        }
    }

    void refresh()
    {
        updating = true;
        auto types = dev.getTypeNames();
        typeBox.clear (juce::dontSendNotification);
        typeBox.addItemList (types, 1);
        int ti = types.indexOf (dev.getTypeName());
        if (ti < 0 && types.size() > 0) ti = 0;
        if (ti >= 0) typeBox.setSelectedItemIndex (ti, juce::dontSendNotification);
        fillDevices();
        updating = false;
        repaint();
    }

    ac::RouterEngine& engine;
    ac::DeviceOutput& dev;
    juce::String title, sub;
    int index;
    bool updating = false;
    juce::ComboBox typeBox, deviceBox;
    juce::TextButton rescanBtn, autoBtn;
};

//==============================================================================
class RoutePanel : public juce::Component, private juce::ChangeListener
{
public:
    explicit RoutePanel (AudioConnectProcessor& p) : proc (p), engine (p.engine.get())
    {
        setup (rowA, "Route A", "destA", "pairA");
        setup (rowB, "Also send to", "destB", "pairB");
        engine.changed.addChangeListener (this);
        fillPairs (rowA);
        fillPairs (rowB);
    }
    ~RoutePanel() override { engine.changed.removeChangeListener (this); }

    void tick()
    {
        for (auto* r : { &rowA, &rowB })
        {
            const int sig = r->dest.getSelectedId() * 100 + deviceFor (*r).getNumPairs();
            if (sig != r->sig) fillPairs (*r);
            else syncSel (*r);
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (col::card);  g.fillRoundedRectangle (b, 12.0f);
        g.setColour (col::line);  g.drawRoundedRectangle (b.reduced (0.5f), 12.0f, 1.0f);
        g.setColour (col::text);  g.setFont (Fonts::semi (13.0f));
        g.drawText ("ROUTING", 14, 8, 200, 18, juce::Justification::left);
        g.setColour (col::dim);   g.setFont (Fonts::regular (10.5f));
        g.drawText ("Send this track/bus to an output pair. Add a second destination to play on both.",
                    100, 9, getWidth() - 114, 16, juce::Justification::right, true);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (14, 0);
        b.removeFromTop (34);
        layoutRow (rowA, b.removeFromTop (28));
        b.removeFromTop (8);
        layoutRow (rowB, b.removeFromTop (28));
    }

private:
    struct Row
    {
        juce::Label label;
        juce::ComboBox dest, pair;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
        juce::String pairId;
        int sig = -1;
    };

    void setup (Row& r, const juce::String& text, const juce::String& destId, const juce::String& pairId)
    {
        r.pairId = pairId;
        r.label.setText (text, juce::dontSendNotification);
        r.label.setColour (juce::Label::textColourId, col::dim);
        r.dest.addItemList ({ "Off", "Audio Interface", "PC / Headphones" }, 1);
        addAndMakeVisible (r.label);
        addAndMakeVisible (r.dest);
        addAndMakeVisible (r.pair);
        r.att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, destId, r.dest);
        r.dest.onChange = [this, &r] { fillPairs (r); };
        r.pair.onChange = [this, &r]
        {
            if (syncing) return;
            const int id = r.pair.getSelectedId();
            if (id <= 0) return;
            if (auto* prm = proc.apvts.getParameter (r.pairId))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) id));
                prm->endChangeGesture();
            }
        };
    }

    void layoutRow (Row& r, juce::Rectangle<int> b)
    {
        r.label.setBounds (b.removeFromLeft (92));
        r.dest.setBounds (b.removeFromLeft (170));
        b.removeFromLeft (8);
        r.pair.setBounds (b);
    }

    ac::DeviceOutput& deviceFor (Row& r)
    {
        return engine.device (juce::jlimit (0, 1, r.dest.getSelectedId() - 2));
    }

    void syncSel (Row& r)
    {
        if (auto* prm = proc.apvts.getParameter (r.pairId))
        {
            const int v = (int) std::round (prm->convertFrom0to1 (prm->getValue()));
            if (r.pair.getSelectedId() != v && ! r.pair.isPopupActive())
            {
                syncing = true;
                r.pair.setSelectedId (v, juce::dontSendNotification);
                syncing = false;
            }
        }
    }

    void fillPairs (Row& r)
    {
        syncing = true;
        r.pair.clear (juce::dontSendNotification);
        const int dsel = r.dest.getSelectedId() - 2;

        if (dsel < 0)
        {
            r.pair.setEnabled (false);
            r.pair.setTextWhenNothingSelected ("-");
        }
        else
        {
            auto& d = engine.device (dsel);
            auto names = d.getChannelNames();
            const int np = juce::jlimit (1, ac::kMaxPairs, d.getNumPairs());
            for (int k = 0; k < np; ++k)
            {
                juce::String t = "Out " + juce::String (k * 2 + 1) + " + " + juce::String (k * 2 + 2);
                const auto n0 = k * 2 < names.size() ? names[k * 2] : juce::String();
                const auto n1 = k * 2 + 1 < names.size() ? names[k * 2 + 1] : juce::String();
                if (n0.isNotEmpty()) t << "    " << n0 << (n1.isNotEmpty() ? " / " + n1 : " (mono)");
                r.pair.addItem (t, k + 1);
            }
            r.pair.setEnabled (true);
            r.pair.setTextWhenNothingSelected ("Output not available");
        }
        syncing = false;
        r.sig = r.dest.getSelectedId() * 100 + (dsel >= 0 ? engine.device (dsel).getNumPairs() : 0);
        syncSel (r);
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override { fillPairs (rowA); fillPairs (rowB); }

    AudioConnectProcessor& proc;
    ac::RouterEngine& engine;
    Row rowA, rowB;
    bool syncing = false;
};

//==============================================================================
class OutputMap : public juce::Component
{
public:
    OutputMap (AudioConnectProcessor& p) : proc (p), engine (p.engine.get()) {}

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (col::card);  g.fillRoundedRectangle (b, 12.0f);
        g.setColour (col::line);  g.drawRoundedRectangle (b.reduced (0.5f), 12.0f, 1.0f);
        g.setColour (col::text);  g.setFont (Fonts::semi (13.0f));
        g.drawText ("OUTPUT MAP", 14, 8, 200, 18, juce::Justification::left);

        auto infos = engine.snapshot();
        const float w = (float) getWidth() - 28.0f;
        int y = 32;

        for (int d = 0; d < 2; ++d)
        {
            auto& dev = engine.device (d);
            g.setColour (col::dim);  g.setFont (Fonts::regular (11.0f));
            g.drawText (juce::String (d == 0 ? "Audio Interface" : "PC / Headphones") + "  -  "
                            + (dev.isOpen() ? dev.getDeviceName() : "not connected"),
                        14, y, (int) w, 16, juce::Justification::left, true);
            y += 19;

            const int np = juce::jmin (ac::kMaxPairs, dev.getNumPairs());
            if (np > 0)
            {
                const float tw = juce::jmin (84.0f, w / (float) np);
                for (int p = 0; p < np; ++p)
                {
                    juce::Rectangle<float> t (14.0f + tw * (float) p, (float) y, tw - 4.0f, 40.0f);
                    juce::StringArray names;
                    bool mine = false;
                    int firstIdx = -1;
                    for (size_t i = 0; i < infos.size(); ++i)
                        for (int k = 0; k < 2; ++k)
                            if (infos[i].dev[k] == d + 1 && infos[i].pair[k] == p + 1)
                            {
                                names.addIfNotAlreadyThere (infos[i].name);
                                if (firstIdx < 0) firstIdx = (int) i;
                                if (infos[i].ptr == mineSend()) mine = true;
                            }

                    g.setColour (firstIdx >= 0 ? senderColour (firstIdx).withAlpha (0.28f) : col::bg);
                    g.fillRoundedRectangle (t, 7.0f);
                    g.setColour (mine ? col::text : (firstIdx >= 0 ? senderColour (firstIdx) : col::line));
                    g.drawRoundedRectangle (t.reduced (0.5f), 7.0f, mine ? 1.6f : 1.0f);

                    g.setColour (col::dim);   g.setFont (Fonts::regular (9.5f));
                    g.drawText (juce::String (p * 2 + 1) + "+" + juce::String (p * 2 + 2),
                                t.removeFromTop (16.0f).toNearestInt(), juce::Justification::centred);
                    g.setColour (col::text);  g.setFont (Fonts::semi (10.0f));
                    auto label = names.isEmpty() ? juce::String() : names[0];
                    if (names.size() > 1) label << " +" << (names.size() - 1);
                    g.drawText (label, t.toNearestInt(), juce::Justification::centred, true);
                }
            }
            y += 48;
        }
    }

private:
    const ac::Send* mineSend() const { return mine; }
    AudioConnectProcessor& proc;
    ac::RouterEngine& engine;
    const ac::Send* mine = nullptr;   // reserved: highlight own instance
};

//==============================================================================
class AudioConnectEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AudioConnectEditor (AudioConnectProcessor& p)
        : AudioProcessorEditor (&p), proc (p),
          dev0 (p.engine.get(), 0, "AUDIO INTERFACE", "No interface detected - plug one in"),
          dev1 (p.engine.get(), 1, "PC OUTPUT / HEADPHONES", "No system output"),
          route (p), map (p)
    {
        setLookAndFeel (&laf);
        for (auto* c : std::initializer_list<juce::Component*> { &dev0, &dev1, &route, &map, &nameLabel, &nameEditor,
                                                                   &gainLabel, &gain, &bufLabel, &buf, &pass, &autoBtn })
            addAndMakeVisible (c);

        nameLabel.setText ("Instance name (e.g. Loops, Stems)", juce::dontSendNotification);
        nameLabel.setColour (juce::Label::textColourId, col::dim);
        nameLabel.setFont (Fonts::regular (10.5f));
        nameEditor.setFont (Fonts::regular (13.0f));
        nameEditor.setText (proc.getInstanceName(), false);
        nameEditor.onTextChange = [this] { proc.setInstanceName (nameEditor.getText()); };

        for (auto* l : { &gainLabel, &bufLabel })
        {
            l->setColour (juce::Label::textColourId, col::dim);
            l->setFont (Fonts::regular (10.5f));
        }
        gainLabel.setText ("Gain (dB)", juce::dontSendNotification);
        bufLabel.setText ("Buffer (ms) - lower = less latency", juce::dontSendNotification);
        gain.setSliderStyle (juce::Slider::LinearHorizontal);
        buf.setSliderStyle (juce::Slider::LinearHorizontal);
        gain.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
        buf.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);

        pass.setButtonText ("Also pass audio through to the DAW");
        autoBtn.setButtonText ("Auto-assign free output pair");
        autoBtn.onClick = [this] { proc.autoAssign(); };

        gainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "gain", gain);
        bufAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "latency", buf);
        passAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "pass", pass);

        setSize (600, 640);
        startTimerHz (10);
    }

    ~AudioConnectEditor() override { stopTimer(); setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (col::bg);
        g.setColour (col::text);  g.setFont (Fonts::semi (26.0f));
        g.drawText ("Audio Connect", 16, 14, 320, 36, juce::Justification::left);
        g.setColour (col::dim);   g.setFont (Fonts::regular (11.0f));
        g.drawText ("by MiKeys Dsz Studios", 18, 48, 300, 16, juce::Justification::left);

        g.setColour (col::card);  g.fillRoundedRectangle (ctrl.toFloat(), 12.0f);
        g.setColour (col::line);  g.drawRoundedRectangle (ctrl.toFloat().reduced (0.5f), 12.0f, 1.0f);

        auto m = juce::Rectangle<float> ((float) ctrl.getX() + 14.0f, (float) ctrl.getBottom() - 12.0f,
                                         (float) ctrl.getWidth() - 28.0f, 4.0f);
        g.setColour (col::bg);    g.fillRoundedRectangle (m, 2.0f);
        const float lv = juce::jlimit (0.0f, 1.0f, proc.level.load());
        g.setColour (lv > 0.95f ? col::warn : col::teal);
        g.fillRoundedRectangle (m.withWidth (m.getWidth() * lv), 2.0f);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (16);
        auto head = b.removeFromTop (60);
        auto right = head.removeFromRight (240);
        nameLabel.setBounds (right.removeFromTop (16));
        nameEditor.setBounds (right.removeFromTop (28));
        b.removeFromTop (8);

        auto devRow = b.removeFromTop (138);
        dev0.setBounds (devRow.removeFromLeft (devRow.getWidth() / 2 - 6));
        devRow.removeFromLeft (12);
        dev1.setBounds (devRow);
        b.removeFromTop (8);

        route.setBounds (b.removeFromTop (104));
        b.removeFromTop (8);

        ctrl = b.removeFromTop (92);
        auto c = ctrl.reduced (14, 8);
        c.removeFromBottom (8);
        auto left  = c.removeFromLeft (c.getWidth() / 2 - 8);
        c.removeFromLeft (16);
        gainLabel.setBounds (left.removeFromTop (16));
        gain.setBounds (left.removeFromTop (24));
        bufLabel.setBounds (left.removeFromTop (16));
        buf.setBounds (left.removeFromTop (24));
        pass.setBounds (c.removeFromTop (28));
        c.removeFromTop (6);
        autoBtn.setBounds (c.removeFromTop (26));
        b.removeFromTop (8);

        map.setBounds (b);
    }

private:
    void timerCallback() override
    {
        route.tick();
        map.repaint();
        repaint (ctrl);
        if (! nameEditor.hasKeyboardFocus (true) && nameEditor.getText() != proc.getInstanceName())
            nameEditor.setText (proc.getInstanceName(), false);
    }

    AudioConnectProcessor& proc;
    AcLookAndFeel laf;
    DevicePanel dev0, dev1;
    RoutePanel route;
    OutputMap map;
    juce::Rectangle<int> ctrl;
    juce::Label nameLabel, gainLabel, bufLabel;
    juce::TextEditor nameEditor;
    juce::Slider gain, buf;
    juce::ToggleButton pass;
    juce::TextButton autoBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAtt, bufAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> passAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioConnectEditor)
};
} // namespace ui

using AudioConnectEditor = ui::AudioConnectEditor;
