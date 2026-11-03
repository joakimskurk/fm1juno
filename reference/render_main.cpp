#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include "control106/event_replay106.h"
}

namespace {

bool parse_type(const std::string &text, ScriptEventType106 &type)
{
    if (text == "VOICE_ON") type = SCRIPT106_VOICE_ON;
    else if (text == "VOICE_OFF") type = SCRIPT106_VOICE_OFF;
    else if (text == "SUSTAIN") type = SCRIPT106_SUSTAIN;
    else if (text == "DCO_COMMAND") type = SCRIPT106_DCO_COMMAND;
    else if (text == "CONTROL_STEP") type = SCRIPT106_CONTROL_STEP;
    else if (text == "PARAMETER") type = SCRIPT106_PARAMETER;
    else if (text == "PITCH_PROFILE") type = SCRIPT106_PITCH_PROFILE;
    else return false;
    return true;
}

bool parse_script(const std::string &path, std::vector<ScriptEvent106> &events)
{
    std::ifstream input(path);
    if (!input) return false;

    std::string line;
    bool first = true;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (first) {
            first = false;
            if (line == "version,sample,frac_q32,type,voice,value,aux") continue;
        }

        std::vector<std::string> fields;
        std::stringstream row(line);
        std::string field;
        while (std::getline(row, field, ',')) fields.push_back(field);
        if (fields.size() != 7u) return false;
        if (std::stoul(fields[0]) != EVENT_SCRIPT106_VERSION) return false;

        ScriptEventType106 type;
        if (!parse_type(fields[3], type)) return false;

        const uint64_t sample = std::stoull(fields[1]);
        const uint64_t frac = std::stoull(fields[2]);
        if (sample > UINT32_MAX || frac > UINT32_MAX) return false;

        ScriptEvent106 event{};
        event.time = (sample << FM1JUNO_TIME_FRAC_BITS) | frac;
        event.type = type;
        event.voice = static_cast<uint8_t>(std::stoul(fields[4]));
        event.value = static_cast<uint32_t>(std::stoul(fields[5]));
        event.aux = static_cast<uint32_t>(std::stoul(fields[6]));
        events.push_back(event);
    }
    return event_replay106_validate(events.data(), events.size());
}

void usage(const char *argv0)
{
    std::cerr << "usage: " << argv0
              << " <event-script.csv> [--outer-frames N] [--samples N]\n";
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    std::string script_path = argv[1];
    uint32_t outer_frames = 128u;
    uint32_t total_samples = 0u;

    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--outer-frames" && i + 1 < argc) {
            outer_frames = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--samples" && i + 1 < argc) {
            total_samples = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    std::vector<ScriptEvent106> events;
    try {
        if (!parse_script(script_path, events)) {
            std::cerr << "invalid event script: " << script_path << '\n';
            return EXIT_FAILURE;
        }
    } catch (const std::exception &e) {
        std::cerr << "parse error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }

    if (total_samples == 0u) {
        if (events.empty()) total_samples = 1u;
        else {
            const uint32_t last = fm1juno_time_sample_index(events.back().time);
            total_samples = last == UINT32_MAX ? UINT32_MAX : last + 1u;
        }
    }

    ReplayResult106 result{};
    if (!event_replay106_run(events.data(), events.size(), total_samples,
                             outer_frames, &result)) {
        std::cerr << "event replay failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "FM1JUNO deterministic render scaffold\n"
              << "script=" << script_path << '\n'
              << "events=" << events.size() << '\n'
              << "outer_frames=" << outer_frames << '\n'
              << "samples=" << total_samples << '\n'
              << "trace_signature=0x" << std::hex << std::setw(16)
              << std::setfill('0') << result.trace_signature << std::dec << '\n'
              << "trace_count=" << result.trace_count << '\n'
              << "trace_overflowed=" << (result.trace_overflowed ? 1 : 0) << '\n'
              << "pass_id=" << result.pass_id << '\n'
              << "abandoned_passes=" << result.abandoned_passes << '\n'
              << "note_gate_bits=" << static_cast<unsigned>(result.note_gate_bits) << '\n'
              << "voice_run_bits=" << static_cast<unsigned>(result.voice_run_bits) << '\n'
              << "audio_wav=not_available_until_reference_audio_blocks\n";
    return EXIT_SUCCESS;
}
