//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "player_traits.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>

class Player {
protected:
    Player() {}
    virtual bool init(unsigned sample_rate) = 0;

public:
    typedef ADLMIDI_AudioFormat Audio_Format;
    typedef ADLMIDI_SampleType Sample_Type;

    static Player *create(Player_Type pt, unsigned sample_rate);
    static Player_Type type_by_name(const char *nam);

    static const char *name(Player_Type pt);
    static const char *version(Player_Type pt);
    static const char *chip_name(Player_Type pt);
    static std::vector<std::string> enumerate_emulators(Player_Type pt);
    static unsigned emulator_by_name(Player_Type pt, const char *name);

    const char *name() const
        { return name(type()); }
    const char *version() const
        { return version(type()); }
    const char *chip_name() const
        { return chip_name(type()); }
    std::vector<std::string> enumerate_emulators() const
        { return enumerate_emulators(type()); }
    unsigned emulator_by_name(const char *name) const
        { return emulator_by_name(type(), name); }

    virtual ~Player() {}
    virtual Player_Type type() const = 0;
    unsigned sample_rate() const { return sample_rate_; }
    virtual bool set_device_identifier(unsigned id) = 0;
    virtual void reset() = 0;
    virtual void panic() = 0;
    virtual const char *emulator_name() const = 0;
    virtual bool set_emulator(unsigned emulator) = 0;
    virtual bool set_embedded_bank(unsigned bank) = 0;
    unsigned emulator() const { return emulator_; }
    virtual unsigned chip_count() const = 0;
    virtual bool set_chip_count(unsigned count) = 0;
    virtual bool load_bank_file(const char *file) = 0;
    virtual bool load_bank_data(const void *data, size_t size) = 0;
    virtual void generate(unsigned nframes, void *left, void *right, const Audio_Format &format) = 0;
    virtual void rt_note_on(unsigned chan, unsigned note, unsigned vel) = 0;
    virtual void rt_note_off(unsigned chan, unsigned note) = 0;
    virtual void rt_note_aftertouch(unsigned chan, unsigned note, unsigned val) = 0;
    virtual void rt_channel_aftertouch(unsigned chan, unsigned val) = 0;
    virtual void rt_controller_change(unsigned chan, unsigned ctl, unsigned val) = 0;
    virtual void rt_program_change(unsigned chan, unsigned pgm) = 0;
    virtual void rt_pitchbend(unsigned chan, unsigned value) = 0;
    virtual void rt_bank_change_msb(unsigned chan, unsigned value) = 0;
    virtual void rt_bank_change_lsb(unsigned chan, unsigned value) = 0;
    virtual bool rt_system_exclusive(const uint8_t *data, unsigned size) = 0;

    bool dynamic_set_chip_count(unsigned nchip);
    bool dynamic_set_emulator(unsigned emulator);
    bool dynamic_load_bank(const char *bankfile);
    void dynamic_panic();

    std::unique_lock<std::mutex> take_lock()
        { return std::unique_lock<std::mutex>(mutex_); }
    std::unique_lock<std::mutex> take_lock(std::try_to_lock_t)
        { return std::unique_lock<std::mutex>(mutex_, std::try_to_lock); }

protected:
    unsigned sample_rate_ = 0;
    unsigned emulator_ = 0;
    std::mutex mutex_;
};

template <Player_Type Pt>
class Generic_Player : public Player {
private:
    typedef Player_Traits<Pt> Traits;
    typedef typename Traits::player player_t;

    struct Deleter { void operator()(player_t *x) { Traits::close(x); } };
    std::unique_ptr<player_t, Deleter> player_;

public:
    virtual ~Generic_Player() {}

    bool init(unsigned sample_rate) override
        {
            sample_rate_ = sample_rate;
            player_.reset(Traits::init(sample_rate));
            return player_ != nullptr;
        }
    Player_Type type() const override
        { return Pt; }
    bool set_device_identifier(unsigned id) override
        { return Traits::set_device_identifier(player_.get(), id) >= 0; }
    void reset() override
        { Traits::reset(player_.get()); }
    void panic() override
        { Traits::panic(player_.get()); }
    const char *emulator_name() const override
        { return Traits::emulator_name(player_.get()); }
    bool set_emulator(unsigned emulator) override
        {
            bool success = Traits::switch_emulator(player_.get(), emulator) >= 0;
            if (success)
                emulator_ = emulator;
            return success;
        }
    unsigned chip_count() const override
        { return Traits::get_num_chips(player_.get()); }
    bool set_chip_count(unsigned count) override
        {
            return Traits::set_num_chips(player_.get(), count) >= 0 && chip_count() == count;
        }
    bool set_embedded_bank(unsigned bank) override
        { return Traits::set_bank(player_.get(), bank) >= 0; }
    bool load_bank_file(const char *file) override
        { return Traits::open_bank_file(player_.get(), file) >= 0; }
    bool load_bank_data(const void *data, size_t size) override
        { return Traits::open_bank_data(player_.get(), data, size) >= 0; }
    void generate(unsigned nframes, void *left, void *right, const Audio_Format &format) override
        { Traits::generate_format(player_.get(), 2 * nframes, (ADL_UInt8 *)left, (ADL_UInt8 *)right, &(typename Traits::audio_format &)format); }
    void rt_note_on(unsigned chan, unsigned note, unsigned vel) override
        { Traits::rt_note_on(player_.get(), chan, note, vel); }
    void rt_note_off(unsigned chan, unsigned note) override
        { Traits::rt_note_off(player_.get(), chan, note); }
    void rt_note_aftertouch(unsigned chan, unsigned note, unsigned val) override
        { Traits::rt_note_aftertouch(player_.get(), chan, note, val); }
    void rt_channel_aftertouch(unsigned chan, unsigned val) override
        { Traits::rt_channel_aftertouch(player_.get(), chan, val); }
    void rt_controller_change(unsigned chan, unsigned ctl, unsigned val) override
        { Traits::rt_controller_change(player_.get(), chan, ctl, val); }
    void rt_program_change(unsigned chan, unsigned pgm) override
        { Traits::rt_program_change(player_.get(), chan, pgm); }
    void rt_pitchbend(unsigned chan, unsigned value) override
        { Traits::rt_pitchbend(player_.get(), chan, value); }
    void rt_bank_change_msb(unsigned chan, unsigned value) override
        { Traits::rt_bank_change_msb(player_.get(), chan, value); }
    void rt_bank_change_lsb(unsigned chan, unsigned value) override
        { Traits::rt_bank_change_lsb(player_.get(), chan, value); }
    bool rt_system_exclusive(const uint8_t *data, unsigned size) override
        { return Traits::rt_system_exclusive(player_.get(), data, size) > 0; }
};
